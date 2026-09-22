import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Rectangle {
    id: sidebar
    required property SatelliteModel satellites
    signal importRequested
    signal targetActivated
    color: Theme.surface
    function groupIndex() {
        for (let i = 0; i < satellites.groups.length; ++i)
            if (satellites.groups[i].key === satellites.group)
                return i;
        return 0;
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        RowLayout {
            Label {
                text: "卫星目录"
                font.bold: true
            }
            Label {
                text: sidebar.satellites.total
                font.family: Theme.numberFont
                color: Theme.muted
                font.pixelSize: 12
            }
            Item {
                Layout.fillWidth: true
            }
            IconButton {
                icon.source: "qrc:/icons/folder-open.svg"
                tip: "导入轨道文件"
                onClicked: sidebar.importRequested()
            }
            IconButton {
                icon.source: "qrc:/icons/refresh-cw.svg"
                tip: "更新轨道数据"
                enabled: !sidebar.satellites.downloading && sidebar.satellites.group !== "local"
                onClicked: sidebar.satellites.refresh()
            }
        }
        ComboBox {
            Layout.fillWidth: true
            model: sidebar.satellites.groups
            textRole: "name"
            valueRole: "key"
            currentIndex: sidebar.groupIndex()
            onActivated: sidebar.satellites.group = currentValue
        }
        Button {
            Layout.fillWidth: true
            text: "载入全部活动卫星"
            font.pixelSize: 12
            icon.source: "qrc:/icons/globe.svg"
            icon.color: Theme.text
            enabled: sidebar.satellites.group !== "active" && !sidebar.satellites.downloading
            onClicked: sidebar.satellites.group = "active"
        }
        TextField {
            Layout.fillWidth: true
            placeholderText: "名称、编号或国际编号"
            selectByMouse: true
            onTextChanged: sidebar.satellites.search = text
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
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                visible: list.count === 0
                anchors.centerIn: parent
                text: sidebar.satellites.downloading ? "正在获取卫星目录" : "暂无匹配目标"
                color: Theme.muted
                font.pixelSize: 12
            }
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
