#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PATH 4096
#define USAGE_DAYS 5


typedef struct {
    double total_ws;
    double total_sec;
    double registered_max;
    double daily_avg_teff[USAGE_DAYS];
    int maturity_seconds;
    /* int usage_days_count; old value */
    int data_points; /* NEW: Tracks how many valid days are in the array */
} Accumulator;

typedef struct {
    int is_audio_active;
    int is_monitor_connected;
    int idle_sec;
    double volume_ratio;
} PeripheralState;

typedef struct {
    int font_size;
    char start_date[32];
    char font_family[64];
    char color_safe[16], color_warn[16], color_crit[16], color_sep[16];
    char ssd_label[32], hw_gpu[32], hw_cpu[32], hw_net[32], hw_ram[32], hw_disk[64];
    char path_audio[256], path_monitor[256], path_panel[MAX_PATH], path_tooltip[MAX_PATH], path_data[MAX_PATH];
    double limit_mhz_warn, limit_mhz_crit;
    double limit_temp_warn, limit_temp_crit;
    double limit_ssd_warn, limit_ssd_crit;
    double limit_ram_warn, limit_ram_crit;
    double limit_net_warn, limit_net_crit;
    double limit_wall_warn, limit_wall_crit;
    double limit_soc_warn, limit_soc_crit;
    double psu_efficiency, mobo_overhead, pc_rest_base, periph_watt;
    double mon_standby, mon_logic, mon_backlight_max;
    double speakers_active, speakers_standby, speakers_eco, euro_per_kwh;
    int update_ms, sync_sec, speakers_timeout_sec, mon_dim_timeout_sec, mon_off_timeout_sec;
    double mon_dim_preset, mon_brightness_preset;
    double maturity_threshold_teff;
    int maturity_required_sec;
    double trend_break_delta;
    /* Ghost-Buster Settings */
    int ghost_freq_min;
    double ghost_watt_max;
    double ghost_floor;
} AppConfig;

AppConfig load_config(const char *path);
#endif
