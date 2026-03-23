#include "config.h"
#include "discovery.h"
#include "error_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#define SET_STR(var, val) strncpy(var, val, sizeof(var) - 1)
#define SET_VAL(var, val) var = val

#define PARSE_STR(name, target, size) \
if (strcmp(key, name) == 0) { \
    strncpy(target, val, size - 1); \
    continue; \
}
#define PARSE_DBL(name, target) \
if (strcmp(key, name) == 0) { \
    target = atof(val); \
    continue; \
}
#define PARSE_INT(name, target) \
if (strcmp(key, name) == 0) { \
    target = atoi(val); \
    continue; \
}

void set_defaults(AppConfig *c) {
    // UI Defaults
    SET_VAL(c->font_size, 32);
    SET_STR(c->font_family, "Hack Nerd Font");
    SET_STR(c->color_safe, "#a6e3a1");
    SET_STR(c->color_warn, "#fab387");
    SET_STR(c->color_crit, "#f38ba8");
    SET_STR(c->color_sep, "#008bd1");
    SET_STR(c->ssd_label, "SSD");

    // Hardware Defaults
    SET_STR(c->hw_gpu, "amdgpu");
    SET_STR(c->hw_cpu, "k10temp");
    SET_STR(c->hw_net, "r8169");
    SET_STR(c->hw_ram, "spd5118");
    SET_STR(c->hw_disk, "nvme"); // <--- NEW Default
    SET_STR(c->path_monitor, "/sys/class/drm/card1-HDMI-A-1/status");

    // Threshold Defaults
    SET_VAL(c->limit_mhz_warn, 3500); SET_VAL(c->limit_mhz_crit, 4500);
    SET_VAL(c->limit_temp_warn, 60.0); SET_VAL(c->limit_temp_crit, 80.0);
    SET_VAL(c->limit_ssd_warn, 50.0); SET_VAL(c->limit_ssd_crit, 70.0);
    SET_VAL(c->limit_ram_warn, 50.0); SET_VAL(c->limit_ram_crit, 70.0);
    SET_VAL(c->limit_net_warn, 50.0); SET_VAL(c->limit_net_crit, 70.0);
    SET_VAL(c->limit_wall_warn, 100.0); SET_VAL(c->limit_wall_crit, 150.0);
    SET_VAL(c->limit_soc_warn, 30.0); SET_VAL(c->limit_soc_crit, 45.0);
    SET_VAL(c->limit_soc_power_warn, 45.0);
    SET_VAL(c->limit_soc_power_crit, 60.0);
    SET_VAL(c->maturity_required_sec, 1800);
    SET_VAL(c->trend_break_delta, 5.0);

    // Thermal Model Defaults //
    SET_VAL(c->maturity_threshold_teff, 39.0);
    SET_VAL(c->maturity_required_sec, 1800);
    SET_VAL(c->trend_break_delta, 5.0);

    // Power Constants
    SET_VAL(c->psu_efficiency, 0.85);
    SET_VAL(c->mobo_overhead, 0.05);
    SET_VAL(c->pc_rest_base, 10.0);
    SET_VAL(c->periph_watt, 5.0);
    SET_VAL(c->mon_standby, 0.5);
    SET_VAL(c->mon_logic, 10.0);
    SET_VAL(c->mon_backlight_max, 30.0);
    SET_VAL(c->mon_dim_preset, 0.2);
    SET_VAL(c->mon_brightness_preset, 0.8);
    SET_VAL(c->speakers_active, 20.0);
    SET_VAL(c->speakers_standby, 10.0);
    SET_VAL(c->speakers_eco, 5.0);
    SET_VAL(c->euro_per_kwh, 0.25);

    // Timings
    SET_VAL(c->update_ms, 1000);
    SET_VAL(c->sync_sec, 60);
    SET_VAL(c->speakers_timeout_sec, 900);
    SET_VAL(c->mon_dim_timeout_sec, 120);
    SET_VAL(c->mon_off_timeout_sec, 300);

    // Paths
    SET_STR(c->path_panel, "/dev/shm/system_metrics_panel.json");
    SET_STR(c->path_tooltip, "/dev/shm/system_metrics_tooltip.txt");

    const char *home = getenv("HOME");
    if (!home) home = getpwuid(getuid())->pw_dir;
    snprintf(c->path_data, MAX_PATH, "%s/.config/system_metrics/stats.dat", home);

    SET_STR(c->start_date, "Unknown");
}

