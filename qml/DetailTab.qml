import QtQuick
import QtQuick.Controls

TabButton {
    id: tab
    implicitHeight: 34
    contentItem: Label {
        text: tab.text
        font.pixelSize: Theme.bodySize
        color: tab.checked ? Theme.text : Theme.muted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: tab.checked ? Theme.background : Theme.surface
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 2
            color: tab.checked ? Theme.accent : "transparent"
        }
    }
}
