#include "power_model.h"
#include <math.h>

void init_power_model(PowerModelState *state, const AppConfig *cfg) {
    state->audio_cooldown = cfg->speakers_timeout_sec;
    state->last_uptime = 0.0;
    state->maturity_seconds = 0; // Initialize the new maturity counter
}

DashboardPower calculate_power(
    PowerModelState *state,
    const AppConfig *cfg,
    SystemVitals *v,        // NO const
    const PeripheralState *p,
    const Accumulator *acc
) {
    // 1. Uptime Check (Detect Reboots or Time Skips)
    double uptime = get_uptime();
    if (uptime < state->last_uptime || state->last_uptime == 0) {
        state->audio_cooldown = cfg->speakers_timeout_sec;
        state->maturity_seconds = 0;
    }
    state->last_uptime = uptime;

    // 2a. Calculated Weighted T_eff (Net: 40%, RAM: 40%, SoC: 20%)
    // This defines the "effective" temperature of your passive chassis
    double raw_teff = (v->net_temp * 0.4) + (v->ram_temp * 0.4) + (v->max_temp * 0.2);

    // 2b. Apply Smoothing (The Digital Sink)
    // If first run, jump straight to raw, otherwise move slowly
    if (state->smoothed_teff < 1.0) {
        state->smoothed_teff = raw_teff;
    } else {
        double alpha = 0.001; // Adjust this to make the 'sink' slower or faster
        state->smoothed_teff += alpha * (raw_teff - state->smoothed_teff);
    }
    // 2c. Save the result back to SystemVitals for the UI
    v->current_teff = state->smoothed_teff;

    // 2d. Amber Hold Timer Logic
    // Define the limit based on learned baseline + config delta
    double amber_limit = acc->registered_max + cfg->trend_break_delta; // [cite: 7, 56]

    if (v->current_teff > amber_limit) {
        // If we just crossed the limit, mark the start time
        if (state->amber_start_time == 0) {
            state->amber_start_time = time(NULL); //
        }
    } else {
        // Reset when we drop back to Gray zone
        state->amber_start_time = 0;
    }

    // 3. Maturity Logic (Self-Calibration Gate)
    // Counts seconds above the 39C threshold toward a "Usage Day"
    if (v->current_teff > cfg->maturity_threshold_teff) {
        state->maturity_seconds++;
    }

    // 4. Dynamic Audio Logic [cite: 2026-02-12]
    double audio_w = 0.0;
    if (p->is_audio_active) {
        /* Use a squared ratio to model logarithmic power draw [cite: 2026-02-12] */
        double volume_sq = p->volume_ratio * p->volume_ratio;
        double range = cfg->speakers_active - cfg->speakers_standby;

        audio_w = cfg->speakers_standby + (volume_sq * range);
        state->audio_cooldown = cfg->speakers_timeout_sec;
    } else if (state->audio_cooldown > 0) {
        /* Keep in standby until the timeout expires [cite: 2026-02-12] */
        audio_w = cfg->speakers_standby;
        state->audio_cooldown--;
    } else {
        /* Drop to eco/off state [cite: 2026-02-12] */
        audio_w = cfg->speakers_eco;
    }

    // 5. Monitor Logic
    double mon_w = 0.0;
    if (p->is_monitor_connected == 0) {
        mon_w = 0.0;
    } else if (p->idle_sec > cfg->mon_off_timeout_sec) {
        mon_w = cfg->mon_standby;
    } else if (p->idle_sec > cfg->mon_dim_timeout_sec) {
        mon_w = cfg->mon_logic + (cfg->mon_dim_preset * cfg->mon_backlight_max);
    } else {
        mon_w = cfg->mon_logic + (cfg->mon_brightness_preset * cfg->mon_backlight_max);
    }

    // 6. Wattage Summation
    DashboardPower pwr;
    pwr.soc_w = v->soc_w;
    pwr.system_w = cfg->pc_rest_base + (pwr.soc_w * cfg->mobo_overhead);
    pwr.ext_w = mon_w + cfg->periph_watt + audio_w;
    pwr.wall_w = ((pwr.soc_w + pwr.system_w) / cfg->psu_efficiency) + pwr.ext_w;

    // 7. Cost Calculation
    pwr.cost = (acc->total_ws / 3600000.0) * cfg->euro_per_kwh;

    return pwr;
}
