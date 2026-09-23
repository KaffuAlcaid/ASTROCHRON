import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

RowLayout {
    id: toolbar
    required property WorldMap map
    required property Action followAction
    required property Action zoomInAction
    required property Action zoomOutAction
    required property Action globalViewAction
    required property Action pickAction
    required property Action expandAction
    property string title
    property bool hasTarget: false
    property bool expanded: false
    property bool following: false
    signal detailsRequested
    spacing: 2
    Label {
        text: toolbar.title
        font.bold: true
        font.pixelSize: 13
        Layout.fillWidth: true
        elide: Text.ElideRight
    }
    IconButton {
        icon.source: "qrc:/icons/info.svg"
        tip: qsTr("目标详情")
        enabled: toolbar.hasTarget
        onClicked: toolbar.detailsRequested()
    }
    IconButton {
        action: toolbar.followAction
        highlighted: toolbar.following
        shortcutHint: "F"
    }
    IconButton {
        action: toolbar.zoomOutAction
        shortcutHint: "-"
    }
    Label {
        text: toolbar.map.zoom.toFixed(1) + "×"
        font.family: Theme.numberFont
        font.pixelSize: 12
        Layout.preferredWidth: 38
        horizontalAlignment: Text.AlignHCenter
    }
    IconButton {
        action: toolbar.zoomInAction
        shortcutHint: "+"
    }
    IconButton {
        action: toolbar.globalViewAction
    }
    IconButton {
        action: toolbar.pickAction
    }
    IconButton {
        action: toolbar.expandAction
    }
}
