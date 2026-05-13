#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include "sensors.h"

int main() {
    Accumulator acc = {0};
    char data_path[4096];
    double kwh;
    int h, m;

    /* Dynamic path resolution replacing hardcoded profile */
    const char *home = getenv("HOME");
    if (!home) home = getpwuid(getuid())->pw_dir;
    snprintf(data_path, sizeof(data_path), "%s/.config/ryzen_metrics_daemon_release/stats.dat", home);

    printf("--- Ryzen Metrics: Manual Data Override ---\n");

    printf("--- Ryzen Metrics: Manual Data Override ---\n");

    printf("1. Stopping Daemons...\n");
    system("systemctl --user stop ryzen_metrics_daemon_release.service 2>/dev/null");

    printf("2. Enter Total Consumption (kWh): ");
    if (scanf("%lf", &kwh) != 1) return 1;

    printf("3. Enter Total Uptime - HOURS: ");
    if (scanf("%d", &h) != 1) return 1;

    printf("4. Enter Total Uptime - MINUTES (0-59): ");
    if (scanf("%d", &m) != 1) return 1;

    acc.total_ws = kwh * 3600000.0;
    acc.total_sec = (double)(h * 3600 + m * 60);

    acc.registered_max = 46.5;
    for(int i = 0; i < USAGE_DAYS; i++) acc.daily_avg_teff[i] = 46.5;

    FILE *f = fopen(data_path, "wb");
    if (f) {
        fwrite(&acc, sizeof(Accumulator), 1, f);
        fclose(f);
        printf("\nSUCCESS: Binary patched at %s\n", data_path);
        printf("Restored: %.3f kWh | %d Hours %d Minutes\n", kwh, h, m);
    } else {
        printf("\nERROR: Could not write to %s\n", data_path);
        return 1;
    }

    printf("\n--- IMPORTANT ---\n");
    printf("Verify 'start_date' in metrics.conf matches this data set.\n");
    printf("-----------------\n");

    printf("\nRestarting daemon...\n");
    system("systemctl --user start ryzen_metrics_daemon_release.service 2>/dev/null");
    return 0;
}
