#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <stdio.h>
#include <time.h>
#include "config.h"
#include "sensors.h"

typedef struct {
    double soc_w;
    double system_w;
    double ext_w;
    double wall_w;
    double cost;
} DashboardPower;


// Parameter 'registered_max' comes from acc.registered_max in daemon.c
int json_build_panel(FILE *fp, const AppConfig *cfg, const SystemVitals *v, const DashboardPower *pwr, double registered_max);// Parameter 'current_teff' comes from v.current_teff in daemon.c
int json_build_tooltip(FILE *fp, const AppConfig *cfg, const Accumulator *acc, const DashboardPower *pwr, double current_teff, int maturity_seconds, time_t amber_start);

#endif
