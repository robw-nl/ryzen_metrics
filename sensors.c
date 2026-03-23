#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <math.h>

#include "sensors.h"
#include "config.h"
#include "discovery.h"
#include "error_policy.h"


static FILE* fopen_nobuf(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) setvbuf(f, NULL, _IONBF, 0);
    return f;
}

static FILE* find_and_open_hwmon(const char *target_name, const char *file_suffix) {
    char found_dir[256] = {0};
    DIR *dr = opendir("/sys/class/hwmon");
    if (!dr) return NULL;
    struct dirent *en;
    while ((en = readdir(dr))) {
        char name_path[256], name_val[64];
        snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", en->d_name);
        FILE *f_name = fopen(name_path, "r");
        if (f_name) {
            if (fscanf(f_name, "%63s", name_val) == 1) {
                if (strstr(name_val, target_name) != NULL) {
                    snprintf(found_dir, sizeof(found_dir), "/sys/class/hwmon/%s", en->d_name);
                    fclose(f_name);
                    break;
                }
            }
            fclose(f_name);
        }
    }
    closedir(dr);
    if (found_dir[0] != '\0') {
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s/%s", found_dir, file_suffix);
        return fopen_nobuf(full_path);
    }
    return NULL;
}

void init_sensors(SensorContext *ctx, const AppConfig *cfg) {
    memset(ctx, 0, sizeof(SensorContext));
    for (int i = 0; i < MAX_CPU_THREADS; i++) ctx->fd_cpu_freq[i] = -1;
    if (cfg->path_data[0]) {
        strncpy(ctx->path_data_file, cfg->path_data, sizeof(ctx->path_data_file) - 1);
    }

    // Initialize safe fallbacks
    ctx->last_valid_soc_w = 7.0;
    ctx->last_valid_cpu_mhz = 1000;
    ctx->last_valid_max_temp = 40.0;
    ctx->last_valid_ssd_temp = 40.0;
    ctx->last_valid_ram_temp = 40.0;
    ctx->last_valid_net_temp = 40.0;

    /* Topology Discovery: Determine thread vs core mapping [cite: 158] */
    ctx->detected_thread_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (ctx->detected_thread_count > MAX_CPU_THREADS) ctx->detected_thread_count = MAX_CPU_THREADS;

    for (int i = 0; i < ctx->detected_thread_count; i++) {
        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        ctx->fd_cpu_freq[i] = open(path, O_RDONLY);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", i);
        FILE *f_top = fopen(path, "r");
        if (f_top) {
            int first_sibling;
            if (fscanf(f_top, "%d", &first_sibling) == 1) {
                if (first_sibling == i) {
                    ctx->is_physical_core[i] = 1;
                    ctx->physical_core_count++;
                }
            }
            fclose(f_top);
        }
    }

    /* --- NEW: Topology Partial-Failure Fallback --- */
    if (ctx->physical_core_count == 0 && ctx->detected_thread_count > 0) {
        log_error(ERR_RECOVERABLE, "Topology", "Failed to map physical cores. Falling back to Thread 0.");
        ctx->is_physical_core[0] = 1;
        ctx->physical_core_count = 1;

        // Ensure the FD is open for our fallback
        if (ctx->fd_cpu_freq[0] == -1) {
            char path[256];
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
            ctx->fd_cpu_freq[0] = open(path, O_RDONLY);
        }
    }

    /* Standard Hardware Monitor Init */
    ctx->fp_gpu_power = find_and_open_hwmon(cfg->hw_gpu, "power1_input");
    if (!ctx->fp_gpu_power) ctx->fp_gpu_power = find_and_open_hwmon(cfg->hw_gpu, "power1_average");
    if (!ctx->fp_gpu_power) log_error(ERR_RECOVERABLE, "Sensors", "GPU power sensor missing. Using 7.0W fallback.");

    ctx->fp_cpu_temp = find_and_open_hwmon(cfg->hw_cpu, "temp1_input");
    if (!ctx->fp_cpu_temp) log_error(ERR_RECOVERABLE, "Sensors", "CPU temp sensor missing. Using 40C fallback.");

    ctx->fp_ssd_temp = find_and_open_hwmon(cfg->hw_disk, "temp1_input");
    if (!ctx->fp_ssd_temp) log_error(ERR_RECOVERABLE, "Sensors", "SSD temp sensor missing. Using 40C fallback.");

    ctx->fp_ram_temp = find_and_open_hwmon(cfg->hw_ram, "temp1_input");
    if (!ctx->fp_ram_temp) ctx->fp_ram_temp = find_and_open_hwmon("jc42", "temp1_input");
    if (!ctx->fp_ram_temp) log_error(ERR_RECOVERABLE, "Sensors", "RAM temp sensor missing. Using 40C fallback.");

    ctx->fp_net_temp = find_and_open_hwmon(cfg->hw_net, "temp1_input");
    if (!ctx->fp_net_temp) log_error(ERR_RECOVERABLE, "Sensors", "Net temp sensor missing. Using 40C fallback.");

    char disc_path[256] = {0};
    scan_for_monitor(disc_path, sizeof(disc_path));
    if (disc_path[0] != '\0') ctx->fp_drm_status = fopen_nobuf(disc_path);
    else if (cfg->path_monitor[0]) ctx->fp_drm_status = fopen_nobuf(cfg->path_monitor);
    if (cfg->path_audio[0]) ctx->fp_audio_status = fopen_nobuf(cfg->path_audio);

    /* --- NEW: Persistent ALSA Handle Initialization --- */
    snd_mixer_t *handle;
    if (snd_mixer_open(&handle, 0) >= 0) {
        if (snd_mixer_attach(handle, "default") >= 0 &&
            snd_mixer_selem_register(handle, NULL, NULL) >= 0 &&
            snd_mixer_load(handle) >= 0) {
            ctx->alsa_handle = (void*)handle;
            } else {
                snd_mixer_close(handle);
                ctx->alsa_handle = NULL;
            }
    }
}

