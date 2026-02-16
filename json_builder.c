#include "json_builder.h"
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
    const char* c_soc  = get_color(v->max_temp, cfg->limit_temp_warn, cfg->limit_temp_crit, cfg);
    const char* c_ssd  = get_color(v->ssd_temp, cfg->limit_ssd_warn, cfg->limit_ssd_crit, cfg);
    const char* c_ram  = get_color(v->ram_temp, cfg->limit_ram_warn, cfg->limit_ram_crit, cfg);
    const char* c_net  = get_color(v->net_temp, cfg->limit_net_warn, cfg->limit_net_crit, cfg);
    const char* c_wall = get_color(pwr->wall_w, cfg->limit_wall_warn, cfg->limit_wall_crit, cfg);

    const char* c_sep = cfg->color_sep;
    if (v->current_teff > (registered_max + cfg->trend_break_delta)) {
        c_sep = cfg->color_warn;
    }

    int written = fprintf(fp, "{"
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
    cfg->font_size, cfg->font_family, (int)v->cpu_mhz, c_mhz, v->max_temp, c_soc,
                          v->ssd_temp, c_ssd, cfg->ssd_label, v->ram_temp, c_ram, v->net_temp, c_net,
                          pwr->soc_w, cfg->color_safe, pwr->system_w, cfg->color_safe, pwr->ext_w,
                          cfg->color_safe, pwr->wall_w, c_wall, pwr->cost, cfg->color_safe, c_sep);

    return (written > 0) ? 1 : 0;
}

int json_build_tooltip(FILE *fp, const AppConfig *cfg, const Accumulator *acc, const DashboardPower *pwr, double current_teff, int maturity_seconds, time_t amber_start) {
    /* 1. Energy & Time: Cast to long to fix 'invalid operands' error */
    double avg_w = (acc->total_sec > 0) ? (acc->total_ws / acc->total_sec) : 0.0;
    double kwh = acc->total_ws / 3600000.0;
    long t_sec = (long)acc->total_sec;
    int h = (int)(t_sec / 3600), m = (int)((t_sec % 3600) / 60);

    /* 2. Compact Thermal Maturity Variables */
    int m_now = maturity_seconds / 60;
    int m_goal = (int)(cfg->maturity_required_sec / 60);

    /* 3. Combined Compact Layout */
    int written = fprintf(fp,
                          "Since: %s | Avg: %.1fW\n"
                          "Cons: %.3f kWh | Cost: €%.2f\n"
                          "Total Time: %d:%02dh\n\n"
                          "Thermal Model:\n"
                          "  Base: %.1f°C | Sink: %.1f°C\n"
                          "  Limit: %.1f°C | Mat: %dm/%dm",
                          cfg->start_date, avg_w, kwh, pwr->cost, h, m,
                          acc->registered_max, current_teff,
                          acc->registered_max + cfg->trend_break_delta, m_now, m_goal
    );

    if (written < 0) return 0;

    /* 4. Append Amber Hold only if active */
    if (amber_start > 0) {
        int hold_s = (int)difftime(time(NULL), amber_start);
        written = fprintf(fp, "\n  Amber Hold: %dm %ds", hold_s / 60, hold_s % 60);
        if (written < 0) return 0;
    }

    return 1;
}
