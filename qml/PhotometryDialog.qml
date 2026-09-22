import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property SatelliteModel satellites
    title: "星等参数"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(510, Overlay.overlay ? Overlay.overlay.width - 32 : 510)
    standardButtons: Dialog.Close
    function loadParameters() {
        const parameters = satellites.photometry;
        magnitude.text = parameters.magnitude === undefined ? "" : parameters.magnitude.toFixed(2);
        phase.currentIndex = parameters.phase === 0 ? 0 : 1;
        source.text = parameters.source || "";
    }
    onOpened: loadParameters()
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: (dialog.satellites.observation.name || "") + " · NORAD " + dialog.satellites.selectedId
            font.pixelSize: 14
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        GridLayout {
            columns: 2
            columnSpacing: 18
            rowSpacing: 10
            Layout.fillWidth: true
            Label { text: "参考星等" }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: magnitude
                    Layout.fillWidth: true
                    placeholderText: "待填写"
                    selectByMouse: true
                    font.family: Theme.numberFont
                    validator: DoubleValidator { bottom: -30; top: 30; decimals: 2; locale: "C"; notation: DoubleValidator.StandardNotation }
                }
                Label { text: "mag" }
            }
            Label { text: "参考距离" }
            Label { text: "1000 km"; font.family: Theme.numberFont }
            Label { text: "参考相位角" }
            ComboBox {
                id: phase
                Layout.fillWidth: true
                model: ["0°（满相）", "90°（半相）"]
            }
            Label { text: "资料来源" }
            TextField {
                id: source
                Layout.fillWidth: true
                placeholderText: "观测记录或资料名称"
                selectByMouse: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "导入星等表"
                icon.source: "qrc:/icons/folder-open.svg"
                icon.color: Theme.text
                onClicked: importDialog.open()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "清除"
                enabled: dialog.satellites.photometry.magnitude !== undefined
                onClicked: if (dialog.satellites.clearPhotometry()) dialog.loadParameters()
            }
            Button {
                text: "保存"
                enabled: magnitude.text.trim().length > 0 && magnitude.acceptableInput
                onClicked: if (dialog.satellites.setPhotometry(Number(magnitude.text), phase.currentIndex === 0 ? 0 : 90, source.text)) dialog.loadParameters()
            }
        }
        Label {
            text: dialog.satellites.photometryStatus
            visible: text.length > 0
            color: Theme.accent
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }
        FieldRow {
            label: "当前相位角"
            value: dialog.satellites.observation.phaseAngle === undefined ? "待计算" : dialog.satellites.observation.phaseAngle.toFixed(1) + "°"
        }
        FieldRow {
            label: "视星等（估算）"
            value: dialog.satellites.observation.magnitude === undefined ? (dialog.satellites.observation.magnitudeStatus || "待计算") : dialog.satellites.observation.magnitude.toFixed(1) + " mag"
        }
        Label {
            text: "大气外亮度采用漫反射球模型，按距离和相位角换算。卫星姿态、镜面反射和大气消光会影响实测亮度。"
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: "QuickSat 星等表以 1000 km、满相时的最大亮度为参考；手动参数优先于导入记录。"
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
    }
    FileDialog {
        id: importDialog
        title: "导入 QuickSat 星等表"
        nameFilters: ["QuickSat 星等表 (*.mag)"]
        onAccepted: {
            dialog.satellites.importMagnitudes(selectedFile);
            dialog.loadParameters();
        }
    }
}
