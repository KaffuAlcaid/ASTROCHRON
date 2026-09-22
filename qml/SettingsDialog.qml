import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property AppState clock
    required property WeatherModel weather
    title: "设置"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 430
    standardButtons: Dialog.Close
    contentItem: ColumnLayout {
        spacing: 16
        RowLayout {
            Label {
                text: "实时更新频率"
                Layout.fillWidth: true
            }
            SpinBox {
                from: 1
                to: 60
                editable: true
                live: true
                value: dialog.clock.updateFrequency
                onValueModified: dialog.clock.updateFrequency = value
            }
            Label {
                text: "Hz"
            }
        }
        CheckBox {
            text: "获取观测天气预报"
            checked: dialog.weather.enabled
            onToggled: dialog.weather.enabled = checked
        }
        Label {
            text: "天气查询向 Open-Meteo 发送观测地点坐标。"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: Theme.muted
        }
    }
}
