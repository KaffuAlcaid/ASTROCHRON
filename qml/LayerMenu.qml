import QtQuick
import QtQuick.Controls

Flow {
    id: menu
    property alias past: pastToggle.checked
    property alias future: futureToggle.checked
    property alias satellites: satelliteToggle.checked
    property alias coverage: coverageToggle.checked
    property alias daynight: dayToggle.checked
    property alias cities: cityToggle.checked
    property alias station: stationToggle.checked
    property alias borders: borderToggle.checked
    property alias lakes: lakeToggle.checked
    property alias grid: gridToggle.checked
    spacing: 10
    component LayerToggle: ToolButton {
        id: toggle
        property color swatch: "transparent"
        property bool dashed: false
        checkable: true
        implicitHeight: 26
        leftPadding: 7
        rightPadding: 7
        font.pixelSize: 12
        contentItem: Row {
            spacing: 5
            Item {
                visible: toggle.swatch.a > 0
                width: 12
                height: 16
                Rectangle {
                    visible: !toggle.dashed
                    width: 12
                    height: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: toggle.swatch
                }
                Row {
                    visible: toggle.dashed
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 3
                            height: 2
                            color: toggle.swatch
                        }
                    }
                }
            }
            Text {
                text: toggle.text
                font: toggle.font
                color: toggle.checked ? Theme.text : Theme.muted
            }
        }
        background: Rectangle {
            radius: Theme.radius
            color: toggle.checked ? Theme.selection : toggle.hovered ? Theme.hover : "transparent"
        }
    }
    Row {
        spacing: 3
        Label {
            text: "轨迹"
            color: Theme.muted
            font.pixelSize: 11
            height: 26
            verticalAlignment: Text.AlignVCenter
            rightPadding: 4
        }
        LayerToggle {
            id: pastToggle
            text: "过去"
            checked: true
            swatch: Theme.past
            dashed: true
        }
        LayerToggle {
            id: futureToggle
            text: "未来"
            checked: true
            swatch: Theme.accent
        }
        LayerToggle {
            id: satelliteToggle
            text: "卫星"
            checked: true
        }
        LayerToggle {
            id: coverageToggle
            text: "覆盖范围"
            ToolTip.visible: hovered
            ToolTip.text: "按最低高度角计算，采用球面近似"
        }
    }
    Row {
        spacing: 3
        Label {
            text: "地图"
            color: Theme.muted
            font.pixelSize: 11
            height: 26
            verticalAlignment: Text.AlignVCenter
            rightPadding: 4
        }
        LayerToggle {
            id: dayToggle
            text: "昼夜线"
            checked: true
        }
        LayerToggle {
            id: cityToggle
            text: "城市"
            checked: true
        }
        LayerToggle {
            id: stationToggle
            text: "地点"
            checked: true
        }
        LayerToggle {
            id: borderToggle
            text: "国界"
            checked: true
        }
        LayerToggle {
            id: lakeToggle
            text: "湖泊"
            checked: true
        }
        LayerToggle {
            id: gridToggle
            text: "经纬网"
        }
    }
}
