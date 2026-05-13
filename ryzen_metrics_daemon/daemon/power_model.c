#include "power_model.h"
#include <unistd.h> /* Required for sysconf and _SC_NPROCESSORS_ONLN */
#include <time.h>
#include <math.h>

/* --- Safety Macros --- */
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define GUARD_FLOAT(val, min, max, fallback) (isnan(val) ? (fallback) : CLAMP(val, min, max))

static double get_dynamic_efficiency(double internal_w, double peak_eff) {
    /* Approximates typical silicon brick curves: heavy penalty at <15W, sweet spot >30W */
    if (internal_w < 15.0) return peak_eff * 0.90;
    if (internal_w < 30.0) return peak_eff * 0.97;
    return peak_eff;
}

void init_power_model(PowerModelState *state, const AppConfig *cfg) {
    state->audio_cooldown = cfg->speakers_timeout_sec;
    state->last_uptime = 0.0;
    state->maturity_seconds = 0;
    state->smoothed_teff = 0.0;
    state->amber_start_time = 0;
}

DashboardPower calculate_power(
    PowerModelState *state,
    const AppConfig *cfg,
    SystemVitals *v,
    const PeripheralState *p,
    const Accumulator *acc
) {
    /* 0. Sanitize Input Data (Guard against sysfs garbage or NaNs) */
    v->cpu_mhz  = (int)GUARD_FLOAT((double)v->cpu_mhz, 0.0, 10000.0, 1000.0);
    v->soc_w    = GUARD_FLOAT(v->soc_w, 0.0, 250.0, cfg->pc_rest_base);
    v->max_temp = GUARD_FLOAT(v->max_temp, 0.0, 110.0, 40.0);
    v->ssd_temp = GUARD_FLOAT(v->ssd_temp, 0.0, 110.0, 40.0);
    v->ram_temp = GUARD_FLOAT(v->ram_temp, 0.0, 110.0, 40.0);
    v->net_temp = GUARD_FLOAT(v->net_temp, 0.0, 110.0, 40.0);

    /* 1. Uptime Check (Detect Reboots or Time Skips) */
    double uptime = get_uptime();
    if (uptime < state->last_uptime || state->last_uptime == 0) {
        state->audio_cooldown = cfg->speakers_timeout_sec;
        state->maturity_seconds = 0;
    }
    state->last_uptime = uptime;

    /* 2a. Calculated Weighted T_eff (Net: 40%, RAM: 40%, SoC: 20%) */
    double raw_teff = (v->net_temp * 0.4) + (v->ram_temp * 0.4) + (v->max_temp * 0.2);

    /* 2b. Apply Smoothing (The Digital Sink) */
    if (state->smoothed_teff < 1.0) {
        state->smoothed_teff = raw_teff;
    } else {
        double alpha = 0.001;
        state->smoothed_teff += alpha * (raw_teff - state->smoothed_teff);
    }

    /* 2c. Save the result back to SystemVitals for the UI */
    v->current_teff = state->smoothed_teff;

    /* 2d. Amber Hold Timer Logic */
    double amber_limit = acc->registered_max + cfg->trend_break_delta;
    if (v->current_teff > amber_limit) {
        if (state->amber_start_time == 0) {
            state->amber_start_time = time(NULL);
        }
    } else {
        state->amber_start_time = 0;
    }

    /* 3. Maturity Logic (Self-Calibration Gate) */
    if (v->current_teff > cfg->maturity_threshold_teff) {
        state->maturity_seconds++;
    }

    /* 4. Dynamic Audio Logic */
    double audio_w = 0.0;
    if (p->is_audio_active) {
        double volume_sq = p->volume_ratio * p->volume_ratio;
        double range = cfg->speakers_active - cfg->speakers_standby;
        audio_w = cfg->speakers_standby + (volume_sq * range);
        state->audio_cooldown = cfg->speakers_timeout_sec;
    } else if (state->audio_cooldown > 0) {
        audio_w = cfg->speakers_standby;
        state->audio_cooldown--;
    } else {
        audio_w = cfg->speakers_eco;
    }

    /* 5. Monitor Logic */
    double mon_w = 0.0;
    if (p->is_monitor_connected == 0) {
        mon_w = 0.0;
    } else if (p->idle_sec > cfg->mon_off_timeout_sec) {
        mon_w = cfg->mon_standby;
    } else if (p->idle_sec > cfg->mon_dim_timeout_sec) {
        mon_w = cfg->mon_logic + (cfg->mon_dim_preset * cfg->mon_backlight_max);
    } else {
        double br = (p->brightness_ratio > 0) ? p->brightness_ratio : cfg->mon_brightness_preset;
        mon_w = cfg->mon_logic + (br * cfg->mon_backlight_max);
    }

    /* 6. Wattage Summation */
    DashboardPower pwr;
    pwr.soc_w = v->soc_w;
    pwr.system_w = cfg->pc_rest_base + (pwr.soc_w * cfg->mobo_overhead);
    double internal_w = pwr.soc_w + pwr.system_w;


    /* Autonomous PSU Tiering based on Host Topology */
    int host_threads = sysconf(_SC_NPROCESSORS_ONLN);
    double autoscale_eff = cfg->psu_efficiency;

    if (host_threads <= 16) {
        if (internal_w < 15.0) autoscale_eff *= 0.85; /* External brick penalty */
    } else {
        if (internal_w < 40.0) autoscale_eff *= 0.80; /* Standard ATX penalty */
    }

    double current_eff = get_dynamic_efficiency(internal_w, autoscale_eff);

    pwr.ext_w = mon_w + cfg->periph_watt + audio_w;
    pwr.wall_w = (internal_w / current_eff) + pwr.ext_w;

    /* 7. Cost Calculation */
    pwr.cost = (acc->total_ws / 3600000.0) * cfg->euro_per_kwh;

    return pwr;
}
