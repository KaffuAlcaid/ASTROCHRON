import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property SatelliteModel satellites
    readonly property var candidates: satellites.cleanupCandidates
    title: qsTr("在线历史快照")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, Overlay.overlay.width - 32)
    height: Math.min(560, Overlay.overlay.height - 32)
    onOpened: satellites.refreshStorage()
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: qsTr("每组保留最近 %1 份在线快照，固定记录和本地导入长期保留").arg(dialog.satellites.snapshotRetention)
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: qsTr("待清理 %1 份").arg(dialog.candidates.length)
            font.bold: true
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: dialog.candidates
            clip: true
            ScrollBar.vertical: ScrollBar {}
            delegate: RowLayout {
                required property var modelData
                width: ListView.view.width - 16
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        text: modelData.group + " · " + modelData.acquired
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                    Label {
                        text: modelData.source + " · " + modelData.size
                        Layout.fillWidth: true
                        Layout.bottomMargin: 12
                        wrapMode: Text.WrapAnywhere
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                }
                IconButton {
                    icon.source: "qrc:/icons/pin.svg"
                    tip: qsTr("固定此快照")
                    enabled: !dialog.satellites.storageBusy
                    onClicked: dialog.satellites.pinSnapshot(modelData.id, true)
                }
            }
            Label {
                anchors.centerIn: parent
                visible: dialog.candidates.length === 0
                text: qsTr("暂无待清理记录")
                color: Theme.muted
            }
        }
        Label {
            text: dialog.satellites.storageStatus
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
    footer: DialogButtonBox {
        Button {
            text: dialog.satellites.autoCleanup ? qsTr("清理以上记录") : qsTr("清理并启用自动清理")
            enabled: !dialog.satellites.storageBusy
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: if (dialog.satellites.enableCleanup()) dialog.close()
        }
        Button { text: qsTr("关闭"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        onRejected: dialog.close()
    }
}