SystemVitals read_fast_vitals(SensorContext *ctx, const AppConfig *cfg) {
    SystemVitals v = {0};
    double val_buf = 0;
    char freq_buf[32];

    /* 1. Frequency Polling (Topology Aware pread loop)  */
    long long mhz_sum = 0;
    int valid_cores = 0;
    for (int i = 0; i < ctx->detected_thread_count; i++) {
        if (ctx->is_physical_core[i] && ctx->fd_cpu_freq[i] != -1) {
            // Guard: Explicitly check that pread successfully grabbed data
            ssize_t bytes = pread(ctx->fd_cpu_freq[i], freq_buf, sizeof(freq_buf) - 1, 0);
            if (bytes > 0) {
                freq_buf[bytes] = '\0';
                mhz_sum += atoll(freq_buf) / 1000;
                valid_cores++;
            }
        }
    }

    if (valid_cores > 0) {
        v.cpu_mhz = (int)(mhz_sum / valid_cores);
        ctx->last_valid_cpu_mhz = v.cpu_mhz;
    } else {
        v.cpu_mhz = ctx->last_valid_cpu_mhz;
    }

    /* 2. SoC Power with Ghost-Buster Shield v3 */
    if (ctx->fp_gpu_power) {
        rewind(ctx->fp_gpu_power);
        if (fscanf(ctx->fp_gpu_power, "%lf", &val_buf) == 1) {
            double current_reading = val_buf / 1000000.0;
            if (v.cpu_mhz > cfg->ghost_freq_min) {
                /* Fix: Removed the (ctx->last_valid_soc_w * 0.5) trap to prevent peak latching */
                if (current_reading < cfg->ghost_watt_max) {
                    v.soc_w = ctx->last_valid_soc_w;
                    #ifdef DEBUG_OUTPUT
                    /* Diagnostic output routed to debug build only */
                    fprintf(stderr, "[DEBUG] Ghost filtered: %fW clamped to %fW\n", current_reading, ctx->last_valid_soc_w);
                    #endif
                } else {
                    v.soc_w = current_reading;
                    ctx->last_valid_soc_w = current_reading;
                }
            } else {
                v.soc_w = current_reading;
                ctx->last_valid_soc_w = (current_reading > cfg->ghost_floor) ? current_reading : ctx->last_valid_soc_w;
            }
        } else {
            v.soc_w = ctx->last_valid_soc_w;
        }
    } else {
        v.soc_w = ctx->last_valid_soc_w;
    }

    /* 3. Thermal Sensors (Guarded) */
    if (ctx->fp_cpu_temp) {
        rewind(ctx->fp_cpu_temp);
        if (fscanf(ctx->fp_cpu_temp, "%lf", &val_buf) == 1) {
            v.max_temp = val_buf / 1000.0;
            ctx->last_valid_max_temp = v.max_temp;
        } else { v.max_temp = ctx->last_valid_max_temp; }
    } else { v.max_temp = ctx->last_valid_max_temp; }

    if (ctx->fp_ssd_temp) {
        rewind(ctx->fp_ssd_temp);
        if (fscanf(ctx->fp_ssd_temp, "%lf", &val_buf) == 1) {
            v.ssd_temp = val_buf / 1000.0;
            ctx->last_valid_ssd_temp = v.ssd_temp;
        } else { v.ssd_temp = ctx->last_valid_ssd_temp; }
    } else { v.ssd_temp = ctx->last_valid_ssd_temp; }

    if (ctx->fp_ram_temp) {
        rewind(ctx->fp_ram_temp);
        if (fscanf(ctx->fp_ram_temp, "%lf", &val_buf) == 1) {
            v.ram_temp = val_buf / 1000.0;
            ctx->last_valid_ram_temp = v.ram_temp;
        } else { v.ram_temp = ctx->last_valid_ram_temp; }
    } else { v.ram_temp = ctx->last_valid_ram_temp; }

    if (ctx->fp_net_temp) {
        rewind(ctx->fp_net_temp);
        if (fscanf(ctx->fp_net_temp, "%lf", &val_buf) == 1) {
            v.net_temp = val_buf / 1000.0;
            ctx->last_valid_net_temp = v.net_temp;
        } else { v.net_temp = ctx->last_valid_net_temp; }
    } else { v.net_temp = ctx->last_valid_net_temp; }

    return v;
}