AppConfig load_config(const char *path) {
    AppConfig c;
    set_defaults(&c);      /* Initializes all fields with safe values [cite: 2026-01-26] */
    discover_hardware(&c); /* Probes paths, can be overridden by the file [cite: 2026-02-12] */

    FILE *f = fopen(path, "r");
    if (!f) {
        char err_msg[512];
        snprintf(err_msg, sizeof(err_msg), "Cannot find config file at %s", path);
        log_error(ERR_FATAL, "Config", err_msg);
        /* Safety: Stop if the source of truth is missing [cite: 39] */
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        val[strcspn(val, "\n")] = 0;

        if (strcmp(key, "start_date") == 0) {
            strncpy(c.start_date, val, sizeof(c.start_date) - 1);
            c.start_date[sizeof(c.start_date) - 1] = '\0';
        }

        /* --- New Ghost-Buster Calibration --- */
        PARSE_INT("ghost_freq_min", c.ghost_freq_min);
        PARSE_DBL("ghost_watt_max", c.ghost_watt_max);
        PARSE_DBL("ghost_floor", c.ghost_floor);

        /* --- Hardware Hooks --- */
        PARSE_STR("hw_gpu", c.hw_gpu, 32);
        PARSE_STR("hw_cpu", c.hw_cpu, 32);
        PARSE_STR("hw_net", c.hw_net, 32);
        PARSE_STR("hw_ram", c.hw_ram, 32);
        PARSE_STR("hw_disk", c.hw_disk, 32);
        PARSE_STR("path_monitor", c.path_monitor, 256);
        PARSE_STR("path_audio", c.path_audio, 256);

        /* --- UI & Paths --- */
        PARSE_INT("font_size", c.font_size);
        PARSE_STR("font_family", c.font_family, 64);
        PARSE_STR("color_safe", c.color_safe, 16);
        PARSE_STR("color_warn", c.color_warn, 16);
        PARSE_STR("color_crit", c.color_crit, 16);
        PARSE_STR("color_sep", c.color_sep, 16);
        PARSE_STR("ssd_label", c.ssd_label, 32);
        PARSE_STR("start_date", c.start_date, 32);
        PARSE_STR("path_panel", c.path_panel, MAX_PATH);
        PARSE_STR("path_tooltip", c.path_tooltip, MAX_PATH);
        PARSE_STR("path_data_file", c.path_data, MAX_PATH); /* Zone 2 persistence */

        /* --- Thresholds --- */
        PARSE_DBL("limit_mhz_warn", c.limit_mhz_warn);
        PARSE_DBL("limit_mhz_crit", c.limit_mhz_crit);
        PARSE_DBL("limit_temp_warn", c.limit_temp_warn);
        PARSE_DBL("limit_temp_crit", c.limit_temp_crit);
        PARSE_DBL("limit_ssd_warn", c.limit_ssd_warn);
        PARSE_DBL("limit_ssd_crit", c.limit_ssd_crit);
        PARSE_DBL("limit_ram_warn", c.limit_ram_warn);
        PARSE_DBL("limit_ram_crit", c.limit_ram_crit);
        PARSE_DBL("limit_net_warn", c.limit_net_warn);
        PARSE_DBL("limit_net_crit", c.limit_net_crit);
        PARSE_DBL("limit_wall_warn", c.limit_wall_warn);
        PARSE_DBL("limit_wall_crit", c.limit_wall_crit);
        PARSE_DBL("limit_soc_warn", c.limit_soc_warn);
        PARSE_DBL("limit_soc_crit", c.limit_soc_crit);
        PARSE_DBL("limit_soc_power_warn", c.limit_soc_power_warn);
        PARSE_DBL("limit_soc_power_crit", c.limit_soc_power_crit);

        /* --- Thermal Model (Maturity) --- */
        PARSE_DBL("maturity_threshold_teff", c.maturity_threshold_teff);
        PARSE_INT("maturity_required_sec", c.maturity_required_sec);
        PARSE_DBL("trend_break_delta", c.trend_break_delta);

        /* --- Power Constants --- */
        PARSE_DBL("psu_efficiency", c.psu_efficiency);
        PARSE_DBL("mobo_overhead", c.mobo_overhead);
        PARSE_DBL("pc_rest_base", c.pc_rest_base);
        PARSE_DBL("periph_watt", c.periph_watt);
        PARSE_DBL("mon_standby", c.mon_standby);
        PARSE_DBL("mon_logic", c.mon_logic);
        PARSE_DBL("mon_backlight_max", c.mon_backlight_max);
        PARSE_DBL("mon_dim_preset", c.mon_dim_preset);
        PARSE_DBL("mon_brightness_preset", c.mon_brightness_preset);
        PARSE_DBL("speakers_active", c.speakers_active);
        PARSE_DBL("speakers_standby", c.speakers_standby);
        PARSE_DBL("speakers_eco", c.speakers_eco);
        PARSE_DBL("euro_per_kwh", c.euro_per_kwh);

        /* --- Timings --- */
        PARSE_INT("update_ms", c.update_ms);
        PARSE_INT("sync_sec", c.sync_sec);
        PARSE_INT("speakers_timeout_sec", c.speakers_timeout_sec);
        PARSE_INT("mon_dim_timeout_sec", c.mon_dim_timeout_sec);
        PARSE_INT("mon_off_timeout_sec", c.mon_off_timeout_sec);
    }

    fclose(f);
    return c;
}
