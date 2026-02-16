#include "discovery.h"
#include "error_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

// --- Helpers (Unchanged) ---
static int has_file(const char *dir, const char *filename) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    return (access(path, F_OK) == 0);
}

static void read_one_line(const char *path, char *out_buf, size_t size) {
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(out_buf, size, f)) {
            out_buf[strcspn(out_buf, "\n")] = 0;
        }
        fclose(f);
    } else {
        out_buf[0] = '\0';
    }
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
        log_error(ERR_FATAL, "Compat", "Unsupported Desktop Environment. KDE Plasma required");
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

        if (strcmp(status, "connected") == 0 && strcmp(enabled, "enabled") == 0) {
            strncpy(out_path, path_s, size - 1);
            break; // Found the active primary monitor
        }
    }
    closedir(dr);
}

// --- NEW: Audio Discovery ---
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

        // If this card is better than what we found so far, save it
        if (current_prio > found_priority) {
            // We assume pcm0p/sub0/status is the main stream.
            // Some cards use pcm1p, but pcm0p is standard for main output.
            char candidate[256];
            snprintf(candidate, sizeof(candidate), "/proc/asound/%s/pcm0p/sub0/status", en->d_name);

            if (access(candidate, F_OK) == 0) {
                strncpy(out_path, candidate, size - 1);
                found_priority = current_prio;
            }
        }
    }
    closedir(dr);
}

void discover_hardware(AppConfig *cfg) {
    // (Existing Hwmon Logic - Unchanged)
    DIR *dr = opendir("/sys/class/hwmon");
    if (dr) {
        struct dirent *en;
        while ((en = readdir(dr))) {
            if (en->d_name[0] == '.') continue;
            char dir_path[256], name_path[256], name[64];
            snprintf(dir_path, sizeof(dir_path), "/sys/class/hwmon/%s", en->d_name);
            snprintf(name_path, sizeof(name_path), "%s/name", dir_path);
            read_one_line(name_path, name, sizeof(name));
            if (name[0] == '\0') continue;

            if (strcmp(name, "amdgpu") == 0 && (has_file(dir_path, "power1_average") || has_file(dir_path, "power1_input"))) strncpy(cfg->hw_gpu, name, 31);
            if (strcmp(name, "zenpower") == 0) strncpy(cfg->hw_cpu, name, 31);
            else if (strcmp(name, "k10temp") == 0 && strcmp(cfg->hw_cpu, "zenpower") != 0) strncpy(cfg->hw_cpu, name, 31);
            if (strcmp(name, "spd5118") == 0) strncpy(cfg->hw_ram, name, 31);
            else if (strcmp(name, "jc42") == 0 && strcmp(cfg->hw_ram, "spd5118") != 0) strncpy(cfg->hw_ram, name, 31);
            if (strstr(name, "r8169") || strcmp(name, "igc") == 0 || strcmp(name, "e1000e") == 0) strncpy(cfg->hw_net, name, 31);
            if (strcmp(name, "nvme") == 0) strncpy(cfg->hw_disk, name, 31);
        }
        closedir(dr);
    }

    // Monitor
    scan_for_monitor(cfg->path_monitor, 255);

    // NEW: Audio
    scan_for_audio(cfg->path_audio, 255);
}
