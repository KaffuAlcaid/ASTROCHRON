import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

RowLayout {
    id: toolbar
    required property WorldMap map
    property string title
    property bool hasTarget: false
    property bool expanded: false
    property bool following: false
    property bool picking: false
    signal followRequested
    signal pickingRequested
    signal expandRequested
    signal detailsRequested
    signal navigationStarted
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
        icon.source: "qrc:/icons/crosshair.svg"
        tip: qsTr("跟随卫星")
        checkable: true
        checked: toolbar.following
        enabled: toolbar.hasTarget
        onClicked: toolbar.followRequested()
    }
    IconButton {
        icon.source: "qrc:/icons/minus.svg"
        tip: qsTr("缩小")
        enabled: toolbar.map.zoom > 1
        onClicked: {
            toolbar.navigationStarted();
            toolbar.map.zoomAt(1 / 1.5, toolbar.map.width / 2, toolbar.map.height / 2);
        }
    }
    Label {
        text: toolbar.map.zoom.toFixed(1) + "×"
        font.family: Theme.numberFont
        font.pixelSize: 12
        Layout.preferredWidth: 38
        horizontalAlignment: Text.AlignHCenter
    }
    IconButton {
        icon.source: "qrc:/icons/plus.svg"
        tip: qsTr("放大")
        enabled: toolbar.map.zoom < 12
        onClicked: {
            toolbar.navigationStarted();
            toolbar.map.zoomAt(1.5, toolbar.map.width / 2, toolbar.map.height / 2);
        }
    }
    IconButton {
        icon.source: "qrc:/icons/globe.svg"
        tip: qsTr("全球视图")
        onClicked: {
            toolbar.navigationStarted();
            toolbar.map.resetView();
        }
    }
    IconButton {
        icon.source: "qrc:/icons/map-pin.svg"
        tip: qsTr("地图选点")
        checkable: true
        checked: toolbar.picking
        onClicked: toolbar.pickingRequested()
    }
    IconButton {
        icon.source: toolbar.expanded ? "qrc:/icons/minimize-2.svg" : "qrc:/icons/maximize-2.svg"
        tip: toolbar.expanded ? qsTr("收起地图") : qsTr("展开地图")
        onClicked: toolbar.expandRequested()
    }
}
