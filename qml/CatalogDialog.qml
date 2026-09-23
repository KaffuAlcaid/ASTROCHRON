import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property SatelliteModel satellites
    required property CatalogModel catalog
    signal importRequested
    signal inspectionRequested
    title: qsTr("卫星目录")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(920, Overlay.overlay ? Overlay.overlay.width - 32 : 920)
    height: Math.min(760, Overlay.overlay ? Overlay.overlay.height - 32 : 760)
    footer: DialogButtonBox {
        Button { text: qsTr("关闭"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        onRejected: dialog.close()
    }
    function groupIndex() {
        for (let i = 0; i < satellites.groups.length; ++i)
            if (satellites.groups[i].key === satellites.group)
                return i;
        return 0;
    }
    function openAll() {
        satellites.group = "catalog";
        catalog.search = "";
        open();
    }
    function openGroup(key) {
        catalog.revealGroup(key);
        open();
    }
    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            Label {
                text: qsTr("目录 ") + dialog.satellites.catalogCount + qsTr(" · 观测清单 ") + dialog.satellites.total
                color: Theme.muted
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            IconButton {
                icon.source: "qrc:/icons/folder-open.svg"
                tip: qsTr("导入轨道文件")
                onClicked: dialog.importRequested()
            }
            IconButton {
                icon.source: "qrc:/icons/refresh-cw.svg"
                tip: dialog.satellites.group === "catalog" ? qsTr("更新活动卫星来源") : qsTr("更新目录分组")
                enabled: !dialog.satellites.downloading && dialog.satellites.group !== "local"
                onClicked: dialog.satellites.refresh()
            }
        }
        RowLayout {
            ComboBox {
                Layout.preferredWidth: 210
                model: dialog.satellites.groups
                textRole: "name"
                valueRole: "key"
                currentIndex: dialog.groupIndex()
                enabled: !dialog.satellites.downloading
                onActivated: {
                    dialog.catalog.search = "";
                    dialog.satellites.group = currentValue;
                }
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("搜索名称、编号或国际编号")
                text: dialog.catalog.search
                selectByMouse: true
                onTextEdited: dialog.catalog.search = text
            }
        }
        Label {
            text: dialog.satellites.groupInfo.description || ""
            color: Theme.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            visible: dialog.satellites.group !== "catalog"
            text: dialog.satellites.groupInfo.loaded ? qsTr("来源记录 ") + dialog.satellites.groupInfo.count + qsTr(" · 获取时间 ") + dialog.satellites.groupInfo.acquired
                : dialog.satellites.groupInfo.localMembers ? qsTr("来源分组待获取 · 列表按本地目录归类") : qsTr("分组资料待获取")
            color: Theme.muted
            font.pixelSize: 11
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            visible: dialog.satellites.groupInfo.source !== undefined && dialog.satellites.groupInfo.source !== ""
            text: dialog.satellites.groupInfo.source || ""
            color: Theme.muted
            font.pixelSize: 11
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            ToolTip.visible: sourceHover.hovered
            ToolTip.text: text
            HoverHandler { id: sourceHover }
        }
        RowLayout {
            Layout.leftMargin: 28
            Layout.rightMargin: 62
            Label {
                text: qsTr("星座 / 卫星")
                color: Theme.muted
                font.pixelSize: 11
                Layout.fillWidth: true
            }
            Label {
                text: "NORAD ID"
                color: Theme.muted
                font.pixelSize: 11
                Layout.preferredWidth: 82
                horizontalAlignment: Text.AlignRight
            }
            Label {
                text: "PRN"
                color: Theme.muted
                font.pixelSize: 11
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
            Label {
                text: qsTr("轨道面")
                color: Theme.muted
                font.pixelSize: 11
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignRight
            }
            Label {
                text: qsTr("升交点赤经")
                color: Theme.muted
                font.pixelSize: 11
                Layout.preferredWidth: 96
                horizontalAlignment: Text.AlignRight
            }
        }
        ListView {
            id: entries
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.catalog
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: entry
                required property string entryKey
                required property string entryName
                required property int memberCount
                required property bool isGroup
                required property bool expanded
                required property bool watched
                required property string catalogNumber
                required property string prn
                required property string plane
                required property string nodeLongitude
                width: ListView.view.width
                height: isGroup ? 42 : 40
                leftPadding: isGroup ? 6 : 28
                background: Rectangle {
                    color: entry.hovered ? Theme.hover : entry.isGroup ? Theme.surface : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 8
                    IconButton {
                        visible: entry.isGroup
                        icon.source: entry.expanded ? "qrc:/icons/chevron-down.svg" : "qrc:/icons/chevron-right.svg"
                        tip: entry.expanded ? qsTr("收起成员") : qsTr("展开成员")
                        onClicked: dialog.catalog.toggleGroup(entry.entryKey)
                    }
                    Label {
                        text: entry.entryName
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.bold: entry.isGroup
                        font.pixelSize: 13
                    }
                    Label {
                        visible: entry.isGroup
                        text: (dialog.catalog.search.trim().length ? qsTr("匹配 ") : dialog.satellites.group === "catalog" || dialog.satellites.groupInfo.localMembers ? qsTr("目录 ") : qsTr("组内 ")) + entry.memberCount
                        color: Theme.muted
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    Label {
                        visible: !entry.isGroup
                        text: entry.catalogNumber
                        Layout.preferredWidth: 82
                        horizontalAlignment: Text.AlignRight
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    Label {
                        visible: !entry.isGroup
                        text: entry.prn
                        Layout.preferredWidth: 48
                        horizontalAlignment: Text.AlignRight
                        font.family: Theme.numberFont
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                    Label {
                        visible: !entry.isGroup
                        text: entry.plane
                        Layout.preferredWidth: 48
                        horizontalAlignment: Text.AlignRight
                        font.family: Theme.numberFont
                        font.pixelSize: 12
                        color: Theme.muted
                    }
                    Label {
                        visible: !entry.isGroup
                        text: entry.nodeLongitude
                        Layout.preferredWidth: 96
                        horizontalAlignment: Text.AlignRight
                        font.family: Theme.numberFont
                        font.pixelSize: 13
                    }
                    IconButton {
                        visible: entry.isGroup
                        icon.source: "qrc:/icons/crosshair.svg"
                        tip: qsTr("预览星座观测子集")
                        onClicked: {
                            dialog.satellites.previewConstellation(entry.entryKey);
                            dialog.close();
                        }
                    }
                    CheckBox {
                        visible: !entry.isGroup
                        checked: entry.watched
                        implicitWidth: 34
                        implicitHeight: 28
                        Accessible.name: entry.watched ? qsTr("移出观测清单") : qsTr("加入观测清单")
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        onToggled: dialog.satellites.setWatched(entry.entryKey, checked)
                    }
                }
                ToolTip.visible: hovered && !isGroup
                ToolTip.delay: 600
                ToolTip.text: entryName + "\nNORAD " + catalogNumber + " · PRN " + prn + qsTr("\n轨道面 ") + plane + qsTr(" · 升交点赤经 ") + nodeLongitude
                onClicked: {
                    if (isGroup)
                        dialog.catalog.toggleGroup(entryKey);
                    else {
                        dialog.satellites.select(entryKey);
                        dialog.close();
                        dialog.inspectionRequested();
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: entries.count === 0
                text: dialog.catalog.search.trim().length ? qsTr("暂无匹配目标")
                    : dialog.satellites.downloading ? qsTr("正在获取目录分组")
                    : dialog.satellites.groupInfo.localMembers ? qsTr("本地目录中暂无该星座的对象")
                    : dialog.satellites.group !== "catalog" && !dialog.satellites.groupInfo.loaded ? qsTr("分组资料待获取") : qsTr("本组暂无目标")
                color: Theme.muted
            }
        }
        Label {
            text: dialog.satellites.status
            font.pixelSize: 11
            color: Theme.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
    }
}
