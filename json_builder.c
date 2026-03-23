#include "json_builder.h"
#include "error_policy.h"
#include <string.h>
#include <time.h>

static const char* get_color(double val, double warn, double crit, const AppConfig *cfg) {
    if (val < 1.0) return cfg->color_safe;
    if (val >= crit) return cfg->color_crit;
    if (val >= warn) return cfg->color_warn;
    return cfg->color_safe;
}

int json_build_panel(FILE *fp, const AppConfig *cfg, const SystemVitals *v, const DashboardPower *pwr, double registered_max) {
    const char* c_mhz  = get_color(v->cpu_mhz, cfg->limit_mhz_warn, cfg->limit_mhz_crit, cfg);
    const char* c_temp = get_color(v->max_temp, cfg->limit_temp_warn, cfg->limit_temp_crit, cfg);
    const char* c_ssd  = get_color(v->ssd_temp, cfg->limit_ssd_warn, cfg->limit_ssd_crit, cfg);
    const char* c_ram  = get_color(v->ram_temp, cfg->limit_ram_warn, cfg->limit_ram_crit, cfg);
    const char* c_net  = get_color(v->net_temp, cfg->limit_net_warn, cfg->limit_net_crit, cfg);
    const char* c_wall = get_color(pwr->wall_w, cfg->limit_wall_warn, cfg->limit_wall_crit, cfg);
    const char* c_soc_pwr = get_color(pwr->soc_w, cfg->limit_soc_power_warn, cfg->limit_soc_power_crit, cfg);

    const char* c_sep = cfg->color_sep;
    if (v->current_teff > (registered_max + cfg->trend_break_delta)) {
        c_sep = cfg->color_warn;
    }

    /* Buffer doubled to 2048 to eliminate config bloat truncation risks */
    char buffer[2048];
    int written = snprintf(buffer, sizeof(buffer),
                           "{"
                           "\"font_size\":%d,"
                           "\"font_family\":\"%s\","
                           "\"cpu\":{\"val\":%d,\"unit\":\"MHz\",\"color\":\"%s\"},"
                           "\"temp\":{\"val\":%.1f,\"unit\":\"°C\",\"color\":\"%s\"},"
                           "\"ssd\":{\"val\":%.0f,\"unit\":\"°C\",\"color\":\"%s\",\"label\":\"%s\"},"
                           "\"ram\":{\"val\":%.0f,\"unit\":\"°C\",\"color\":\"%s\"},"
                           "\"net\":{\"val\":%.0f,\"unit\":\"°C\",\"color\":\"%s\"},"
                           "\"soc\":{\"val\":%.1f,\"unit\":\"W\",\"color\":\"%s\"},"
                           "\"sys\":{\"val\":%.1f,\"unit\":\"W\",\"color\":\"%s\"},"
                           "\"ext\":{\"val\":%.1f,\"unit\":\"W\",\"color\":\"%s\"},"
                           "\"wall\":{\"val\":%.1f,\"unit\":\"W\",\"color\":\"%s\"},"
                           "\"cost\":{\"val\":%.2f,\"unit\":\"€\",\"color\":\"%s\"},"
                           "\"sep_color\":\"%s\""
                           "}",
                           cfg->font_size, cfg->font_family, (int)v->cpu_mhz, c_mhz, v->max_temp, c_temp,
                           v->ssd_temp, c_ssd, cfg->ssd_label, v->ram_temp, c_ram, v->net_temp, c_net,
                           pwr->soc_w, c_soc_pwr, pwr->system_w, cfg->color_safe, pwr->ext_w,
                           cfg->color_safe, pwr->wall_w, c_wall, pwr->cost, cfg->color_safe, c_sep);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        /* Hooks into central error policy instead of failing silently [cite: 256] */
        log_error(ERR_RECOVERABLE, "JSON_Panel", "Buffer overflow prevented. Payload dropped.");
        return 0;
    }

    size_t elements_written = fwrite(buffer, 1, written, fp);
    return (elements_written == (size_t)written) ? 1 : 0;
}

int json_build_tooltip(FILE *fp, const AppConfig *cfg, const Accumulator *acc, const DashboardPower *pwr, double current_teff, int maturity_seconds, time_t amber_start) {
    double avg_w = (acc->total_sec > 0) ? (acc->total_ws / acc->total_sec) : 0.0;
    double kwh = acc->total_ws / 3600000.0;
    long t_sec = (long)acc->total_sec;
    int h = (int)(t_sec / 3600), m = (int)((t_sec % 3600) / 60);

    int m_now = maturity_seconds / 60;
    int m_goal = (int)(cfg->maturity_required_sec / 60);

    char buffer[2048];
    int written = snprintf(buffer, sizeof(buffer),
                           "Since: %s | Avg: %.1fW\n"
                           "Cons: %.3f kWh | Cost: €%.2f\n"
                           "Total Time: %d:%02dh\n\n"
                           "Thermal Model:\n"
                           "  Base: %.1f°C | Sink: %.1f°C\n"
                           "  Limit: %.1f°C | Mat: %dm/%dm",
                           cfg->start_date, avg_w, kwh, pwr->cost, h, m,
                           acc->registered_max, current_teff,
                           acc->registered_max + cfg->trend_break_delta, m_now, m_goal);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        log_error(ERR_RECOVERABLE, "JSON_Tooltip", "Primary buffer overflow prevented.");
        return 0;
    }

    if (amber_start > 0) {
        int hold_s = (int)difftime(time(NULL), amber_start);
        int appended = snprintf(buffer + written, sizeof(buffer) - written,
                                "\n  Amber Hold: %dm %ds", hold_s / 60, hold_s % 60);

        if (appended < 0 || (size_t)appended >= sizeof(buffer) - written) {
            /* Protects the secondary append logic [cite: 267] */
            log_error(ERR_RECOVERABLE, "JSON_Tooltip", "Amber Hold append overflow prevented.");
            return 0;
        }
        written += appended;
    }

    size_t elements_written = fwrite(buffer, 1, written, fp);
    return (elements_written == (size_t)written) ? 1 : 0;
}
