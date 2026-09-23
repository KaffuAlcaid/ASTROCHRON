import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

ColumnLayout {
    id: control
    required property AppState clock
    spacing: 10
    RowLayout {
        Label { text: qsTr("语言"); Layout.fillWidth: true }
        ComboBox {
            id: languageChoice
            Layout.preferredWidth: 220
            model: ["system", "简体中文", "English"]
            displayText: currentIndex === 0 ? qsTr("跟随系统") : currentText
            Accessible.name: displayText
            delegate: ItemDelegate {
                required property int index
                required property string modelData
                width: languageChoice.width
                text: index === 0 ? qsTr("跟随系统") : modelData
                highlighted: languageChoice.highlightedIndex === index
            }
            currentIndex: ["system", "zh_CN", "en"].indexOf(control.clock.language)
            onActivated: control.clock.language = ["system", "zh_CN", "en"][currentIndex]
        }
    }
}
