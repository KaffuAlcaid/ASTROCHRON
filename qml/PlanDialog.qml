import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import Astrochron

Dialog {
    id: dialog
    required property SatelliteModel satellites
    readonly property var info: satellites.planInfo
    property string exportFormat: "csv"
    title: qsTr("导出观测计划")
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, Overlay.overlay.width - 32)
    height: Math.min(540, Overlay.overlay.height - 32)
    onOpened: {
        const button = standardButton(Dialog.Close);
        if (button) button.text = Qt.binding(function() { return qsTr("关闭"); });
    }
    function openPlan() {
        satellites.preparePlan();
        open();
    }
    function save(format) {
        exportFormat = format;
        destination.selectedFile = "file:///" + StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/" + info.fileName + "." + format;
        destination.open();
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: (dialog.info.name || "") + " · " + (dialog.info.observer || "")
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: (dialog.info.start || "") + " → " + (dialog.info.end || "") + "\n" + (dialog.info.zone || "") + " · " + qsTr("最低高度角 %1°").arg(dialog.info.minimum || 0)
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.family: Theme.numberFont
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.satellites.planPasses
            ScrollBar.vertical: ScrollBar {}
            delegate: ColumnLayout {
                required property var modelData
                width: ListView.view.width - 14
                spacing: 4
                Label {
                    text: modelData.start + " → " + modelData.end
                    font.family: Theme.numberFont
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Label {
                    text: (modelData.partial ? qsTr("时段内最高") : qsTr("峰值")) + " " + modelData.maximum + " @ " + modelData.peak + (modelData.optical ? " · " + qsTr("光学") : "")
                    color: Theme.muted
                    Layout.fillWidth: true
                    Layout.bottomMargin: 14
                    wrapMode: Text.Wrap
                }
            }
        }
        Label {
            text: dialog.satellites.planBusy ? qsTr("正在计算观测计划") : dialog.satellites.planStatus
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
    footer: DialogButtonBox {
        Button {
            text: qsTr("导出 CSV")
            icon.source: "qrc:/icons/download.svg"
            enabled: !dialog.satellites.planBusy && dialog.satellites.planPasses.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialog.save("csv")
        }
        Button {
            text: qsTr("导出 ICS")
            icon.source: "qrc:/icons/download.svg"
            enabled: !dialog.satellites.planBusy && dialog.satellites.planPasses.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialog.save("ics")
        }
        onRejected: dialog.close()
    }
    FileDialog {
        id: destination
        title: qsTr("保存观测计划")
        fileMode: FileDialog.SaveFile
        nameFilters: dialog.exportFormat === "csv" ? [qsTr("CSV 文件 (*.csv)")] : [qsTr("日历文件 (*.ics)")]
        defaultSuffix: dialog.exportFormat
        onAccepted: dialog.satellites.exportPlan(selectedFile, dialog.exportFormat)
    }
}
