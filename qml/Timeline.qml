import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Rectangle {
    id: timeline
    required property AppState clock
    required property SatelliteModel satellites
    required property Action nowAction
    implicitHeight: 94
    color: Theme.surface
    function fraction(time) {
        return (time - clock.referenceTime + 43200) / 86400;
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 6
        anchors.bottomMargin: 7
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Slider {
                id: slider
                Accessible.name: qsTr("观测时刻")
                Accessible.description: timeline.clock.timeText + " · " + timeline.clock.timeZoneName
                Layout.fillWidth: true
                implicitHeight: 53
                from: -720
                to: 720
                stepSize: 1 / 60
                topPadding: 18
                bottomPadding: 4
                leftPadding: 7
                rightPadding: 7
                value: (timeline.clock.unixTime - timeline.clock.referenceTime) / 60
                onMoved: timeline.clock.seek(timeline.clock.referenceTime + value * 60)
                background: Rectangle {
                    x: slider.leftPadding + 6
                    y: slider.topPadding + slider.availableHeight / 2
                    width: slider.availableWidth - 12
                    height: 2
                    color: Theme.line
                    Rectangle {
                        property real fraction: timeline.fraction(timeline.clock.nowTime)
                        visible: fraction >= 0 && fraction <= 1
                        x: fraction * parent.width
                        y: -8
                        width: 1
                        height: 18
                        color: Theme.muted
                    }
                }
                handle: Item {
                    width: 12
                    height: 28
                    x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                    y: slider.topPadding + (slider.availableHeight - height) / 2
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 3
                        width: 2
                        height: 25
                        color: Theme.accent
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 0
                        width: 12
                        height: 5
                        radius: 1
                        color: Theme.accent
                    }
                }
                Label {
                    text: timeline.clock.live ? qsTr("现在 · ") + timeline.clock.formatTime(timeline.clock.unixTime, "HH:mm:ss") : timeline.clock.formatTime(timeline.clock.unixTime, "MM-dd HH:mm:ss")
                    x: Math.max(0, Math.min(slider.width - width, slider.handle.x + 6 - width / 2))
                    y: 0
                    font.family: Theme.numberFont
                    font.pixelSize: 12
                    color: Theme.accent
                }
                ToolTip.visible: pressed
                ToolTip.text: timeline.clock.formatTime(timeline.clock.unixTime, "MM-dd HH:mm:ss")
            }
            Item {
                id: events
                Layout.fillWidth: true
                Layout.leftMargin: 13
                Layout.rightMargin: 13
                implicitHeight: 10
                clip: true
                Repeater {
                    model: timeline.satellites.passes
                    Rectangle {
                        required property var modelData
                        x: Math.max(0, timeline.fraction(modelData.start)) * events.width
                        width: Math.max(3, (Math.min(1, timeline.fraction(modelData.end)) - Math.max(0, timeline.fraction(modelData.start))) * events.width)
                        height: 3
                        y: 0
                        radius: 1
                        color: Theme.accent
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -3
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: qsTr("过境 ") + timeline.clock.formatTime(parent.modelData.start, "HH:mm:ss") + " - " + timeline.clock.formatTime(parent.modelData.end, "HH:mm:ss") + qsTr("\n峰值 ") + parent.modelData.maximum
                            onClicked: timeline.clock.seek(parent.modelData.peak)
                        }
                    }
                }
                Repeater {
                    model: timeline.satellites.shadowEvents
                    Rectangle {
                        required property var modelData
                        x: timeline.fraction(modelData.time) * events.width - 2
                        width: 4
                        height: 4
                        y: 5
                        radius: 2
                        color: modelData.entering ? Theme.past : Theme.shadow
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -2
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: parent.modelData.name + " · " + timeline.clock.formatTime(parent.modelData.time, "HH:mm:ss")
                            onClicked: timeline.clock.seek(parent.modelData.time)
                        }
                    }
                }
            }
            Item {
                Layout.fillWidth: true
                implicitHeight: 15
                Label {
                    anchors.left: parent.left
                    text: timeline.clock.startTimeText
                    color: Theme.muted
                    font.family: Theme.numberFont
                    font.pixelSize: 11
                }
                Label {
                    anchors.right: parent.right
                    text: timeline.clock.endTimeText
                    color: Theme.muted
                    font.family: Theme.numberFont
                    font.pixelSize: 11
                }
                Label {
                    visible: !timeline.clock.live && timeline.fraction(timeline.clock.nowTime) >= 0 && timeline.fraction(timeline.clock.nowTime) <= 1
                    x: Math.max(70, Math.min(parent.width - width - 70, timeline.fraction(timeline.clock.nowTime) * parent.width - width / 2))
                    text: qsTr("现在")
                    color: Theme.muted
                    font.pixelSize: 11
                }
            }
        }
        Button {
            action: timeline.nowAction
            icon.color: Theme.text
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: text
        }
    }
}
