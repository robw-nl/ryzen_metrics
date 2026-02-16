#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "config.h"
#include "sensors.h"
#include "json_builder.h"
#include "power_model.h"
#include "discovery.h"

/* --- Debug Observability Macros --- */
#ifndef __OPTIMIZE__
#define DEBUG_TIME_START() \
struct timespec dbg_ts_start, dbg_ts_end; \
clock_gettime(CLOCK_MONOTONIC, &dbg_ts_start);

#define DEBUG_TIME_END(label) \
clock_gettime(CLOCK_MONOTONIC, &dbg_ts_end); \
double dbg_elapsed = (dbg_ts_end.tv_sec - dbg_ts_start.tv_sec) * 1000.0 + \
(dbg_ts_end.tv_nsec - dbg_ts_start.tv_nsec) / 1000000.0; \
if (dbg_elapsed > 50.0) { \
    printf("[Debug Output] %s WARNING: Overrun! Took %.3f ms\n", label, dbg_elapsed); \
} else { \
    printf("[Debug Output] %s took %.3f ms\n", label, dbg_elapsed); \
}
#else
#define DEBUG_TIME_START()
#define DEBUG_TIME_END(label)
#endif


void ensure_paths_exist(const char* data_path) {
    char dir_path[512];
    strncpy(dir_path, data_path, sizeof(dir_path) - 1);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        struct stat st = {0};
        if (stat(dir_path, &st) == -1) mkdir(dir_path, 0755);
    }
}

void update_panel_file(const char *final_path, const AppConfig *cfg, const SystemVitals *v, const DashboardPower *pwr, double registered_max) {
    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", final_path);
    FILE *fp = fopen(tmp_path, "w");
    if (!fp) return;

    if (json_build_panel(fp, cfg, v, pwr, registered_max)) {
        fflush(fp);
        fclose(fp);
        rename(tmp_path, final_path);
    } else {
        fclose(fp);
        unlink(tmp_path);
    }
}

void update_tooltip_file(const char *final_path, const AppConfig *cfg, const Accumulator *acc, const DashboardPower *pwr, double current_teff, int maturity_seconds, time_t amber_start) {
    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", final_path);
    FILE *fp = fopen(tmp_path, "w");
    if (!fp) return;

    if (json_build_tooltip(fp, cfg, acc, pwr, current_teff, maturity_seconds, amber_start)) {
        fflush(fp);
        fclose(fp);
        rename(tmp_path, final_path);
    } else {
        fclose(fp);
        unlink(tmp_path);
    }
}

void sleep_until_next_tick(struct timespec *target, int interval_ms) {
    target->tv_nsec += interval_ms * 1000000L;
    if (target->tv_nsec >= 1000000000L) { target->tv_nsec -= 1000000000L; target->tv_sec++; }
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, target, NULL) == EINTR) { }
}

int main() {
    verify_system_compatibility(); /* <-- The new Fail-Fast guard */

    char config_path[MAX_PATH];
    ssize_t len = readlink("/proc/self/exe", config_path, sizeof(config_path) - 1);
    if (len != -1) {
        config_path[len] = '\0';
        char *last = strrchr(config_path, '/');
        if (last) strcpy(last + 1, "metrics.conf");
    } else strcpy(config_path, "metrics.conf");

    AppConfig cfg = load_config(config_path);
    ensure_paths_exist(cfg.path_data);

    SensorContext sensors;
    init_sensors(&sensors, &cfg);
    Accumulator acc = load_from_ssd(&sensors);

    PowerModelState logic_state;
    init_power_model(&logic_state, &cfg);
    logic_state.maturity_seconds = acc.maturity_seconds;

    time_t last_sync = time(NULL);
    PeripheralState periph_cache = {0, 1, 0, 0.0};
    unsigned int tick = 0;
    struct timespec next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);

    SystemVitals v = {0};
    SystemVitals last_v = {0};
    DashboardPower last_pwr = {0};
    int force_update = 1;

    while (1) {
        DEBUG_TIME_START() // <-- START TIMING

        DashboardPower pwr;
        periph_cache.is_monitor_connected = check_monitor_connected(&sensors);
        periph_cache.is_audio_active = check_audio_active(&sensors);

        /* Persistent ALSA Call */
        if (periph_cache.is_audio_active) {
            if (tick % 5 == 0) periph_cache.volume_ratio = check_audio_volume(&sensors);
        } else { periph_cache.volume_ratio = 0.0; }

        v = read_fast_vitals(&sensors, &cfg);
        pwr = calculate_power(&logic_state, &cfg, &v, &periph_cache, &acc);
        acc.total_ws += pwr.wall_w;
        acc.total_sec += 1.0;

        if (force_update || abs(v.cpu_mhz - last_v.cpu_mhz) > 2 ||
            fabs(v.max_temp - last_v.max_temp) > 0.5 ||
            fabs(pwr.wall_w - last_pwr.wall_w) > 0.2 || (tick % 30 == 0)) {
            update_panel_file(cfg.path_panel, &cfg, &v, &pwr, acc.registered_max);
        last_v = v; last_pwr = pwr; force_update = 0;
            }

            if (tick % 60 == 0) update_tooltip_file(cfg.path_tooltip, &cfg, &acc, &pwr, v.current_teff, logic_state.maturity_seconds, logic_state.amber_start_time);

            time_t now_time = time(NULL);
        int force_sync = 0;

        if (logic_state.maturity_seconds >= cfg.maturity_required_sec) {
            // 1. Shift and insert new data
            for (int i = USAGE_DAYS - 1; i > 0; i--) acc.daily_avg_teff[i] = acc.daily_avg_teff[i-1];
            acc.daily_avg_teff[0] = v.current_teff;

            // 2. Increment valid points up to USAGE_DAYS (5)
            if (acc.data_points < USAGE_DAYS) acc.data_points++;

            // 3. ONLY update registered_max if we have a full window
            if (acc.data_points >= USAGE_DAYS) {
                double old_max = acc.registered_max;
                double sorted[USAGE_DAYS];
                memcpy(sorted, acc.daily_avg_teff, sizeof(sorted));

                // Simple Bubble Sort for Median
                for (int i = 0; i < USAGE_DAYS-1; i++) {
                    for (int j = 0; j < USAGE_DAYS-i-1; j++) {
                        if (sorted[j] > sorted[j+1]) {
                            double tmp = sorted[j];
                            sorted[j] = sorted[j+1]; sorted[j+1] = tmp;
                        }
                    }
                }
                acc.registered_max = sorted[USAGE_DAYS / 2];

                if (fabs(acc.registered_max - old_max) > 0.01) force_sync = 1;
            }

            // 4. Reset session maturity but keep the data_points
            logic_state.maturity_seconds = 0;
            acc.maturity_seconds = 0;
        }

        if (force_sync || difftime(now_time, last_sync) >= cfg.sync_sec) {
            acc.maturity_seconds = logic_state.maturity_seconds;
            save_to_ssd(&sensors, acc);
            last_sync = now_time;
        }

        DEBUG_TIME_END("Main Loop Exec") // <-- END TIMING

        sleep_until_next_tick(&next_tick, cfg.update_ms);
        tick++;
    }
    cleanup_sensors(&sensors);
    return 0;
}
