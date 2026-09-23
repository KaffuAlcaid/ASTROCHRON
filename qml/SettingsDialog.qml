import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property AppState clock
    required property WeatherModel weather
    required property SatelliteModel satellites
    required property var mapView
    title: qsTr("设置")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 430
    height: Math.min(implicitHeight, Overlay.overlay ? Overlay.overlay.height - 32 : implicitHeight)
    standardButtons: Dialog.Close
    UpdateChecker { id: updates }
    SnapshotDialog { id: snapshotsDialog; satellites: dialog.satellites }
    onOpened: satellites.refreshStorage()
    Connections {
        target: dialog.clock
        function onLocalizedChanged() { updates.retranslate(); }
    }
    contentItem: ScrollView {
        id: settingsScroll
        implicitHeight: settingsContent.implicitHeight
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            id: settingsContent
            width: settingsScroll.availableWidth
            spacing: 16
            LanguageSettings {
                clock: dialog.clock
                Layout.fillWidth: true
            }
            CheckBox {
                text: qsTr("跟随当前卫星")
                checked: dialog.mapView.following
                onToggled: dialog.mapView.following = checked
            }
            RowLayout {
                Label { text: qsTr("地图倍率"); Layout.fillWidth: true }
                SpinBox {
                    from: 1; to: 12
                    value: Math.round(dialog.mapView.zoom)
                    onValueModified: dialog.mapView.zoom = value
                }
                Label { text: "×" }
            }
            RowLayout {
                Label {
                    text: qsTr("绘图刷新频率")
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
            RowLayout {
                Label {
                    text: qsTr("轨道计算频率")
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 1
                    to: dialog.clock.updateFrequency
                    editable: true
                    live: true
                    value: dialog.clock.calculationFrequency
                    onValueModified: dialog.clock.calculationFrequency = value
                }
                Label { text: "Hz" }
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
                text: qsTr("数据保存")
                font.bold: true
                Layout.topMargin: 8
            }
            Label {
                readonly property var info: dialog.satellites.storageInfo
                text: qsTr("快照 %1 份 · 固定 %2 份 · 本地导入 %3 份").arg(info.count || 0).arg(info.pinned || 0).arg(info.imported || 0)
                    + "\n" + qsTr("数据库 %1 MiB · 可回收 %2 MiB").arg(((info.bytes || 0) / 1048576).toFixed(1)).arg(((info.freeBytes || 0) / 1048576).toFixed(1))
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
            }
            RowLayout {
                Label { text: qsTr("每组在线历史"); Layout.fillWidth: true }
                ComboBox {
                    model: [5, 10]
                    currentIndex: dialog.satellites.snapshotRetention === 5 ? 0 : 1
                    enabled: !dialog.satellites.storageBusy
                    onActivated: dialog.satellites.snapshotRetention = currentIndex === 0 ? 5 : 10
                }
                Label { text: qsTr("份") }
            }
            CheckBox {
                text: qsTr("自动清理在线历史")
                checked: dialog.satellites.autoCleanup
                enabled: !dialog.satellites.storageBusy
                nextCheckState: function() {
                    if (checked) dialog.satellites.disableCleanup();
                    else snapshotsDialog.open();
                    return Qt.Unchecked;
                }
            }
            RowLayout {
                Button {
                    text: qsTr("查看待清理记录")
                    enabled: !dialog.satellites.storageBusy
                    onClicked: snapshotsDialog.open()
                }
                Button {
                    text: qsTr("整理数据库")
                    enabled: !dialog.satellites.storageBusy && !dialog.satellites.downloading
                    onClicked: dialog.satellites.compactDatabase()
                }
            }
            Label {
                text: dialog.satellites.storageStatus
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
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
}
