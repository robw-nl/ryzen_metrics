#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "config.h"
#include <stddef.h> // For size_t

void discover_hardware(AppConfig *cfg);
// ADD THIS LINE:
void scan_for_monitor(char *out_path, size_t size);
void verify_system_compatibility(void);

#endif
