import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

ColumnLayout {
    id: workspace
    required property AppState clock
    required property SatelliteModel satellites
    property var passInfo: ({})
    property bool expanded: false
    property bool following: false
    property bool picking: false
    property var observation: satellites.observation
    signal expandRequested
    signal detailsRequested
    signal targetActivated
    spacing: 0
    function centerObserver() {
        following = false;
        worldMap.centerOn(clock.observerLongitude, clock.observerLatitude);
    }
    Connections {
        target: workspace.satellites
        function onFrameChanged() {
            if (workspace.following && workspace.observation.longitude !== undefined)
                worldMap.centerOn(workspace.observation.longitude, workspace.observation.latitude);
        }
    }
    MapToolbar {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 8
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        map: worldMap
        title: workspace.observation.name || "全球地图"
        hasTarget: workspace.satellites.selectedId !== "0"
        expanded: workspace.expanded
        following: workspace.following
        picking: workspace.picking
        onFollowRequested: {
            workspace.following = !workspace.following;
            if (workspace.following && workspace.observation.longitude !== undefined)
                worldMap.centerOn(workspace.observation.longitude, workspace.observation.latitude);
        }
        onNavigationStarted: workspace.following = false
        onPickingRequested: workspace.picking = !workspace.picking
        onExpandRequested: workspace.expandRequested()
        onDetailsRequested: workspace.detailsRequested()
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 170
        color: Theme.ocean
        clip: true
        WorldMap {
            id: worldMap
            anchors.fill: parent
            showLakes: layers.lakes
            showBorders: layers.borders
            showGrid: layers.grid
            showCities: layers.cities
            showStation: layers.station
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
            visible: layers.station
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
                    workspace.clock.setObserver("地图选点", point.y, point.x, NaN, workspace.clock.timeZone);
                    workspace.picking = false;
                } else if (dragDistance < 4) {
                    const id = orbitLayer.satelliteAt(mouse.x, mouse.y);
                    if (id.length) {
                        workspace.satellites.select(id);
                        workspace.targetActivated();
                    }
                }
            }
            onWheel: wheel => {
                workspace.following = false;
                worldMap.zoomAt(Math.pow(1.0015, wheel.angleDelta.y), wheel.x, wheel.y);
                wheel.accepted = true;
            }
            onDoubleClicked: mouse => {
                workspace.following = false;
                worldMap.zoomAt(1.5, mouse.x, mouse.y);
            }
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
    Component.onCompleted: centerObserver()
}
