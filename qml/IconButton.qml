import QtQuick
import QtQuick.Controls

ToolButton {
    id: control
    property string tip: action ? action.text : ""
    property string shortcutHint: ""
    display: AbstractButton.IconOnly
    focusPolicy: Qt.StrongFocus
    implicitWidth: 30
    implicitHeight: 30
    icon.width: 17
    icon.height: 17
    icon.color: Theme.text
    Accessible.name: tip
    ToolTip.visible: hovered || visualFocus
    ToolTip.text: tip + (shortcutHint.length ? " (" + shortcutHint + ")" : "")
    ToolTip.delay: 450
    Keys.onReturnPressed: click()
    Keys.onEnterPressed: click()
    background: Rectangle {
        color: control.down ? Theme.selection : control.checked ? Theme.selection : control.hovered ? Theme.hover : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.accent
        radius: 3
    }
}
