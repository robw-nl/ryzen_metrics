#!/bin/bash
# Ryzen System Metrics - Safe Deployment Pipeline

# --- 1. Define Standard Paths ---
BIN_DIR="$HOME/.local/bin"
CONF_DIR="$HOME/.config/ryzen_metrics_daemon_release"
SYSTEMD_DIR="$HOME/.config/systemd/user"
APPLET_DIR="../com.rob.ryzenmetrics_release"
SERVICE_NAME="ryzen_metrics_daemon_release.service"

echo "--- Starting Deployment Pipeline ---"

# Explicitly halt the daemon before replacing the binary to ensure clean FD release
systemctl --user stop $SERVICE_NAME 2>/dev/null || true

# --- 2. Binary Deployment ---
mkdir -p "$BIN_DIR"

if [ -f "ryzen_metrics_daemon_release" ]; then
    echo "-> Found Release binary. Deploying..."
    install -m 755 ryzen_metrics_daemon_release "$BIN_DIR/ryzen_metrics_daemon_release"
    ACTIVE_BIN="ryzen_metrics_daemon_release"
elif [ -f "ryzen_metrics_daemon" ]; then
    echo "-> Found Standard binary. Deploying..."
    install -m 755 ryzen_metrics_daemon "$BIN_DIR/ryzen_metrics_daemon"
    ACTIVE_BIN="ryzen_metrics_daemon"
elif [ -f "ryzen_metrics_daemon_debug" ]; then
    echo "-> Found Debug binary. Deploying..."
    install -m 755 ryzen_metrics_daemon_debug "$BIN_DIR/ryzen_metrics_daemon_debug"
    ACTIVE_BIN="ryzen_metrics_daemon_debug"
else
    echo "-> ERROR: No compiled binary found! Run 'make release', 'make', or 'make debug' first."
    exit 1
fi

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
cat <<EOF > "$SYSTEMD_DIR/$SERVICE_NAME"
[Unit]
Description=Ryzen System Metrics Daemon
After=plasma-workspace.target

[Service]
Type=simple
ExecStart=$BIN_DIR/$ACTIVE_BIN
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload

# Tear down the legacy service if it exists
if systemctl --user is-active --quiet system_metrics.service; then
    systemctl --user stop system_metrics.service
    systemctl --user disable system_metrics.service
    rm -f "$SYSTEMD_DIR/system_metrics.service"
fi

systemctl --user enable $SERVICE_NAME
systemctl --user restart $SERVICE_NAME

# --- 5. Plasma Applet Installation/Upgrade ---
echo "-> Processing Plasma Applet..."
if [ -d "$APPLET_DIR" ]; then
if kpackagetool6 -t Plasma/Applet -l | grep -q "com.rob.ryzenmetrics_release";
then
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
