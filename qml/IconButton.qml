import QtQuick
import QtQuick.Controls

ToolButton {
    property string tip
    implicitWidth: 30
    implicitHeight: 30
    icon.width: 17
    icon.height: 17
    icon.color: Theme.text
    Accessible.name: tip
    ToolTip.visible: hovered
    ToolTip.text: tip
    ToolTip.delay: 450
}
