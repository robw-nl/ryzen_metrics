#ifndef SENSORS_H
#define SENSORS_H

#include <stdio.h>
#include "config.h"

#define MAX_CPU_THREADS 128

typedef struct {
    int cpu_mhz;
    double soc_w;
    double max_temp;
    double ssd_temp;
    double ram_temp;
    double net_temp;
    double current_teff;
    double cpu_crit; /* Kernel-defined SoC limit */
    double ssd_crit; /* Kernel-defined SSD limit */
} SystemVitals;

typedef struct {
    FILE *fp_gpu_power;
    FILE *fp_cpu_temp;
    FILE *fp_ssd_temp;
    FILE *fp_ram_temp;
    FILE *fp_net_temp;
    int fd_cpu_freq[MAX_CPU_THREADS];
    int is_physical_core[MAX_CPU_THREADS];
    int detected_thread_count;
    int physical_core_count;
    FILE *fp_drm_status;
    FILE *fp_audio_status;
    void *alsa_handle;
    char path_data_file[4096];
    double last_valid_soc_w;

    FILE *fp_ram;
    FILE *fp_soc_w;
    FILE *fp_cpu_crit; /* Pointer to SoC temp1_crit */
    FILE *fp_ssd_crit; /* Pointer to SSD temp1_crit */
    // --- NEW: Sensor read guards fallback ---
    int last_valid_cpu_mhz;
    double last_valid_max_temp;
    double last_valid_ssd_temp;
    double last_valid_ram_temp;
    double last_valid_net_temp;
    long last_hw_ptr; /* Tracks ALSA ring buffer advancement */
} SensorContext;

void init_sensors(SensorContext *ctx, const AppConfig *cfg);
void cleanup_sensors(SensorContext *ctx);
SystemVitals read_fast_vitals(SensorContext *ctx, const AppConfig *cfg);
int check_monitor_connected(SensorContext *ctx);
int check_audio_active(SensorContext *ctx);
double read_ddc_brightness();
double check_audio_volume(SensorContext *ctx);
long get_uptime();
void save_to_ssd(SensorContext *ctx, Accumulator acc);
Accumulator load_from_ssd(SensorContext *ctx);

#endif
