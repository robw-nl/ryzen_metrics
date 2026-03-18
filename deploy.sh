#!/bin/bash
# Manjaro System Metrics - Safe Deployment Pipeline

# --- 1. Define Standard Paths ---
BIN_DIR="$HOME/.local/bin"
CONF_DIR="$HOME/.config/system_metrics"
SYSTEMD_DIR="$HOME/.config/systemd/user"
APPLET_DIR="../com.rob.ryzenmetrics"

echo "--- Starting Deployment Pipeline ---"

# --- 2. Binary Deployment ---
mkdir -p "$BIN_DIR"
if [ -f "daemon_release" ]; then
    echo "-> Found Release binary. Deploying..."
    install -m 755 daemon_release "$BIN_DIR/manjaro_metrics_daemon"
elif [ -f "daemon" ]; then
    echo "-> Found Standard binary. Deploying..."
    install -m 755 daemon "$BIN_DIR/manjaro_metrics_daemon"
elif [ -f "daemon_debug" ]; then
    echo "-> Found Debug binary. Deploying..."
    install -m 755 daemon_debug "$BIN_DIR/manjaro_metrics_daemon"
else
    echo "-> ERROR: No compiled binary found! Run 'make release', 'make', or 'make debug' first."
    exit 1
fi
chmod +x "$BIN_DIR/manjaro_metrics_daemon"

# --- 3. Configuration Guard ---
mkdir -p "$CONF_DIR"
if [ ! -f "$CONF_DIR/metrics.conf" ]; then
    echo "-> No existing config found. Triggering generator..."
    if [ -x "./config_gen.sh" ]; then
        ./config_gen.sh
        mv metrics.conf "$CONF_DIR/"
    else
        echo "-> ERROR: config_gen.sh not found or not executable."
    fi
else
    echo "-> Existing metrics.conf protected. Skipping generation."
fi

# --- 4. Systemd Service Integration ---
echo "-> Configuring user systemd service..."
mkdir -p "$SYSTEMD_DIR"
cat <<EOF > "$SYSTEMD_DIR/system_metrics.service"
[Unit]
Description=Manjaro System Metrics Daemon
After=plasma-workspace.target

[Service]
Type=simple
ExecStart=$BIN_DIR/manjaro_metrics_daemon
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable system_metrics.service
systemctl --user restart system_metrics.service

# --- 5. Plasma Applet Installation/Upgrade ---
echo "-> Processing Plasma Applet..."
if [ -d "$APPLET_DIR" ]; then
    # Check if already installed
    if kpackagetool6 -t Plasma/Applet -l | grep -q "com.rob.manjarometrics"; then
        echo "-> Upgrading existing Plasmoid..."
        kpackagetool6 -t Plasma/Applet -u "$APPLET_DIR"
    else
        echo "-> Installing new Plasmoid..."
        kpackagetool6 -t Plasma/Applet -i "$APPLET_DIR"
    fi
else
    echo "-> WARNING: Applet directory $APPLET_DIR not found. Skipping UI deployment."
fi

echo "--- Deployment Complete ---"
