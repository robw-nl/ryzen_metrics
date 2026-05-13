#ifndef POWER_MODEL_H
#define POWER_MODEL_H

#include "config.h"
#include "sensors.h"
#include "json_builder.h"

// 1. Define the type FIRST
typedef struct {
    int audio_cooldown;
    double last_uptime;
    int maturity_seconds;
    double smoothed_teff;
    time_t amber_start_time;
} PowerModelState;

// 2. Now the compiler knows what 'PowerModelState' is
void init_power_model(PowerModelState *state, const AppConfig *cfg);

// Note: Ensure 'const' is removed from SystemVitals *v here too
DashboardPower calculate_power(
    PowerModelState *state,
    const AppConfig *cfg,
    SystemVitals *v,
    const PeripheralState *p,
    const Accumulator *acc
);

#endif
