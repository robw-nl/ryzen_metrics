#!/bin/bash
# Manjaro Metrics: Comprehensive Config Generator (Audited & Verified)

CONFIG_FILE="metrics.conf"
# 1. THE SOURCE OF TRUTH: Single path definition [cite: 2026-02-12]
TARGET_DATA_FILE="/home/rob/.config/system_metrics/stats.dat"

echo "--- Probing Hardware Architecture ---"

# Discover GPU (Targeting amdgpu for 8700G) [cite: 2026-01-26]
HW_GPU=$(grep -l "amdgpu" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)

# Discover CPU (Targeting k10temp) [cite: 2026-01-26]
HW_CPU=$(grep -l "k10temp" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)

# Discover NVMe/SSD [cite: 2026-01-26]
HW_DISK=$(grep -l "nvme" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)

# Discover RAM (Often jc42 or spd) [cite: 2026-01-26]
HW_RAM=$(grep -lE "jc42|spd" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)

# Discover Network (Find active interface driver) [cite: 2026-01-26]
HW_NET=$(ls -l /sys/class/net/ | grep -v "virtual" | head -n 2 | tail -n 1 | awk '{print $9}' | xargs -I{} ethtool -i {} 2>/dev/null | grep "driver" | awk '{print $2}')

# Discover Monitor Status Path [cite: 2026-01-26]
DRM_PATH=$(find /sys/class/drm/ -name "status" | grep -E "HDMI|DP" | head -n 1)

echo "--- Resolving System Paths ---"
# Use Bash Parameter Expansion to get the directory safely [cite: 2026-02-12]
TARGET_DIR="${TARGET_DATA_FILE%/*}"
mkdir -p "$TARGET_DIR"

echo "Generating Full $CONFIG_FILE..."

cat <<EOF > "$CONFIG_FILE"
# --- Manjaro System Metrics: Full Calibration Configuration ---

# --- Zone 1: Fast Lane (Shared RAM) ---
path_panel=/dev/shm/system_metrics_panel.json
path_tooltip=/dev/shm/system_metrics_tooltip.txt

# --- Zone 2: Persistence (Local SSD) ---
# Audited: Points to the verified absolute path [cite: 2026-02-12]
path_data_file=$TARGET_DATA_FILE

# --- General Settings ---
# Audited: Hardcoded to prevent historical resets [cite: 2026-02-12]
start_date=28-01-2026
update_ms=1000
sync_sec=300
euro_per_kwh=0.26

# --- Ghost-Buster Calibration (SoC Filter) ---
ghost_freq_min=2500
ghost_watt_max=10.0
ghost_floor=0.1

# --- Power Constants (8700G Passive Block) ---
psu_efficiency=0.85
mobo_overhead=0.05
pc_rest_base=7.1
periph_watt=3.5

# --- Monitor (MSI Optix MAG272QP) ---
mon_logic=12.0
mon_backlight_max=26.0
mon_standby=0.5
mon_dim_preset=0.2
mon_brightness_preset=0.75

# --- Speakers (Klipsch The Fives) ---
speakers_eco=2.0
speakers_standby=10.0
speakers_active=25.0
speakers_timeout_sec=900

# --- Visuals ---
font_size=9
font_family=Monospace
color_safe=#ccccff
color_warn=#EBCB8B
color_crit=#BF616A
color_sep=#BBBBBB
ssd_label=JAWS

# --- Thresholds ---
limit_mhz_warn=3200.0
limit_mhz_crit=4800.0
limit_temp_warn=40.0
limit_temp_crit=60.0
limit_ssd_warn=50.0
limit_ssd_crit=65.0
limit_ram_warn=55.0
limit_ram_crit=75.0
limit_net_warn=60.0
limit_net_crit=75.0
limit_soc_warn=20.0
limit_soc_crit=45.0
limit_wall_warn=80.0
limit_wall_crit=110.0

# --- Thermal Model (Maturity) ---
maturity_threshold_teff=39.0
maturity_required_sec=1800
trend_break_delta=5.0

# --- Discovered Hardware Hooks ---
hw_gpu=${HW_GPU:-amdgpu}
hw_cpu=${HW_CPU:-k10temp}
hw_disk=${HW_DISK:-nvme}
hw_ram=${HW_RAM:-spd5118}
hw_net=${HW_NET:-r8169}
path_monitor=${DRM_PATH:-/sys/class/drm/card1-HDMI-A-1/status}
EOF

echo "Done. Config generated with $(wc -l < "$CONFIG_FILE") entries."