int check_monitor_connected(SensorContext *ctx) {
    if (!ctx->fp_drm_status) return 0;
    char status[64]; rewind(ctx->fp_drm_status);
    if (fscanf(ctx->fp_drm_status, "%63s", status) == 1) return (strcmp(status, "connected") == 0);
    return 0;
}

int check_audio_active(SensorContext *ctx) {
    if (!ctx->fp_audio_status) return 0;

    char line[128];
    int is_running = 0;
    long current_hw_ptr = -1;

    rewind(ctx->fp_audio_status);

    while (fgets(line, sizeof(line), ctx->fp_audio_status)) {
        if (strstr(line, "state: RUNNING")) {
            is_running = 1;
        } else if (strncmp(line, "hw_ptr", 6) == 0) {
            sscanf(line, "hw_ptr : %ld", &current_hw_ptr);
        }
    }

    if (is_running && current_hw_ptr != -1) {
        if (current_hw_ptr != ctx->last_hw_ptr) {
            ctx->last_hw_ptr = current_hw_ptr;
            return 1;
        }

        #ifdef DEBUG_OUTPUT
        /* Output excluded from release build automatically */
        fprintf(stderr, "[DEBUG] Audio RUNNING but hw_ptr stalled at %ld. Forcing standby.\n", current_hw_ptr);
        #endif

        return 0;
    }
//
    ctx->last_hw_ptr = -1;
    return 0;
}

long get_uptime() { struct sysinfo s_info; return (sysinfo(&s_info) == 0) ? s_info.uptime : 0; }

void save_to_ssd(SensorContext *ctx, Accumulator acc) {
    if (!ctx->path_data_file[0]) return;
    FILE *f = fopen(ctx->path_data_file, "wb");
    if (f) { fwrite(&acc, sizeof(Accumulator), 1, f); fclose(f); }
}

Accumulator load_from_ssd(SensorContext *ctx) {
    Accumulator acc = {0};
    FILE *f = fopen(ctx->path_data_file, "rb");
    if (f) { fread(&acc, sizeof(Accumulator), 1, f); fclose(f); }
    if (acc.registered_max < 1.0) {
        acc.registered_max = 45.0;
        for(int i=0; i<USAGE_DAYS; i++) acc.daily_avg_teff[i] = 45.0;
    }
    return acc;
}

/* --- Optimized Persistent check_audio_volume --- */
double check_audio_volume(SensorContext *ctx) {
    if (!ctx->alsa_handle) return 0.5;

    long min, max, volume;
    snd_mixer_t *handle = (snd_mixer_t *)ctx->alsa_handle;
    /* RE-SYNC: This tells ALSA to update the internal state from the system [cite: 2026-02-12] */
    snd_mixer_handle_events(handle);
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, "Master");
    elem = snd_mixer_find_selem(handle, sid);

    if (!elem) return 0.5;

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &volume) < 0) {
        return 0.5;
    }

    return (max - min == 0) ? 0.0 : (double)(volume - min) / (double)(max - min);
}

void cleanup_sensors(SensorContext *ctx) {
    if (ctx->fp_gpu_power) fclose(ctx->fp_gpu_power); if (ctx->fp_cpu_temp) fclose(ctx->fp_cpu_temp);
    if (ctx->fp_ssd_temp) fclose(ctx->fp_ssd_temp); if (ctx->fp_ram_temp) fclose(ctx->fp_ram_temp);
    if (ctx->fp_net_temp) fclose(ctx->fp_net_temp); if (ctx->fp_drm_status) fclose(ctx->fp_drm_status);
    if (ctx->fp_audio_status) fclose(ctx->fp_audio_status);

    /* Clean up CPU pread descriptors [cite: 136] */
    for (int i = 0; i < MAX_CPU_THREADS; i++) if (ctx->fd_cpu_freq[i] != -1) close(ctx->fd_cpu_freq[i]);

    /* Clean up Persistent ALSA handle */
    if (ctx->alsa_handle) {
        snd_mixer_close((snd_mixer_t *)ctx->alsa_handle);
        ctx->alsa_handle = NULL;
    }
}
