#include "discovery.h"
#include "error_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

// --- Helpers ---
static int has_file(const char *dir, const char *filename) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    return (access(path, F_OK) == 0);
}

/* discovery.c - Universal Helper: Finds the hardware name of the OS drive  */
static void get_os_drive_prefix(char *out_prefix, size_t size) {
    FILE *f = fopen("/proc/mounts", "r");
    char line[512], device[256], mount_point[256];

    out_prefix[0] = '\0';
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255s %255s", device, mount_point) == 2) {
            if (strcmp(mount_point, "/") == 0) {
                // Strip path: /dev/nvme0n1p3 -> nvme0n1p3
                char *name = strrchr(device, '/');
                name = name ? name + 1 : device;
                strncpy(out_prefix, name, size - 1);

                // Truncate at partition/sub-device: nvme0n1p3 -> nvme0 | sda1 -> sda
                char *p = out_prefix;
                while (*p && !(*p >= '0' && *p <= '9')) p++; // Find first number
                if (*p && *(p+1) == 'n') { // Case: nvme0n1
                    *(p+1) = '\0';
                } else if (*p) { // Case: sda1
                    *p = '\0';
                }
                break;
            }
        }
    }
    fclose(f);
}

static void read_one_line(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return; }
    if (fgets(buf, size, f)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    } else { buf[0] = '\0'; }
    fclose(f);
}

// Internal helper to read a single line from a path
static void read_line(const char *path, char *out, size_t size) {
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(out, size, f)) {
            out[strcspn(out, "\n")] = 0; // Strip newline
        }
        fclose(f);
    } else {
        out[0] = '\0';
    }
}

void verify_system_compatibility(void) {
    /* 1. Desktop Environment Check */
    const char *xdg_desktop = getenv("XDG_CURRENT_DESKTOP");
    const char *kde_session = getenv("KDE_FULL_SESSION");

    int is_kde = 0;
    if (kde_session && strcmp(kde_session, "true") == 0) is_kde = 1;
    if (xdg_desktop && strstr(xdg_desktop, "KDE")) is_kde = 1;

    if (!is_kde) {
        log_error(ERR_RECOVERABLE, "Compat", "Desktop Environment check failed. Continuing...");
    }

    /* 2. CPU Architecture Check */
    FILE *f_cpu = fopen("/proc/cpuinfo", "r");
    if (f_cpu) {
        char line[256];
        int valid_cpu = 0;
        while (fgets(line, sizeof(line), f_cpu)) {
            if (strstr(line, "vendor_id")) {
                if (strstr(line, "AuthenticAMD") || strstr(line, "GenuineIntel")) {
                    valid_cpu = 1;
                }
                break;
            }
        }
        fclose(f_cpu);

        if (!valid_cpu) {
            log_error(ERR_FATAL, "Compat", "Unsupported Architecture. AMD or Intel required");
        }
    } else {
        log_error(ERR_FATAL, "Compat", "Cannot read /proc/cpuinfo to verify architecture");
    }
}

void scan_for_monitor(char *out_path, size_t size) {
    DIR *dr = opendir("/sys/class/drm");
    if (!dr) return;

    struct dirent *en;
    while ((en = readdir(dr))) {
        // We look for connector directories like "card1-HDMI-A-1"
        if (en->d_name[0] == '.' || strchr(en->d_name, '-') == NULL) continue;

        char base[512], status[32], enabled[32];
        snprintf(base, sizeof(base), "/sys/class/drm/%s", en->d_name);

        // Check both status AND enabled to find the active display
        char path_s[512], path_e[512];
        snprintf(path_s, sizeof(path_s), "%s/status", base);
        snprintf(path_e, sizeof(path_e), "%s/enabled", base);

        read_line(path_s, status, sizeof(status));
        read_line(path_e, enabled, sizeof(enabled));

        // IMPROVEMENT: Prioritize 'connected'. Use 'enabled' as a secondary hint.
        if (strcmp(status, "connected") == 0) {
            strncpy(out_path, path_s, size - 1);
            out_path[size - 1] = '\0';
            // If it's both connected AND enabled, we definitely found the primary.
            if (strcmp(enabled, "enabled") == 0) break;
            // Otherwise, keep looking for an 'enabled' one, but keep this 'connected' one as fallback.
        }
    }
    closedir(dr);
}

