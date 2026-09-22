import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Rectangle {
    id: sidebar
    required property SatelliteModel satellites
    required property CatalogModel catalog
    signal catalogRequested
    signal groupRequested(string key)
    signal targetActivated
    color: Theme.surface
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        Button {
            Layout.fillWidth: true
            text: "卫星目录 · " + sidebar.satellites.catalogCount
            icon.source: "qrc:/icons/folder-open.svg"
            icon.color: Theme.text
            onClicked: sidebar.catalogRequested()
        }
        TextField {
            Layout.fillWidth: true
            placeholderText: "搜索观测清单"
            selectByMouse: true
            onTextChanged: sidebar.satellites.search = text
        }
        RowLayout {
            Label {
                text: "观测清单"
                font.bold: true
            }
            Label {
                text: sidebar.satellites.search.length ? sidebar.satellites.visibleCount + "/" + sidebar.satellites.total : sidebar.satellites.total
                color: Theme.muted
                font.family: Theme.numberFont
                font.pixelSize: 12
            }
            Item {
                Layout.fillWidth: true
            }
            IconButton {
                icon.source: "qrc:/icons/plus.svg"
                tip: "从目录加入目标"
                onClicked: sidebar.catalogRequested()
            }
        }
        RowLayout {
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Label {
                text: "卫星 / 编号"
                color: Theme.muted
                font.pixelSize: 11
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: "高度角"
                color: Theme.muted
                font.pixelSize: 11
            }
            Item {
                width: 24
                height: 1
            }
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 54
            Layout.preferredHeight: Math.max(54, Math.min(count, 8) * 54)
            Layout.maximumHeight: Math.max(54, Math.min(count, 8) * 54)
            clip: true
            model: sidebar.satellites
            currentIndex: -1
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: entry
                required property string satelliteId
                required property string satelliteName
                required property string originalName
                required property string elevationText
                width: ListView.view.width
                height: 54
                highlighted: satelliteId === sidebar.satellites.selectedId
                background: Rectangle {
                    color: entry.highlighted ? Theme.selection : entry.hovered ? Theme.hover : "transparent"
                    Rectangle {
                        width: 3
                        height: parent.height
                        color: Theme.accent
                        visible: entry.highlighted
                    }
                }
                contentItem: ColumnLayout {
                    spacing: 3
                    Label {
                        text: entry.satelliteName
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.bold: entry.highlighted
                        font.pixelSize: 13
                    }
                    RowLayout {
                        Label {
                            text: entry.satelliteId
                            color: Theme.muted
                            font.family: Theme.numberFont
                            font.pixelSize: 12
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Label {
                            text: entry.elevationText
                            color: Theme.muted
                            font.family: Theme.numberFont
                            font.pixelSize: 13
                        }
                        IconButton {
                            implicitWidth: 24
                            implicitHeight: 22
                            icon.source: "qrc:/icons/minus.svg"
                            tip: "移出观测清单"
                            opacity: entry.hovered ? 1 : 0
                            enabled: entry.hovered
                            onClicked: sidebar.satellites.setWatched(entry.satelliteId, false)
                        }
                    }
                }
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: originalName + "\n卫星编号 " + satelliteId + "\n高度角 " + elevationText
                onClicked: {
                    sidebar.satellites.select(satelliteId);
                    sidebar.targetActivated();
                }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: sidebar.satellites.downloading ? "正在获取轨道数据" : "观测清单暂无匹配目标"
                color: Theme.muted
                font.pixelSize: 11
            }
        }
        Label {
            text: "GNSS"
            font.bold: true
            font.pixelSize: 13
            Layout.topMargin: 12
        }
        Repeater {
            model: sidebar.catalog.navigationGroups
            ItemDelegate {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 38
                contentItem: RowLayout {
                    Label {
                        text: modelData.name
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    Label {
                        text: modelData.count > 0 ? modelData.count : "待获取"
                        font.family: Theme.numberFont
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                    IconButton {
                        icon.source: "qrc:/icons/crosshair.svg"
                        tip: "预览 " + modelData.name
                        enabled: modelData.count > 0
                        onClicked: sidebar.satellites.previewConstellation(modelData.key)
                    }
                    IconButton {
                        icon.source: "qrc:/icons/chevron-right.svg"
                        tip: "查看成员"
                        onClicked: sidebar.groupRequested(modelData.key)
                    }
                }
                onClicked: sidebar.groupRequested(modelData.key)
            }
        }
        Item {
            Layout.fillHeight: true
        }
        Label {
            text: sidebar.satellites.status
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 11
        }
    }
}
