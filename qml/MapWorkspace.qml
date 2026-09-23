import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Astrochron

ColumnLayout {
    id: workspace
    required property AppState clock
    required property SatelliteModel satellites
    required property Action nowAction
    required property Action expandAction
    property var passInfo: ({})
    property bool expanded: false
    property alias following: mapPreferences.following
    property alias zoom: worldMap.zoom
    property bool picking: false
    property var observation: satellites.observation
    signal detailsRequested
    signal targetActivated
    spacing: 0
    Settings {
        id: mapPreferences
        category: "map"
        property bool following: true
    }
    function followTarget() {
        if (!following) return;
        const point = orbitLayer.selectedCoordinate(clock.unixTime);
        if (isFinite(point.x) && isFinite(point.y)) worldMap.centerOn(point.x, point.y);
    }
    onFollowingChanged: if (following) Qt.callLater(followTarget)
    Action {
        id: mapFollowAction
        text: qsTr("跟随卫星")
        icon.source: "qrc:/icons/crosshair.svg"
        enabled: workspace.satellites.selectedId !== "0" && workspace.clock.hasObserver
        onTriggered: {
            workspace.picking = false;
            workspace.following = true;
            worldMap.zoom = 9;
            workspace.followTarget();
        }
    }
    Action {
        id: mapZoomInAction
        text: qsTr("放大")
        icon.source: "qrc:/icons/plus.svg"
        enabled: worldMap.zoom < 12
        onTriggered: worldMap.zoomAt(1.5, worldMap.width / 2, worldMap.height / 2)
    }
    Action {
        id: mapZoomOutAction
        text: qsTr("缩小")
        icon.source: "qrc:/icons/minus.svg"
        enabled: worldMap.zoom > 1
        onTriggered: worldMap.zoomAt(1 / 1.5, worldMap.width / 2, worldMap.height / 2)
    }
    Action {
        id: mapGlobalAction
        text: qsTr("全球视图")
        icon.source: "qrc:/icons/globe.svg"
        onTriggered: { workspace.following = false; worldMap.resetView(); }
    }
    Action {
        id: mapPickAction
        text: qsTr("地图选点")
        icon.source: "qrc:/icons/map-pin.svg"
        checkable: true
        checked: workspace.picking
        onTriggered: { workspace.picking = checked; if (checked) workspace.following = false; }
    }
    Keys.priority: Keys.AfterItem
    Keys.onPressed: event => {
        const modifiers = event.modifiers & ~(Qt.KeypadModifier | Qt.ShiftModifier);
        if (modifiers !== Qt.NoModifier) return;
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) mapZoomInAction.trigger();
        else if (event.key === Qt.Key_Minus) mapZoomOutAction.trigger();
        else if (event.key === Qt.Key_F) mapFollowAction.trigger();
        else if (event.key === Qt.Key_Space) workspace.nowAction.trigger();
        else return;
        event.accepted = true;
    }
    function centerObserver() {
        following = false;
        worldMap.centerOn(clock.observerLongitude, clock.observerLatitude);
    }
    Connections {
        target: workspace.clock
        function onLocalizedChanged() { worldMap.updateLabels(); }
        function onTimeChanged() { workspace.followTarget(); }
    }
    Connections {
        target: workspace.satellites
        function onFrameChanged() {
            Qt.callLater(workspace.followTarget);
        }
    }
    RowLayout {
        visible: workspace.satellites.previewActive
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 8
        Layout.topMargin: 4
        Label {
            text: qsTr("预览：") + workspace.satellites.previewName + " · " + workspace.satellites.previewTotal
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        ComboBox {
            model: [qsTr("高于最低高度角"), qsTr("未来 15 min 过境"), qsTr("显示全部位置")]
            currentIndex: workspace.satellites.previewMode
            implicitWidth: 160
            implicitHeight: 28
            font.pixelSize: 12
            onActivated: workspace.satellites.previewMode = currentIndex
        }
        Label {
            text: workspace.satellites.previewBusy ? qsTr("计算中") : workspace.satellites.previewCount + qsTr(" 个")
            color: Theme.muted
            font.pixelSize: 11
        }
        IconButton {
            icon.source: "qrc:/icons/x.svg"
            tip: qsTr("结束星座预览")
            onClicked: workspace.satellites.closePreview()
        }
    }
    MapToolbar {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 8
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        map: worldMap
        title: workspace.observation.name || qsTr("全球地图")
        hasTarget: workspace.satellites.selectedId !== "0"
        expanded: workspace.expanded
        following: workspace.following
        followAction: mapFollowAction
        zoomInAction: mapZoomInAction
        zoomOutAction: mapZoomOutAction
        globalViewAction: mapGlobalAction
        pickAction: mapPickAction
        expandAction: workspace.expandAction
        onDetailsRequested: workspace.detailsRequested()
    }
    Rectangle {
        id: mapViewport
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 170
        color: Theme.background
        clip: true
        activeFocusOnTab: true
        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("卫星地图")
        Accessible.description: workspace.observation.name || qsTr("全球地图")
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height * 2 * worldMap.zoom)
            height: Math.min(parent.height, parent.width / 2 * worldMap.zoom)
            color: Theme.ocean
            clip: true
            WorldMap {
                id: worldMap
                anchors.fill: parent
                showLakes: layers.lakes
                showBorders: layers.borders
                showGrid: layers.grid
                showCities: layers.cities
                showStation: layers.station && workspace.clock.hasObserver
                observerLongitude: workspace.clock.observerLongitude
                observerLatitude: workspace.clock.observerLatitude
                landColor: Theme.land
                waterColor: Theme.ocean
                borderColor: Theme.border
                gridColor: Theme.grid
            }
            ShaderEffect {
                anchors.fill: parent
                visible: layers.daynight
                property vector2d viewSize: Qt.vector2d(width, height)
                property real pixelsPerDegree: worldMap.pixelsPerDegree
                property real centerLongitude: worldMap.centerLongitude
                property real centerLatitude: worldMap.centerLatitude
                property vector3d sunDirection: workspace.clock.sunDirection
                property color nightColor: Theme.dark ? "#66081117" : "#302d4249"
                property color lineColor: Theme.dark ? "#b3c49a69" : "#a6b57a36"
                fragmentShader: "qrc:/shaders/daynight.frag.qsb"
            }
            OrbitLayer {
                id: gnssLayer
                anchors.fill: parent
                visible: layers.gnss
                map: worldMap
                markers: workspace.satellites.gnssMarkers
                time: workspace.clock.unixTime
                markerColor: Theme.marker
                z: 1
            }
            OrbitLayer {
                id: orbitLayer
                anchors.fill: parent
                z: 1
                map: worldMap
                markers: workspace.satellites.markers
                trajectory: workspace.satellites.trajectory
                selectedId: workspace.satellites.selectedId
                time: workspace.clock.unixTime
                minimumElevation: workspace.clock.minimumElevation
                highlightStart: workspace.passInfo.start || 0
                highlightEnd: workspace.passInfo.end || 0
                showPast: layers.past
                showFuture: layers.future
                showCoverage: layers.coverage
                showSatellites: layers.satellites
                pastColor: Theme.past
                futureColor: Theme.accent
                markerColor: Theme.marker
            }
            Repeater {
                model: worldMap.cityLabels
                delegate: Item {
                    required property var modelData
                    x: worldMap.labelOffset.x
                    y: worldMap.labelOffset.y
                    z: 2
                    Rectangle {
                        x: modelData.pointX - 1
                        y: modelData.pointY - 1
                        width: 2
                        height: 2
                        radius: 1
                        color: Theme.city
                    }
                    Text {
                        x: modelData.x
                        y: modelData.y
                        text: modelData.name
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        color: Theme.city
                        style: Text.Outline
                        styleColor: Theme.alpha(Theme.ocean, 0.65)
                    }
                }
            }
            Text {
                z: 3
                visible: workspace.observation.name !== undefined && orbitLayer.selectedPosition.x >= 0 && orbitLayer.selectedPosition.x <= parent.width && orbitLayer.selectedPosition.y >= 0 && orbitLayer.selectedPosition.y <= parent.height
                x: Math.max(4, Math.min(parent.width - width - 4, orbitLayer.selectedPosition.x + 9))
                y: orbitLayer.selectedPosition.y - height - 4
                text: workspace.observation.name || ""
                color: Theme.accent
                font.bold: true
                font.pixelSize: 13
                style: Text.Outline
                styleColor: Theme.ocean
            }
            Rectangle {
                z: 3
                visible: layers.station && workspace.clock.hasObserver
                x: worldMap.observerPosition.x - width / 2
                y: worldMap.observerPosition.y - height / 2
                width: 8
                height: 8
                rotation: 45
                color: Theme.past
                border.width: 1
                border.color: Theme.background
            }
            MouseArea {
                z: 4
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: workspace.picking ? Qt.CrossCursor : pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property real previousX
                property real previousY
                property real dragDistance: 0
                onPressed: mouse => {
                    mapViewport.forceActiveFocus(Qt.MouseFocusReason);
                    previousX = mouse.x;
                    previousY = mouse.y;
                    dragDistance = 0;
                }
                onPositionChanged: mouse => {
                    if (pressed && !workspace.picking) {
                        dragDistance += Math.hypot(mouse.x - previousX, mouse.y - previousY);
                        if (dragDistance > 3)
                            workspace.following = false;
                        worldMap.panBy(mouse.x - previousX, mouse.y - previousY);
                        previousX = mouse.x;
                        previousY = mouse.y;
                    }
                }
                onClicked: mouse => {
                    if (workspace.picking) {
                        const point = worldMap.coordinateAt(mouse.x, mouse.y);
                        workspace.clock.setObserver(qsTr("地图选点"), point.y, point.x, NaN, workspace.clock.timeZone);
                        workspace.picking = false;
                    } else if (dragDistance < 4) {
                        let id = orbitLayer.satelliteAt(mouse.x, mouse.y);
                        if (!id.length && layers.gnss)
                            id = gnssLayer.satelliteAt(mouse.x, mouse.y);
                        if (id.length) {
                            workspace.satellites.select(id);
                            workspace.targetActivated();
                        }
                    }
                }
                onWheel: wheel => {
                    worldMap.zoomAt(Math.pow(1.0015, wheel.angleDelta.y), workspace.following ? width / 2 : wheel.x, workspace.following ? height / 2 : wheel.y);
                    wheel.accepted = true;
                }
                onDoubleClicked: mouse => {
                    worldMap.zoomAt(1.5, workspace.following ? width / 2 : mouse.x, workspace.following ? height / 2 : mouse.y);
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: mapViewport.activeFocus ? 1 : 0
            border.color: Theme.accent
            enabled: false
        }
    }
    LayerMenu {
        id: layers
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 10
        Layout.topMargin: 7
        Layout.bottomMargin: 7
    }
    Component.onCompleted: {
        if (following) Qt.callLater(followTarget);
        else worldMap.centerOn(clock.observerLongitude, clock.observerLatitude);
    }
}
