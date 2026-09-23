import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property SatelliteModel satellites
    title: qsTr("星等参数")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(510, Overlay.overlay ? Overlay.overlay.width - 32 : 510)
    standardButtons: Dialog.Close
    onOpened: {
        loadParameters();
        const button = standardButton(Dialog.Close);
        if (button) button.text = Qt.binding(function() { return qsTr("关闭"); });
    }
    function loadParameters() {
        const parameters = satellites.photometry;
        magnitude.text = parameters.magnitude === undefined ? "" : parameters.magnitude.toFixed(2);
        phase.currentIndex = parameters.phase === 0 ? 0 : 1;
        source.text = parameters.source || "";
        sourceDate.text = parameters.sourceDate || "";
    }
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
            Label { text: qsTr("参考星等") }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: magnitude
                    Layout.fillWidth: true
                    placeholderText: qsTr("待填写")
                    selectByMouse: true
                    font.family: Theme.numberFont
                    validator: DoubleValidator { bottom: -30; top: 30; decimals: 2; locale: "C"; notation: DoubleValidator.StandardNotation }
                }
                Label { text: "mag" }
            }
            Label { text: qsTr("参考距离") }
            Label { text: "1000 km"; font.family: Theme.numberFont }
            Label { text: qsTr("参考相位角") }
            ComboBox {
                id: phase
                Layout.fillWidth: true
                model: [qsTr("0°（满相）"), qsTr("90°（半相）")]
            }
            Label { text: qsTr("资料来源") }
            TextField {
                id: source
                Layout.fillWidth: true
                placeholderText: qsTr("观测记录或资料名称")
                selectByMouse: true
            }
            Label { text: qsTr("资料日期") }
            TextField {
                id: sourceDate
                Layout.fillWidth: true
                placeholderText: qsTr("YYYY-MM-DD（可留空）")
                selectByMouse: true
                font.family: Theme.numberFont
                validator: RegularExpressionValidator { regularExpression: /(?:\d{4}-\d{2}-\d{2})?/ }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("导入星等表")
                icon.source: "qrc:/icons/folder-open.svg"
                icon.color: Theme.text
                onClicked: importDialog.open()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: dialog.satellites.photometry.hasDefault ? qsTr("恢复默认") : qsTr("清除")
                enabled: dialog.satellites.photometry.hasOverride === true
                onClicked: if (dialog.satellites.clearPhotometry()) dialog.loadParameters()
            }
            Button {
                text: qsTr("保存")
                enabled: magnitude.text.trim().length > 0 && magnitude.acceptableInput && sourceDate.acceptableInput
                onClicked: if (dialog.satellites.setPhotometry(Number(magnitude.text), phase.currentIndex === 0 ? 0 : 90, source.text, sourceDate.text)) dialog.loadParameters()
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
            label: qsTr("当前相位角")
            value: dialog.satellites.observation.phaseAngle === undefined ? qsTr("待计算") : dialog.satellites.observation.phaseAngle.toFixed(1) + "°"
        }
        FieldRow {
            label: qsTr("视星等（估算）")
            value: dialog.satellites.observation.magnitude === undefined ? (dialog.satellites.observation.magnitudeStatus || qsTr("待计算")) : dialog.satellites.observation.magnitude.toFixed(1) + " mag"
        }
        Label {
            text: qsTr("大气外亮度采用漫反射球模型，按距离和相位角换算。卫星姿态、镜面反射和大气消光会影响实测亮度。")
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: qsTr("QuickSat 星等表以 1000 km、满相时的最大亮度为参考。内置资料自动参与计算，手动参数和导入资料优先。")
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
    }
    FileDialog {
        id: importDialog
        title: qsTr("导入 QuickSat 星等表")
        nameFilters: [qsTr("QuickSat 星等表 (*.mag)")]
        onAccepted: {
            importDate.text = "";
            importDetails.open();
        }
    }
    Dialog {
        id: importDetails
        property string error: ""
        title: qsTr("星等表资料日期")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 32 : 420)
        onOpened: error = ""
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: qsTr("采用来源标明的发布日期或观测截止日期。")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
            }
            TextField {
                id: importDate
                Layout.fillWidth: true
                placeholderText: qsTr("YYYY-MM-DD（可留空）")
                selectByMouse: true
                font.family: Theme.numberFont
                validator: RegularExpressionValidator { regularExpression: /(?:\d{4}-\d{2}-\d{2})?/ }
            }
            Label {
                text: importDetails.error
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.muted
                visible: text.length > 0
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                Button { text: qsTr("取消"); onClicked: importDetails.close() }
                Button {
                    text: qsTr("导入")
                    enabled: importDate.acceptableInput
                    onClicked: {
                        if (dialog.satellites.importMagnitudes(importDialog.selectedFile, importDate.text)) {
                            importDetails.close();
                            dialog.loadParameters();
                        } else {
                            importDetails.error = dialog.satellites.photometryStatus;
                        }
                    }
                }
            }
        }
    }
}
