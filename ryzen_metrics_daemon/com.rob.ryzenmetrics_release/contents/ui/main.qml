import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as P5Support

PlasmoidItem {
    id: root
    preferredRepresentation: fullRepresentation
    property string popupText: "Loading..."

    // Defaults
    property int myFontSize: 11
    property string myFontFamily: "Monospace"

    fullRepresentation: Item {
        implicitWidth: mainLayout.implicitWidth
        implicitHeight: mainLayout.implicitHeight
        Layout.preferredWidth: mainLayout.implicitWidth
        Layout.minimumWidth: mainLayout.implicitWidth
        Layout.preferredHeight: mainLayout.implicitHeight
        Layout.minimumHeight: mainLayout.implicitHeight

        RowLayout {
            id: mainLayout
            anchors.centerIn: parent
            spacing: 5

            // --- REUSABLE COMPONENT ---
            component MetricText : Text {
                text: "--"
                color: "#9999C0"
                font.family: root.myFontFamily
                font.pointSize: root.myFontSize
            }

            component SlashSep : Text {
                text: "/"
                color: "#666666"
                font.bold: true
                font.family: root.myFontFamily
                font.pointSize: root.myFontSize
            }

            // --- FUNCTION ---
            function updateUI(json) {
                // 1. Update Global Visuals
                if (json.font_size) root.myFontSize = json.font_size
                    if (json.font_family) root.myFontFamily = json.font_family

                        // 2. Frequency
                        cpuTxt.text = json.cpu.val + " MHz"
                        cpuTxt.color = json.cpu.color

                        // 3. Wall Power
                        wallTxt.text = "Wall " + json.wall.val.toFixed(0) + " W"
                        wallTxt.color = json.wall.color

                        // 4. SoC Power
                        socWattTxt.text = "SoC " + json.soc.val.toFixed(1) + " W"
                        socWattTxt.color = json.soc.color

                        // 5. Temperatures (Grouped)
                        socTempTxt.text = "SoC " + json.temp.val.toFixed(0) + "°C"
                        socTempTxt.color = json.temp.color

                        ssdTempTxt.text = json.ssd.label + " " + json.ssd.val.toFixed(0) + "°C"
                        ssdTempTxt.color = json.ssd.color

                        ramTempTxt.text = "RAM " + json.ram.val.toFixed(0) + "°C"
                        ramTempTxt.color = json.ram.color

                        netTempTxt.text = "Net " + json.net.val.toFixed(0) + "°C"
                        netTempTxt.color = json.net.color

                        // 6. Separators
                        sep1.color = json.sep_color
                        sep2.color = json.sep_color
                        sep3.color = json.sep_color
            }

            // --- UI LAYOUT ---
            MetricText { id: cpuTxt }
            MetricText { id: sep1; text: "▓"; font.bold: true; color: json.sep_color }

            MetricText { id: wallTxt }
            MetricText { id: sep2; text: "▓"; font.bold: true; color: json.sep_color }
            MetricText { id: socWattTxt }
            MetricText { id: sep3; text: "▓"; font.bold: true; color: json.sep_color }

            MetricText { id: socTempTxt }
            SlashSep {}
            MetricText { id: ssdTempTxt }
            SlashSep {}
            MetricText { id: ramTempTxt }
            SlashSep {}
            MetricText { id: netTempTxt }
        }

        // --- INTERACTION ---
        PlasmaCore.ToolTipArea {
            anchors.fill: parent
            mainText: "Energy Dashboard"
            subText: root.popupText
        }

        // --- DATA SOURCES ---
        P5Support.DataSource {
            id: dataSource
            engine: "executable"
            // READING FROM RAM DISK (Low Latency)
            connectedSources: ["cat /dev/shm/system_metrics_panel.json"]
            interval: 1000
            onNewData: function(source, data) {
                var raw = data["stdout"]
                if (raw.length < 5) return;
                try {
                    mainLayout.updateUI(JSON.parse(raw))
                } catch(e) { }
            }
        }

        P5Support.DataSource {
            id: tooltipSource
            engine: "executable"
            connectedSources: ["cat /dev/shm/system_metrics_tooltip.txt"]
            interval: 2000 // Check every 2 seconds for the minute-update
            onNewData: function(source, data) {
                var out = data["stdout"]
                if (out && out.length > 10) {
                    root.popupText = out
                }
            }

        }
    }
}
