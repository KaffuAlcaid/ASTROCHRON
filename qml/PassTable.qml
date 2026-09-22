import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

ColumnLayout {
    id: table
    required property AppState clock
    required property SatelliteModel satellites
    property var nextPass: ({})
    spacing: 6
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        Layout.topMargin: 8
        Label {
            text: "过境预报"
            font.bold: true
            font.pixelSize: 13
        }
        Label {
            text: table.satellites.passes.length
            font.family: Theme.numberFont
            font.pixelSize: 12
            color: Theme.muted
        }
        Item {
            Layout.fillWidth: true
        }
        Label {
            text: "最低高度角 " + table.clock.minimumElevation.toFixed(0) + "°"
            font.pixelSize: 11
            color: Theme.muted
        }
    }
    ItemDelegate {
        id: nextEntry
        visible: table.nextPass.start !== undefined
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        implicitHeight: nextSummary.implicitHeight + 14
        background: Rectangle {
            color: nextEntry.hovered ? Theme.hover : "transparent"
        }
        contentItem: Flow {
            id: nextSummary
            spacing: 10
            Label {
                text: table.clock.unixTime >= (table.nextPass.start || Infinity) ? "正在过境" : "下一次"
                font.bold: true
                color: Theme.accent
                font.pixelSize: 13
            }
            Label {
                text: table.clock.formatTime(table.nextPass.start || 0, "HH:mm") + " → " + table.clock.formatTime(table.nextPass.end || 0, "HH:mm")
                font.family: Theme.numberFont
                font.pixelSize: 15
            }
            Label {
                text: "峰值 " + (table.nextPass.maximum || "") + " @ " + table.clock.formatTime(table.nextPass.peak || 0, "HH:mm")
                font.family: Theme.numberFont
                font.pixelSize: 13
            }
            Label {
                text: table.nextPass.duration || ""
                font.family: Theme.numberFont
                font.pixelSize: 13
            }
            Label {
                visible: table.nextPass.hasOptical === true
                text: "具备光学条件"
                font.pixelSize: 11
                color: Theme.accent
            }
        }
        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
        onClicked: table.clock.seek(table.nextPass.peak)
    }
    Row {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        Repeater {
            model: ["开始", "最高", "结束", "持续", "光学条件"]
            Label {
                required property string modelData
                width: parent.width / 5
                text: modelData
                font.pixelSize: 11
                color: Theme.muted
            }
        }
    }
    ListView {
        id: passes
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: table.satellites.passes
        ScrollBar.vertical: ScrollBar {}
        delegate: ItemDelegate {
            id: row
            required property var modelData
            property bool selected: table.clock.unixTime >= modelData.start && table.clock.unixTime <= modelData.end
            width: ListView.view.width
            height: 68
            leftPadding: 12
            rightPadding: 12
            background: Rectangle {
                color: row.selected ? Theme.selection : row.hovered ? Theme.hover : "transparent"
            }
            contentItem: Row {
                Column {
                    width: parent.width / 5
                    spacing: 2
                    Label {
                        text: (row.modelData.startClipped ? "早于 " : "") + table.clock.formatTime(row.modelData.start, "MM-dd")
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 11
                    }
                    Label {
                        text: table.clock.formatTime(row.modelData.start, "HH:mm:ss")
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    Label {
                        text: row.modelData.startAz
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 11
                    }
                }
                Column {
                    width: parent.width / 5
                    spacing: 2
                    Label {
                        text: table.clock.formatTime(row.modelData.peak, "MM-dd")
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 11
                    }
                    Label {
                        text: table.clock.formatTime(row.modelData.peak, "HH:mm:ss")
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    Label {
                        text: row.modelData.maximum
                        color: Theme.accent
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                }
                Column {
                    width: parent.width / 5
                    spacing: 2
                    Label {
                        text: (row.modelData.endClipped ? "晚于 " : "") + table.clock.formatTime(row.modelData.end, "MM-dd")
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 11
                    }
                    Label {
                        text: table.clock.formatTime(row.modelData.end, "HH:mm:ss")
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    Label {
                        text: row.modelData.endAz
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 11
                    }
                }
                Column {
                    width: parent.width / 5
                    spacing: 5
                    Label {
                        text: row.modelData.duration
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    Label {
                        text: row.modelData.range
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 12
                    }
                }
                Label {
                    width: parent.width / 5
                    wrapMode: Text.Wrap
                    text: row.modelData.hasOptical ? "光照适宜" : "条件欠佳"
                    color: row.modelData.hasOptical ? Theme.accent : Theme.muted
                    font.pixelSize: 12
                }
            }
            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: "光照观测时段：" + modelData.optical + "\n最高点距离 " + modelData.range
            onClicked: table.clock.seek(modelData.peak)
        }
        Label {
            anchors.centerIn: parent
            visible: passes.count === 0
            text: table.satellites.calculating ? "正在计算过境" : "本时段内暂无满足高度角条件的过境"
            color: Theme.muted
            font.pixelSize: 12
        }
    }
}
