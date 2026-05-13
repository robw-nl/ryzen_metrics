#!/bin/bash
# Ryzen Metrics: Comprehensive Config Generator (Audited & Verified)

CONFIG_FILE="metrics.conf"
# 1. THE SOURCE OF TRUTH: Dynamic path definition
TARGET_DATA_FILE="$HOME/.config/ryzen_metrics_daemon_release/stats.dat"

echo "--- Probing Hardware Architecture ---"

HW_GPU=$(grep -l "amdgpu" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)
HW_CPU=$(grep -l "k10temp" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)
HW_DISK=$(grep -l "nvme" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)
HW_RAM=$(grep -lE "jc42|spd" /sys/class/hwmon/hwmon*/name 2>/dev/null | head -n1 | xargs -I{} cat {}/name)
HW_NET=$(ls -l /sys/class/net/ | grep -v "virtual" | head -n 2 | tail -n 1 | awk '{print $9}' | xargs -I{} ethtool -i {} 2>/dev/null | grep "driver" | awk '{print $2}')
DRM_PATH=$(find /sys/class/drm/ -name "status" | grep -E "HDMI|DP" | head -n 1)

echo "--- Select Hardware Profile ---"
echo "1) 8700G Passive Block (Original)"
echo "2) Standard Desktop (Active Cooling)"
echo "3) Laptop / Mobile APU"
read -p "Select profile [1-3]: " PROFILE_CHOICE

case $PROFILE_CHOICE in
    2)
        GHOST_FREQ=800; GHOST_WATT=5.0; GHOST_FLOOR=0.5
        PC_BASE=15.0; MATURITY_THRES=45.0
        ;;
    3)
        GHOST_FREQ=1200; GHOST_WATT=3.0; GHOST_FLOOR=0.1
        PC_BASE=3.0; MATURITY_THRES=45.0
        ;;
    *) # Default to 1 (Passive)
        GHOST_FREQ=2500; GHOST_WATT=10.0; GHOST_FLOOR=0.1
        PC_BASE=7.1; MATURITY_THRES=39.0
        ;;
esac

echo "--- Resolving System Paths ---"
TARGET_DIR="${TARGET_DATA_FILE%/*}"
mkdir -p "$TARGET_DIR"

echo "Generating Full $CONFIG_FILE..."

cat <<EOF > "$CONFIG_FILE"
# --- Ryzen System Metrics: Full Calibration Configuration ---

# --- Zone 1: Fast Lane (Shared RAM) ---
path_panel=/dev/shm/system_metrics_panel.json
path_tooltip=/dev/shm/system_metrics_tooltip.txt

# --- Zone 2: Persistence (Local SSD) ---
path_data_file=$TARGET_DATA_FILE

# --- General Settings ---
start_date=$(date +"%d-%m-%Y")
update_ms=1000
sync_sec=300
euro_per_kwh=0.26

# --- Ghost-Buster Calibration (SoC Filter) ---
ghost_freq_min=$GHOST_FREQ
ghost_watt_max=$GHOST_WATT
ghost_floor=$GHOST_FLOOR

# --- Power Constants ---
psu_efficiency=0.85
mobo_overhead=0.05
pc_rest_base=$PC_BASE
periph_watt=3.5

# --- Monitor ---
mon_logic=12.0
mon_backlight_max=26.0
mon_standby=0.5
mon_dim_preset=0.2
mon_brightness_preset=0.75

# --- Speakers ---
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
ssd_label=SSD

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
maturity_threshold_teff=$MATURITY_THRES
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
