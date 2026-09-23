import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import Astrochron

ApplicationWindow {
    id: window
    width: 1480
    height: 960
    minimumWidth: 880
    minimumHeight: 720
    visible: true
    title: "ASTROCHRON"
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize
    color: Theme.background
    property bool expanded: false
    readonly property bool compact: width < 1240
    readonly property var passList: satelliteModel.passes
    property var nextPass: {
        const passes = window.passList;
        for (let i = 0; i < passes.length; ++i)
            if (passes[i].end >= appState.unixTime)
                return passes[i];
        return ({});
    }
    property var skyPass: nextPass.start !== undefined ? nextPass : passList.length ? passList[passList.length - 1] : ({})
    function showDetails() {
        if (compact)
            detailDrawer.open();
        else
            expanded = false;
    }
    onCompactChanged: if (!compact)
        detailDrawer.close()

    palette.window: Theme.background
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.background
    palette.mid: Theme.line
    palette.dark: Theme.line
    palette.placeholderText: Theme.muted

    AppState {
        id: appState
    }
    SatelliteModel {
        id: satelliteModel
        clock: appState
    }
    WeatherModel {
        id: weatherModel
        clock: appState
    }
    CatalogModel {
        id: catalogModel
        source: satelliteModel
    }
    Binding {
        target: Theme
        property: "dark"
        value: appState.darkTheme
    }
    Settings {
        id: preferences
        category: "workspace"
        property real sidebarWidth: 224
        property real detailWidth: 340
    }

    header: ColumnLayout {
        spacing: 0
        ToolBar {
            Layout.fillWidth: true
            implicitHeight: 44
            background: Rectangle {
                color: Theme.background
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 12
                Label {
                    text: qsTr("星纪")
                    font.pixelSize: 20
                    font.bold: true
                }
                Label {
                    text: "ASTROCHRON"
                    visible: appState.chinese
                    font.pixelSize: 11
                    color: Theme.muted
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: appState.timeText
                    font.family: Theme.numberFont
                    font.pixelSize: 13
                }
                Label {
                    text: appState.timeZoneName
                    color: Theme.muted
                    font.pixelSize: 11
                }
                IconButton {
                    icon.source: "qrc:/icons/settings-2.svg"
                    tip: qsTr("设置")
                    onClicked: settingsDialog.open()
                }
                IconButton {
                    icon.source: "qrc:/icons/moon.svg"
                    tip: appState.darkTheme ? qsTr("浅色界面") : qsTr("深色界面")
                    onClicked: appState.darkTheme = !appState.darkTheme
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 32
            color: Theme.statusSurface
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 12
                Label {
                    text: appState.hasObserver ? qsTr("观测地点：") + appState.observerName : qsTr("观测地点：待选择")
                    font.bold: true
                    font.pixelSize: 12
                    Layout.maximumWidth: 180
                    elide: Text.ElideRight
                }
                Button {
                    text: qsTr("更换地点")
                    icon.source: "qrc:/icons/map-pin.svg"
                    icon.color: Theme.text
                    implicitHeight: 26
                    font.pixelSize: 12
                    onClicked: observerDialog.openCities()
                }
                Label {
                    visible: appState.hasObserver && window.width >= 1040
                    text: (appState.observerLatitude >= 0 ? qsTr("北纬 ") : qsTr("南纬 ")) + Math.abs(appState.observerLatitude).toFixed(4) + "°"
                    font.family: Theme.numberFont
                    font.pixelSize: 12
                    color: Theme.muted
                }
                Label {
                    visible: appState.hasObserver && window.width >= 1040
                    text: (appState.observerLongitude >= 0 ? qsTr("东经 ") : qsTr("西经 ")) + Math.abs(appState.observerLongitude).toFixed(4) + "°"
                    font.family: Theme.numberFont
                    font.pixelSize: 12
                    color: Theme.muted
                }
                Label {
                    visible: appState.hasObserver
                    text: appState.hasObserverHeight ? qsTr("海拔 ") + appState.observerHeight.toFixed(0) + " m" : appState.elevationBusy ? qsTr("正在查询海拔") : qsTr("海拔待填写")
                    font.pixelSize: 12
                    color: Theme.muted
                }
                Label {
                    text: qsTr("最低高度角 ") + appState.minimumElevation.toFixed(0) + "°"
                    font.pixelSize: 12
                    color: Theme.muted
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }

    SplitView {
        id: workbench
        anchors.fill: parent
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.pressed ? Theme.accent : SplitHandle.hovered ? Theme.line : "transparent"
        }
        onResizingChanged: if (!resizing) {
            preferences.sidebarWidth = sidebar.width;
            if (detailDock.visible)
                preferences.detailWidth = detailDock.width;
        }
        SatelliteSidebar {
            id: sidebar
            satellites: satelliteModel
            catalog: catalogModel
            SplitView.minimumWidth: 200
            SplitView.maximumWidth: 320
            SplitView.preferredWidth: preferences.sidebarWidth
            onCatalogRequested: catalogDialog.openAll()
            onGroupRequested: key => catalogDialog.openGroup(key)
            onTargetActivated: if (window.compact)
                detailDrawer.open()
        }
        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 360
            spacing: 0
            MapWorkspace {
                id: mapWorkspace
                Layout.fillWidth: true
                Layout.fillHeight: true
                clock: appState
                satellites: satelliteModel
                passInfo: window.skyPass
                expanded: window.expanded
                onExpandRequested: window.expanded = !window.expanded
                onDetailsRequested: window.showDetails()
                onTargetActivated: if (window.compact)
                    detailDrawer.open()
            }
            PassTable {
                visible: !window.expanded
                Layout.fillWidth: true
                Layout.preferredHeight: window.height < 820 ? 215 : 270
                Layout.fillHeight: false
                Layout.maximumHeight: window.height < 820 ? 215 : 270
                clock: appState
                satellites: satelliteModel
                nextPass: window.nextPass
            }
        }
        Item {
            id: detailDock
            visible: !window.compact && !window.expanded
            SplitView.minimumWidth: 300
            SplitView.maximumWidth: 420
            SplitView.preferredWidth: preferences.detailWidth
        }
    }
    Drawer {
        id: detailDrawer
        edge: Qt.RightEdge
        width: Math.min(400, window.width - 48)
        y: window.header.height
        height: window.height - y - window.footer.height
        modal: true
        interactive: window.compact
        background: Rectangle {
            color: Theme.background
        }
    }
    ObservationPane {
        parent: window.compact ? detailDrawer.contentItem : detailDock
        anchors.fill: parent
        clock: appState
        satellites: satelliteModel
        weather: weatherModel
        skyPass: window.skyPass
        onExportRequested: exportDialog.open()
        onPhotometryRequested: photometryDialog.open()
    }
    footer: Timeline {
        clock: appState
        satellites: satelliteModel
    }

    ObserverDialog {
        id: observerDialog
        clock: appState
        onLocationApplied: mapWorkspace.centerObserver()
    }
    SettingsDialog {
        id: settingsDialog
        clock: appState
        weather: weatherModel
    }
    CatalogDialog {
        id: catalogDialog
        satellites: satelliteModel
        catalog: catalogModel
        onImportRequested: importDialog.open()
        onInspectionRequested: window.showDetails()
    }
    FileDialog {
        id: importDialog
        title: qsTr("导入轨道文件")
        nameFilters: [qsTr("轨道文件 (*.json *.tle *.txt)"), qsTr("所有文件 (*)")]
        onAccepted: satelliteModel.importFile(selectedFile)
    }
    PhotometryDialog {
        id: photometryDialog
        satellites: satelliteModel
    }
    FileDialog {
        id: exportDialog
        title: qsTr("保存轨道根数")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("轨道根数 (*.json)")]
        defaultSuffix: "json"
        onAccepted: satelliteModel.exportSelected(selectedFile)
    }
    Shortcut {
        sequence: "Escape"
        enabled: window.expanded
        onActivated: window.expanded = false
    }
    Component.onCompleted: if (!appState.hasObserver)
        Qt.callLater(function() { observerDialog.openCities(); })
}
