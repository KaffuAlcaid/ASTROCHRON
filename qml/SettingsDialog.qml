import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property AppState clock
    required property WeatherModel weather
    title: qsTr("设置")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 430
    standardButtons: Dialog.Close
    UpdateChecker { id: updates }
    Connections {
        target: dialog.clock
        function onLocalizedChanged() { updates.retranslate(); }
    }
    contentItem: ColumnLayout {
        spacing: 16
        LanguageSettings {
            clock: dialog.clock
            Layout.fillWidth: true
        }
        RowLayout {
            Label {
                text: qsTr("实时更新频率")
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
            text: qsTr("获取观测天气预报")
            checked: dialog.weather.enabled
            onToggled: dialog.weather.enabled = checked
        }
        Label {
            text: qsTr("天气查询向 Open-Meteo 发送观测地点坐标。")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: Theme.muted
        }
        Label {
            text: qsTr("软件更新")
            font.bold: true
            Layout.topMargin: 8
        }
        Label {
            text: qsTr("当前版本：%1").arg(updates.currentVersion)
            color: Theme.muted
        }
        CheckBox {
            text: qsTr("接收预发布版本")
            checked: updates.includePrereleases
            onToggled: updates.includePrereleases = checked
        }
        RowLayout {
            Button {
                text: qsTr("检查更新")
                icon.source: "qrc:/icons/refresh-cw.svg"
                icon.color: Theme.text
                enabled: !updates.busy
                onClicked: updates.check()
            }
            Button {
                text: qsTr("打开发布页")
                icon.source: "qrc:/icons/download.svg"
                icon.color: Theme.text
                onClicked: Qt.openUrlExternally(updates.releaseUrl)
            }
        }
        Label {
            text: updates.status
            visible: text.length > 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: updates.available ? Theme.accent : Theme.muted
        }
        ScrollView {
            visible: updates.available && updates.releaseNotes.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            contentWidth: availableWidth
            TextArea {
                text: updates.releaseNotes
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
            }
        }
    }
}
