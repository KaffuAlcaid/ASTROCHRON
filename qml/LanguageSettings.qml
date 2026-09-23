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
            Layout.preferredWidth: 220
            model: [qsTr("跟随系统"), "简体中文", "English"]
            currentIndex: ["system", "zh_CN", "en"].indexOf(control.clock.language)
            onActivated: control.clock.language = ["system", "zh_CN", "en"][currentIndex]
        }
    }
    RowLayout {
        visible: control.clock.language !== control.clock.startupLanguage
        Label {
            text: qsTr("语言设置将在重新启动后生效")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }
        Button {
            text: qsTr("重新启动")
            onClicked: restartError.visible = !control.clock.restart()
        }
    }
    Label {
        id: restartError
        visible: false
        text: qsTr("请关闭程序后重新打开，以应用语言设置")
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.muted
    }
}