// --- Audio Discovery ---
static void scan_for_audio(char *out_path, size_t size) {
    DIR *dr = opendir("/proc/asound");
    if (!dr) {
        log_error(ERR_IGNORABLE, "Discovery", "No /proc/asound found. Audio monitoring disabled.");
        return;
    }

    struct dirent *en;
    int found_priority = 0; // 0=None, 1=HDMI/PCH, 2=USB(Preferred)

    while ((en = readdir(dr))) {
        // Look for "cardX" folders
        if (strncmp(en->d_name, "card", 4) != 0) continue;

        // Construct paths
        char id_path[256];
        snprintf(id_path, sizeof(id_path), "/proc/asound/%s/id", en->d_name);

        // Check what this card is
        char id[64];
        read_one_line(id_path, id, sizeof(id));

        // Determine Priority
        // Klipsch/USB audio usually has "USB" or specific brand in ID
        int current_prio = 1;
        if (strstr(id, "USB") || strstr(id, "Fives") || strstr(id, "Klipsch")) {
            current_prio = 2;
        }

        if (current_prio > found_priority) {
            // We assume pcm0p/sub0/status is the main stream.
            // Some cards use pcm1p, but pcm0p is standard for main output.
            char candidate[256];
            snprintf(candidate, sizeof(candidate), "/proc/asound/%s/pcm0p/sub0/status", en->d_name);
            if (access(candidate, F_OK) == 0) {
                strncpy(out_path, candidate, size - 1);
                out_path[size - 1] = '\0';
                found_priority = current_prio;
            }
        }
    }
    closedir(dr);
}

/* Universal Ryzen Hardware Detection */
void discover_hardware(AppConfig *cfg) {
    char os_prefix[32];
    get_os_drive_prefix(os_prefix, sizeof(os_prefix));

    /* Universal Keyword Map */
    SensorMap map[] = {
        {"k10temp",  cfg->hw_cpu,  "SoC"},
        {"zenpower", cfg->hw_cpu,  "SoC"},
        {"amdgpu",   cfg->hw_gpu,  "GPU"},
        {"radeon",   cfg->hw_gpu,  "GPU"},
        {"nvme",     cfg->hw_disk, "SSD"},
        {"spd5118",  cfg->hw_ram,  "RAM"},
        {"jc42",     cfg->hw_ram,  "RAM"},
        {"r8169",    cfg->hw_net,  "Net"},
        {"igc",      cfg->hw_net,  "Net"},
        {"e1000",    cfg->hw_net,  "Net"},
        {"rtl",      cfg->hw_net,  "Net"}
    };

    DIR *dr = opendir("/sys/class/hwmon");
    if (dr) {
        struct dirent *en;
        while ((en = readdir(dr))) {
            if (en->d_name[0] == '.') continue;

            char name_path[256], name[64];
            snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", en->d_name);
            read_one_line(name_path, name, sizeof(name));

            for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
                if (strstr(name, map[i].keyword)) {
                    /* OS-Disk Validation */
                    if (strcmp(map[i].label, "SSD") == 0 && os_prefix[0] != '\0') {
                        char link_path[256], real_path[1024];
                        snprintf(link_path, sizeof(link_path), "/sys/class/hwmon/%s/device", en->d_name);
                        ssize_t len = readlink(link_path, real_path, sizeof(real_path)-1);
                        if (len != -1) {
                            real_path[len] = '\0';
                            if (strstr(real_path, os_prefix)) {
                                strncpy(map[i].target_slot, en->d_name, 31);
                            }
                        }
                    }
                    /* Generic Slot Filling (First-Match) */
                    else if (map[i].target_slot[0] == '\0') {
                        strncpy(map[i].target_slot, en->d_name, 31);
                    }
                }
            }
        }
        closedir(dr);
    }

    scan_for_monitor(cfg->path_monitor, 255);
    scan_for_audio(cfg->path_audio, 255);
}
