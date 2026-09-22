import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Astrochron

ApplicationWindow {
    id: window
    width: 1480
    height: 960
    minimumWidth: 1100
    minimumHeight: 760
    visible: true
    title: "ASTROCHRON · 星纪"
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 13
    property bool expanded: false
    property bool choosingLocation: false
    property bool following: false
    property color backgroundColor: appState.darkTheme ? "#202628" : "#ffffff"
    property color surfaceColor: appState.darkTheme ? "#292f31" : "#f3f5f5"
    property color textColor: appState.darkTheme ? "#e4ebed" : "#263135"
    property color mutedColor: appState.darkTheme ? "#a8b7bc" : "#64757b"
    property color lineColor: appState.darkTheme ? "#414e52" : "#d6dfe1"
    property color accentColor: appState.darkTheme ? "#55c8ae" : "#087e6f"
    property color pastColor: appState.darkTheme ? "#c59a6e" : "#b47741"
    property color oceanColor: appState.darkTheme ? "#233136" : "#eaf1f3"
    property var cityResults: []
    property var obs: satellites.observation
    property bool hasSatellite: satellites.selectedId !== "0"
    property var skyPass: {
        const passes = satellites.passes;
        for (let i = 0; i < passes.length; ++i)
            if (passes[i].end >= appState.unixTime) return passes[i];
        return passes.length ? passes[passes.length - 1] : ({});
    }

    function value(key, digits, suffix) {
        return obs[key] === undefined ? "待计算" : Number(obs[key]).toFixed(digits) + (suffix || "");
    }
    function groupIndex() {
        for (let i = 0; i < satellites.groups.length; ++i)
            if (satellites.groups[i].key === satellites.group)
                return i;
        return 0;
    }
    function snapshotIndex() {
        for (let i = 0; i < satellites.snapshots.length; ++i)
            if (satellites.snapshots[i].id === satellites.snapshotId)
                return i;
        return 0;
    }
    function chooseCity(city) {
        appState.setObserver(city.name, city.latitude, city.longitude, NaN, city.timeZone);
        window.following = false;
        worldMap.centerOn(city.longitude, city.latitude);
        observerDialog.populate();
        observerTabs.currentIndex = 0;
    }

    color: backgroundColor
    palette.window: backgroundColor
    palette.base: surfaceColor
    palette.text: textColor
    palette.windowText: textColor
    palette.button: surfaceColor
    palette.buttonText: textColor
    palette.highlight: accentColor
    palette.highlightedText: backgroundColor
    palette.mid: lineColor
    palette.dark: lineColor
    palette.placeholderText: mutedColor
    AppState {
        id: appState
    }
    SatelliteModel {
        id: satellites
        clock: appState
    }
    Connections {
        target: satellites
        function onFrameChanged() {
            if (window.following && window.obs.longitude !== undefined)
                worldMap.centerOn(window.obs.longitude, window.obs.latitude);
        }
    }

    component IconButton: ToolButton {
        property string tip
        implicitWidth: 32
        implicitHeight: 32
        icon.width: 17
        icon.height: 17
        icon.color: window.textColor
        Accessible.name: tip
        ToolTip.visible: hovered
        ToolTip.text: tip
        ToolTip.delay: 500
    }
    component DetailTab: TabButton {
        id: tab
        contentItem: Label {
            text: tab.text
            color: tab.checked ? window.textColor : window.mutedColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: tab.checked ? window.backgroundColor : window.surfaceColor
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: tab.checked ? window.accentColor : window.lineColor }
        }
    }
    component FieldRow: RowLayout {
        property string label
        property string value
        spacing: 8
        Layout.fillWidth: true
        Label {
            text: label
            color: window.mutedColor
            font.pixelSize: 12
            Layout.preferredWidth: 116
            wrapMode: Text.Wrap
        }
        TextEdit {
            text: value
            color: window.textColor
            font.family: window.font.family
            font.pixelSize: 12
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            Layout.fillWidth: true
        }
    }
    component Divider: Rectangle {
        color: window.lineColor
        Layout.fillWidth: true
        implicitHeight: 1
    }

    header: ColumnLayout {
        spacing: 0
        ToolBar {
            Layout.fillWidth: true
            implicitHeight: 46
            background: Rectangle {
                color: window.backgroundColor
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 12
                Label {
                    text: "星纪"
                    font.pixelSize: 20
                    font.bold: true
                }
                Label {
                    text: "ASTROCHRON"
                    font.pixelSize: 11
                    color: window.mutedColor
                }
                Rectangle {
                    width: 1
                    height: 20
                    color: window.lineColor
                }
                Label {
                    text: "卫星观测"
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: appState.timeText
                    font.pixelSize: 12
                }
                Label {
                    text: appState.timeZoneName
                    color: window.mutedColor
                    font.pixelSize: 11
                }
                IconButton {
                    icon.source: "qrc:/icons/moon.svg"
                    tip: appState.darkTheme ? "浅色界面" : "深色界面"
                    onClicked: appState.darkTheme = !appState.darkTheme
                }
            }
        }
        Divider {}
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 38
            color: window.surfaceColor
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 16
                Label {
                    text: "观测地点：" + appState.observerName
                    font.bold: true
                    Layout.maximumWidth: 210
                    elide: Text.ElideRight
                }
                Button {
                    text: "更换地点"
                    icon.source: "qrc:/icons/map-pin.svg"
                    icon.color: window.textColor
                    implicitHeight: 30
                    font.pixelSize: 12
                    onClicked: {
                        observerTabs.currentIndex = 1;
                        observerDialog.open();
                    }
                }
                Label {
                    text: (appState.observerLatitude >= 0 ? "北纬 " : "南纬 ") + Math.abs(appState.observerLatitude).toFixed(4) + "°"
                    font.pixelSize: 12
                }
                Label {
                    text: (appState.observerLongitude >= 0 ? "东经 " : "西经 ") + Math.abs(appState.observerLongitude).toFixed(4) + "°"
                    font.pixelSize: 12
                }
                Label {
                    text: appState.hasObserverHeight ? "海拔 " + appState.observerHeight.toFixed(0) + " 米" : appState.elevationBusy ? "正在查询海拔" : "海拔待填写"
                    font.pixelSize: 12
                }
                Label {
                    text: "最低高度角 " + appState.minimumElevation.toFixed(0) + "°"
                    font.pixelSize: 12
                    color: window.mutedColor
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
        Divider {}
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: window.expanded ? 200 : 224
            Layout.fillHeight: true
            color: window.surfaceColor
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                RowLayout {
                    Label {
                        text: "卫星"
                        font.bold: true
                    }
                    Label {
                        text: satellites.total
                        color: window.mutedColor
                        font.pixelSize: 11
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    IconButton {
                        icon.source: "qrc:/icons/folder-open.svg"
                        tip: "导入轨道文件"
                        onClicked: importDialog.open()
                    }
                    IconButton {
                        icon.source: "qrc:/icons/refresh-cw.svg"
                        tip: "更新轨道数据"
                        enabled: !satellites.downloading && satellites.group !== "local"
                        onClicked: satellites.refresh()
                    }
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: satellites.groups
                    textRole: "name"
                    valueRole: "key"
                    currentIndex: window.groupIndex()
                    onActivated: satellites.group = currentValue
                }
                TextField {
                    Layout.fillWidth: true
                    placeholderText: "名称、编号或国际编号"
                    selectByMouse: true
                    onTextChanged: satellites.search = text
                }
                ListView {
                    id: satelliteList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: satellites
                    currentIndex: -1
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: satelliteDelegate
                        required property string satelliteId
                        required property string satelliteName
                        required property string originalName
                        required property string elevationText
                        width: ListView.view.width
                        height: 53
                        highlighted: satelliteId === satellites.selectedId
                        background: Rectangle {
                            color: satelliteDelegate.highlighted ? (appState.darkTheme ? "#364c47" : "#e0efea") : "transparent"
                            Rectangle {
                                width: 3
                                height: parent.height
                                color: window.accentColor
                                visible: satelliteDelegate.highlighted
                            }
                        }
                        contentItem: ColumnLayout {
                            spacing: 3
                            Label {
                                Layout.fillWidth: true
                                text: satelliteName
                                elide: Text.ElideRight
                                font.bold: satelliteId === satellites.selectedId
                            }
                            RowLayout {
                                Label {
                                    text: satelliteId
                                    color: window.mutedColor
                                    font.pixelSize: 11
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: elevationText
                                    color: window.mutedColor
                                    font.pixelSize: 11
                                }
                            }
                        }
                        onClicked: satellites.select(satelliteId)
                        ToolTip.visible: hovered
                        ToolTip.text: originalName
                        ToolTip.delay: 800
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: satelliteList.count === 0
                        text: satellites.downloading ? "正在获取卫星目录" : "暂无匹配目标"
                        color: window.mutedColor
                        font.pixelSize: 12
                    }
                }
                Divider {}
                Label {
                    text: satellites.status
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 11
                    color: window.mutedColor
                }
            }
        }
        Rectangle {
            width: 1
            Layout.fillHeight: true
            color: window.lineColor
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 8
                Layout.topMargin: 5
                Layout.bottomMargin: 5
                Label {
                    text: window.hasSatellite ? (window.obs.name || "卫星轨迹") : "全球地图"
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                IconButton {
                    icon.source: "qrc:/icons/crosshair.svg"
                    tip: "跟随卫星"
                    checkable: true
                    checked: window.following
                    enabled: window.hasSatellite
                    onClicked: {
                        window.following = !window.following;
                        if (window.following && window.obs.longitude !== undefined)
                            worldMap.centerOn(window.obs.longitude, window.obs.latitude);
                    }
                }
                IconButton {
                    icon.source: "qrc:/icons/minus.svg"
                    tip: "缩小"
                    enabled: worldMap.zoom > 1
                    onClicked: {
                        window.following = false;
                        worldMap.zoomAt(1 / 1.5, worldMap.width / 2, worldMap.height / 2);
                    }
                }
                Label {
                    text: worldMap.zoom.toFixed(1) + "×"
                    Layout.preferredWidth: 38
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 12
                }
                IconButton {
                    icon.source: "qrc:/icons/plus.svg"
                    tip: "放大"
                    enabled: worldMap.zoom < 12
                    onClicked: {
                        window.following = false;
                        worldMap.zoomAt(1.5, worldMap.width / 2, worldMap.height / 2);
                    }
                }
                IconButton {
                    icon.source: "qrc:/icons/globe.svg"
                    tip: "全球视图"
                    onClicked: {
                        window.following = false;
                        worldMap.resetView();
                    }
                }
                IconButton {
                    icon.source: "qrc:/icons/map-pin.svg"
                    tip: "地图选点"
                    checkable: true
                    checked: window.choosingLocation
                    onClicked: window.choosingLocation = !window.choosingLocation
                }
                IconButton {
                    icon.source: window.expanded ? "qrc:/icons/minimize-2.svg" : "qrc:/icons/maximize-2.svg"
                    tip: window.expanded ? "收起地图" : "展开地图"
                    onClicked: window.expanded = !window.expanded
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 180
                color: window.oceanColor
                clip: true
                WorldMap {
                    id: worldMap
                    anchors.fill: parent
                    showLakes: lakeLayer.checked
                    showBorders: borderLayer.checked
                    showGrid: gridLayer.checked
                    showCities: cityLayer.checked
                    showStation: stationLayer.checked
                    observerLongitude: appState.observerLongitude
                    observerLatitude: appState.observerLatitude
                    landColor: appState.darkTheme ? "#526c72" : "#bacbd0"
                    waterColor: window.oceanColor
                    borderColor: appState.darkTheme ? "#849a9f" : "#8da5ab"
                    gridColor: appState.darkTheme ? "#41595f" : "#d0dfe2"
                }
                ShaderEffect {
                    anchors.fill: parent
                    visible: daynightLayer.checked
                    property vector2d viewSize: Qt.vector2d(width, height)
                    property real pixelsPerDegree: worldMap.pixelsPerDegree
                    property real centerLongitude: worldMap.centerLongitude
                    property real centerLatitude: worldMap.centerLatitude
                    property vector3d sunDirection: appState.sunDirection
                    property color nightColor: appState.darkTheme ? "#66081117" : "#382d4249"
                    property color lineColor: appState.darkTheme ? "#e9b57b" : "#b57a36"
                    fragmentShader: "qrc:/shaders/daynight.frag.qsb"
                }
                Repeater {
                    model: worldMap.cityLabels
                    delegate: Item {
                        required property var modelData
                        z: 2
                        Rectangle {
                            x: modelData.pointX - 1.5
                            y: modelData.pointY - 1.5
                            width: 3
                            height: 3
                            radius: 1.5
                            color: window.textColor
                        }
                        Text {
                            x: modelData.x
                            y: modelData.y
                            text: modelData.name
                            font.family: window.font.family
                            font.pixelSize: 11
                            color: window.textColor
                            style: Text.Outline
                            styleColor: window.oceanColor
                        }
                    }
                }
                OrbitLayer {
                    id: orbitLayer
                    z: 1
                    anchors.fill: parent
                    map: worldMap
                    markers: satellites.markers
                    trajectory: satellites.trajectory
                    selectedId: satellites.selectedId
                    time: appState.unixTime
                    minimumElevation: appState.minimumElevation
                    showPast: pastLayer.checked
                    showFuture: futureLayer.checked
                    showCoverage: coverageLayer.checked
                    showSatellites: satelliteLayer.checked
                    pastColor: window.pastColor
                    futureColor: window.accentColor
                    markerColor: appState.darkTheme ? "#c3cce7" : "#536a8e"
                }
                Text {
                    z: 3
                    visible: window.obs.name !== undefined && orbitLayer.selectedPosition.x >= 0 && orbitLayer.selectedPosition.x <= parent.width && orbitLayer.selectedPosition.y >= 0 && orbitLayer.selectedPosition.y <= parent.height
                    x: Math.max(4, Math.min(parent.width - width - 4, orbitLayer.selectedPosition.x + 9))
                    y: orbitLayer.selectedPosition.y - height - 4
                    text: window.obs.name || ""
                    color: window.accentColor
                    font.bold: true
                    font.pixelSize: 12
                    style: Text.Outline
                    styleColor: window.oceanColor
                }
                Rectangle {
                    visible: stationLayer.checked
                    z: 3
                    x: worldMap.observerPosition.x - width / 2
                    y: worldMap.observerPosition.y - height / 2
                    width: 9
                    height: 9
                    rotation: 45
                    color: appState.darkTheme ? "#efb478" : "#bf702c"
                    border.width: 1
                    border.color: window.backgroundColor
                }
                MouseArea {
                    z: 4
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: window.choosingLocation ? Qt.CrossCursor : pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real previousX
                    property real previousY
                    property real dragDistance: 0
                    onPressed: mouse => {
                        previousX = mouse.x;
                        previousY = mouse.y;
                        dragDistance = 0;
                    }
                    onPositionChanged: mouse => {
                        if (pressed && !window.choosingLocation) {
                            dragDistance += Math.hypot(mouse.x - previousX, mouse.y - previousY);
                            if (dragDistance > 3)
                                window.following = false;
                            worldMap.panBy(mouse.x - previousX, mouse.y - previousY);
                            previousX = mouse.x;
                            previousY = mouse.y;
                        }
                    }
                    onClicked: mouse => {
                        if (window.choosingLocation) {
                            const point = worldMap.coordinateAt(mouse.x, mouse.y);
                            appState.setObserver("地图选点", point.y, point.x, NaN, appState.timeZone);
                            window.choosingLocation = false;
                        } else if (dragDistance < 4) {
                            const id = orbitLayer.satelliteAt(mouse.x, mouse.y);
                            if (id.length)
                                satellites.select(id);
                        }
                    }
                    onWheel: wheel => {
                        window.following = false;
                        worldMap.zoomAt(Math.pow(1.0015, wheel.angleDelta.y), wheel.x, wheel.y);
                        wheel.accepted = true;
                    }
                    onDoubleClicked: mouse => {
                        window.following = false;
                        worldMap.zoomAt(1.5, mouse.x, mouse.y);
                    }
                }
            }
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: 3
                Layout.bottomMargin: 3
                spacing: 3
                CheckBox {
                    id: pastLayer
                    text: "过去"
                    checked: true
                    palette.windowText: window.pastColor
                    font.pixelSize: 12
                }
                CheckBox {
                    id: futureLayer
                    text: "未来"
                    checked: true
                    palette.windowText: window.accentColor
                    font.pixelSize: 12
                }
                CheckBox {
                    id: satelliteLayer
                    text: "卫星"
                    checked: true
                    font.pixelSize: 12
                }
                CheckBox {
                    id: daynightLayer
                    text: "昼夜线"
                    checked: true
                    font.pixelSize: 12
                }
                CheckBox {
                    id: coverageLayer
                    text: "覆盖范围"
                    font.pixelSize: 12
                    ToolTip.visible: hovered
                    ToolTip.text: "按最低高度角计算，采用球面近似"
                }
                CheckBox {
                    id: cityLayer
                    text: "城市"
                    checked: true
                    font.pixelSize: 12
                }
                CheckBox {
                    id: stationLayer
                    text: "观测地点"
                    checked: true
                    font.pixelSize: 12
                }
                CheckBox {
                    id: borderLayer
                    text: "国界"
                    checked: true
                    font.pixelSize: 12
                }
                CheckBox {
                    id: lakeLayer
                    text: "湖泊"
                    checked: true
                    font.pixelSize: 12
                }
                CheckBox {
                    id: gridLayer
                    text: "经纬网"
                    checked: true
                    font.pixelSize: 12
                }
            }
            Divider {
                visible: !window.expanded
            }
            RowLayout {
                visible: !window.expanded
                Layout.fillWidth: true
                Layout.margins: 10
                Label {
                    text: "过境预报"
                    font.bold: true
                }
                Label {
                    text: satellites.passes.length + " 次"
                    color: window.mutedColor
                    font.pixelSize: 11
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: "最低高度角 " + appState.minimumElevation.toFixed(0) + "°"
                    color: window.mutedColor
                    font.pixelSize: 11
                }
            }
            ScrollView {
                id: passScroll
                visible: !window.expanded
                Layout.fillWidth: true
                Layout.preferredHeight: window.height < 820 ? 160 : 200
                Layout.minimumHeight: 130
                clip: true
                contentWidth: Math.max(840, availableWidth)
                contentHeight: passColumn.implicitHeight
                Column {
                    id: passColumn
                    width: passScroll.contentWidth
                    Row {
                        height: 28
                        Repeater {
                            model: ["开始 / 方位", "最高点 / 高度角", "结束 / 方位", "持续 / 最高点距离", "光照观测时段"]
                            Label {
                                required property string modelData
                                width: modelData === "光照观测时段" ? passColumn.width - 600 : 150
                                leftPadding: 10
                                text: modelData
                                color: window.mutedColor
                                font.pixelSize: 11
                            }
                        }
                    }
                    Repeater {
                        model: satellites.passes
                        delegate: ItemDelegate {
                            id: passDelegate
                            required property var modelData
                            width: passColumn.width
                            height: 52
                            background: Rectangle {
                                color: appState.unixTime >= modelData.start && appState.unixTime <= modelData.end ? (appState.darkTheme ? "#344440" : "#e9f2ee") : "transparent"
                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: window.lineColor
                                    anchors.bottom: parent.bottom
                                }
                            }
                            contentItem: Row {
                                Repeater {
                                    model: [[passDelegate.modelData.startText, passDelegate.modelData.startAz], [passDelegate.modelData.peakText, passDelegate.modelData.maximum], [passDelegate.modelData.endText, passDelegate.modelData.endAz], [passDelegate.modelData.duration, passDelegate.modelData.range]]
                                    Column {
                                        required property var modelData
                                        width: 150
                                        spacing: 4
                                        Label {
                                            text: modelData[0]
                                            font.pixelSize: 11
                                        }
                                        Label {
                                            text: modelData[1]
                                            font.pixelSize: 11
                                            color: window.mutedColor
                                        }
                                    }
                                }
                                Label {
                                    text: passDelegate.modelData.optical
                                    width: passColumn.width - 620
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 11
                                    color: window.mutedColor
                                }
                            }
                            onClicked: appState.seek(modelData.peak)
                        }
                    }
                    Label {
                        visible: satellites.passes.length === 0
                        text: satellites.calculating ? "正在计算过境" : "本时段内暂无满足高度角条件的过境"
                        color: window.mutedColor
                        padding: 14
                        font.pixelSize: 12
                    }
                }
            }
        }
        Rectangle {
            visible: !window.expanded
            width: 1
            Layout.fillHeight: true
            color: window.lineColor
        }
        ColumnLayout {
            visible: !window.expanded
            Layout.preferredWidth: 324
            Layout.minimumWidth: 324
            Layout.fillHeight: true
            spacing: 0
            TabBar {
                id: detailsTabs
                Layout.fillWidth: true
                DetailTab {
                    text: "观测"
                }
                DetailTab {
                    text: "轨道"
                }
                DetailTab {
                    text: "资料"
                }
                DetailTab {
                    text: "地影"
                }
            }
            ScrollView {
                id: detailScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: detailScroll.availableWidth
                    spacing: 10
                    ColumnLayout {
                        visible: detailsTabs.currentIndex === 0
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 9
                        Label {
                            text: window.obs.name || "请选择卫星"
                            font.pixelSize: 17
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Label {
                            text: window.obs.visibility || ""
                            color: window.accentColor
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Label {
                            text: window.skyPass.peakText ? "天空轨迹 · " + window.skyPass.peakText : "天空轨迹"
                            color: window.mutedColor
                            font.pixelSize: 11
                        }
                        SkyPlot {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 230
                            trajectory: satellites.trajectory
                            observation: window.obs
                            selectedTime: appState.unixTime
                            passTime: window.skyPass.peak || 0
                            minimumElevation: appState.minimumElevation
                            gridColor: window.lineColor
                            textColor: window.mutedColor
                            futureColor: window.accentColor
                            pastColor: window.pastColor
                        }
                        FieldRow {
                            label: "方位角"
                            value: window.value("azimuth", 2, "°") + "  " + (window.obs.direction || "")
                        }
                        FieldRow {
                            label: "高度角"
                            value: window.value("elevation", 2, "°")
                        }
                        FieldRow {
                            label: "距离"
                            value: window.value("range", 2, " 千米")
                        }
                        FieldRow {
                            label: "径向速度"
                            value: window.value("rangeRate", 3, " 千米/秒")
                        }
                        FieldRow {
                            label: "相对运动"
                            value: window.obs.motion || "待计算"
                        }
                        Divider {}
                        FieldRow {
                            label: "卫星高度"
                            value: window.value("altitude", 2, " 千米")
                        }
                        FieldRow {
                            label: "运行速度"
                            value: window.value("speed", 3, " 千米/秒")
                        }
                        FieldRow {
                            label: "星下点纬度"
                            value: window.value("latitude", 4, "°")
                        }
                        FieldRow {
                            label: "星下点经度"
                            value: window.value("longitude", 4, "°")
                        }
                        Divider {}
                        FieldRow {
                            label: "太阳高度角"
                            value: window.value("sunElevation", 2, "°")
                        }
                        FieldRow {
                            label: "卫星受光"
                            value: window.obs.lighting || "待计算"
                        }
                        FieldRow {
                            label: "预计星等"
                            value: "暂无数据"
                        }
                        FieldRow {
                            label: "云量 / 能见度"
                            value: "暂无数据"
                        }
                        Divider {}
                        RowLayout {
                            Label {
                                text: "接收频率"
                                color: window.mutedColor
                                font.pixelSize: 12
                                Layout.preferredWidth: 108
                            }
                            TextField {
                                Layout.fillWidth: true
                                text: satellites.receiveFrequency.toFixed(3)
                                selectByMouse: true
                                validator: DoubleValidator {
                                    bottom: 0
                                    top: 1000000
                                    locale: "C"
                                }
                                onEditingFinished: if (acceptableInput)
                                    satellites.receiveFrequency = Number(text)
                            }
                            Label {
                                text: "兆赫"
                                font.pixelSize: 11
                            }
                        }
                        FieldRow {
                            label: "多普勒频移"
                            value: window.value("doppler", 0, " 赫兹")
                        }
                        Label {
                            visible: window.obs.heightEstimated === true
                            text: "观测计算暂按海拔 0 米估算"
                            color: window.pastColor
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Label {
                            visible: Math.abs(window.obs.epochAge || 0) > 7
                            text: "根数历元与所选时刻相隔 " + Math.abs(window.obs.epochAge || 0).toFixed(1) + " 天，预报精度可能下降"
                            color: window.pastColor
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                    }
                    ColumnLayout {
                        visible: detailsTabs.currentIndex === 1
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 12
                        RowLayout {
                            Label {
                                text: "轨道根数"
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            IconButton {
                                icon.source: "qrc:/icons/copy.svg"
                                tip: "复制轨道参数"
                                enabled: window.hasSatellite
                                onClicked: satellites.copyDetails()
                            }
                            IconButton {
                                icon.source: "qrc:/icons/download.svg"
                                tip: "保存轨道根数"
                                enabled: window.hasSatellite
                                onClicked: exportDialog.open()
                            }
                        }
                        Repeater {
                            model: satellites.orbitFields
                            FieldRow {
                                required property var modelData
                                label: modelData.label
                                value: modelData.value
                            }
                        }
                        Divider { visible: satellites.relatedObjects.length > 1 }
                        Label {
                            visible: satellites.relatedObjects.length > 1
                            text: "轨道条目（" + satellites.relatedObjects.length + "）"
                            font.bold: true
                        }
                        Repeater {
                            model: satellites.relatedObjects.length > 1 ? satellites.relatedObjects : []
                            ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 3
                                Label { text: modelData.name; Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 12 }
                                Label { text: modelData.id + " · " + modelData.internationalId; color: window.mutedColor; font.pixelSize: 11 }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: detailsTabs.currentIndex === 2
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 12
                        Label {
                            text: "轨道资料"
                            font.bold: true
                        }
                        Label {
                            text: "获取时间"
                            color: window.mutedColor
                            font.pixelSize: 12
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: satellites.snapshots
                            textRole: "label"
                            valueRole: "id"
                            currentIndex: window.snapshotIndex()
                            onActivated: satellites.loadSnapshot(currentValue)
                        }
                        Label {
                            text: window.snapshotIndex() > 0 ? "历史根数回放" : "使用最近获取的根数"
                            color: window.accentColor
                            font.pixelSize: 12
                        }
                        Label {
                            text: "数据来源"
                            color: window.mutedColor
                            font.pixelSize: 12
                        }
                        TextEdit {
                            text: satellites.sourceText
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            color: window.textColor
                            font.pixelSize: 11
                        }
                        Label {
                            text: "根数按协调世界时记时，观测时间采用地点时区。光照观测时段按卫星受阳光照射、太阳高度角低于 -6° 筛选；实际可见性还与星等、天气和地形有关。"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: window.mutedColor
                            font.pixelSize: 12
                        }
                        Divider {}
                        Label {
                            text: "地图与高程"
                            font.bold: true
                        }
                        Label {
                            text: "底图：Natural Earth，1:5000 万\n地形高程：Open-Meteo / Copernicus DEM\n高度基准：EGM2008\n大地水准面：NGA / GeographicLib"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: window.mutedColor
                            font.pixelSize: 12
                            lineHeight: 1.5
                        }
                        Label {
                            text: "轨道计算：Vallado SGP4\n太阳位置：Astronomy Engine"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: window.mutedColor
                            font.pixelSize: 12
                            lineHeight: 1.5
                        }
                    }
                    ColumnLayout {
                        visible: detailsTabs.currentIndex === 3
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 8
                        Label { text: "地影事件"; font.bold: true }
                        Repeater {
                            model: satellites.shadowEvents
                            ItemDelegate {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 34
                                contentItem: RowLayout {
                                    Label { text: modelData.timeText; font.pixelSize: 12; Layout.fillWidth: true }
                                    Label { text: modelData.name; font.pixelSize: 12; color: window.mutedColor }
                                }
                                onClicked: appState.seek(modelData.time)
                            }
                        }
                        Label { visible: satellites.shadowEvents.length === 0; text: "本时段内暂无地影进出事件"; font.pixelSize: 12; color: window.mutedColor }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        color: window.surfaceColor
        implicitHeight: 68
        Rectangle {
            width: parent.width
            height: 1
            color: window.lineColor
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 16
            Button {
                text: "回到现在"
                icon.source: "qrc:/icons/rotate-ccw.svg"
                icon.color: window.textColor
                enabled: !appState.live
                onClicked: appState.resumeLive()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Slider {
                    Layout.fillWidth: true
                    from: -720
                    to: 720
                    stepSize: 1
                    value: appState.minuteOffset
                    onMoved: appState.minuteOffset = Math.round(value)
                    ToolTip.visible: pressed || hovered
                    ToolTip.text: appState.timeText
                }
                RowLayout {
                    Label { text: appState.startTimeText; font.pixelSize: 11; color: window.mutedColor }
                    Item { Layout.fillWidth: true }
                    Label { text: appState.endTimeText; font.pixelSize: 11; color: window.mutedColor }
                }
            }
        }
    }

    Dialog {
        id: observerDialog
        title: "观测地点"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 490
        height: Math.min(720, window.height - 40)
        function populate() {
            nameField.text = appState.observerName;
            latitudeField.text = appState.observerLatitude.toFixed(6);
            longitudeField.text = appState.observerLongitude.toFixed(6);
            heightField.text = appState.hasObserverHeight ? appState.observerHeight.toFixed(1) : "";
            timeZoneField.editText = appState.timeZone;
            minimumField.value = appState.minimumElevation;
            inputError.visible = false;
        }
        onOpened: populate()
        Connections {
            target: appState
            function onElevationChanged() {
                if (observerDialog.opened && !appState.elevationBusy && appState.hasObserverHeight && !heightField.activeFocus)
                    heightField.text = appState.observerHeight.toFixed(1);
            }
        }
        contentItem: ColumnLayout {
            spacing: 10
            TabBar {
                id: observerTabs
                Layout.fillWidth: true
                DetailTab {
                    text: "地点设置"
                }
                DetailTab {
                    text: "城市"
                }
                DetailTab {
                    text: "常用地点"
                }
            }
            ScrollView {
                id: observerScroll
                visible: observerTabs.currentIndex === 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: observerScroll.availableWidth
                    spacing: 9
                    Label {
                        text: "名称"
                    }
                    TextField {
                        id: nameField
                        Layout.fillWidth: true
                        selectByMouse: true
                    }
                    RowLayout {
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "纬度（北纬为正）"
                            }
                            TextField {
                                id: latitudeField
                                Layout.fillWidth: true
                                selectByMouse: true
                                validator: DoubleValidator {
                                    bottom: -90
                                    top: 90
                                    decimals: 6
                                    locale: "C"
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "经度（东经为正）"
                            }
                            TextField {
                                id: longitudeField
                                Layout.fillWidth: true
                                selectByMouse: true
                                validator: DoubleValidator {
                                    bottom: -180
                                    top: 180
                                    decimals: 6
                                    locale: "C"
                                }
                            }
                        }
                    }
                    Label {
                        text: "海拔（米，EGM2008）"
                    }
                    RowLayout {
                        TextField {
                            id: heightField
                            Layout.fillWidth: true
                            selectByMouse: true
                            validator: DoubleValidator {
                                bottom: -12000
                                top: 100000
                                decimals: 1
                                locale: "C"
                            }
                        }
                        Button {
                            text: "查询海拔"
                            enabled: !appState.elevationBusy
                            onClicked: {
                                if (latitudeField.acceptableInput && longitudeField.acceptableInput && appState.setObserver(nameField.text, Number(latitudeField.text), Number(longitudeField.text), NaN, timeZoneField.editText)) {
                                    if (!appState.automaticElevation)
                                        appState.lookupElevation();
                                } else
                                    inputError.visible = true;
                            }
                        }
                    }
                    Label {
                        text: appState.elevationStatus
                        color: window.mutedColor
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.pixelSize: 11
                    }
                    Label {
                        text: appState.hasObserverHeight ? "椭球高 " + appState.ellipsoidHeight.toFixed(1) + " 米" : "海拔待填写，观测计算暂按海拔 0 米估算"
                        color: window.mutedColor
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.pixelSize: 11
                    }
                    CheckBox {
                        text: "选点后自动查询地形海拔"
                        checked: appState.automaticElevation
                        onToggled: appState.automaticElevation = checked
                    }
                    Label {
                        text: "查询时向 Open-Meteo 发送地点坐标。地形海拔采用约 90 米分辨率的 Copernicus DEM，楼顶等位置可手动填写。"
                        font.pixelSize: 11
                        color: window.mutedColor
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                    Label {
                        text: "<a href='https://open-meteo.com/en/docs/elevation-api'>Open-Meteo</a> · <a href='https://doi.org/10.5270/ESA-c5d3d65'>Copernicus DEM</a>"
                        font.pixelSize: 11
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                    RowLayout {
                        Label {
                            text: "最低高度角"
                            Layout.fillWidth: true
                        }
                        SpinBox {
                            id: minimumField
                            from: 0
                            to: 89
                            editable: true
                        }
                        Label {
                            text: "度"
                        }
                    }
                    Label {
                        text: "时区"
                    }
                    ComboBox {
                        id: timeZoneField
                        Layout.fillWidth: true
                        editable: true
                        model: appState.timeZones
                    }
                    Label {
                        id: inputError
                        text: "请输入有效的地点名称、坐标和时区。"
                        color: "#b85142"
                        visible: false
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                }
            }
            ColumnLayout {
                visible: observerTabs.currentIndex === 1
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextField {
                    Layout.fillWidth: true
                    placeholderText: "搜索城市"
                    selectByMouse: true
                    onTextChanged: window.cityResults = appState.findCities(text)
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: window.cityResults
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        required property var modelData
                        width: ListView.view.width
                        height: 44
                        text: modelData.name + "    " + modelData.latitude.toFixed(2) + "°, " + modelData.longitude.toFixed(2) + "°"
                        onClicked: window.chooseCity(modelData)
                    }
                }
            }
            ListView {
                visible: observerTabs.currentIndex === 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: appState.savedObservers
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: 48
                    contentItem: RowLayout {
                        Label {
                            text: modelData.name
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        IconButton {
                            icon.source: "qrc:/icons/minus.svg"
                            tip: "移除常用地点"
                            onClicked: appState.removeObserver(index)
                        }
                    }
                    onClicked: {
                        appState.loadObserver(index);
                        observerDialog.populate();
                        observerTabs.currentIndex = 0;
                        window.following = false;
                        worldMap.centerOn(appState.observerLongitude, appState.observerLatitude);
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: appState.savedObservers.length === 0
                    text: "暂无常用地点"
                    color: window.mutedColor
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                text: "保存为常用地点"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: if (observerDialog.apply())
                    appState.saveObserver()
            }
            Button {
                text: "关闭"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: "确定"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: if (observerDialog.apply())
                    observerDialog.close()
            }
        }
        function apply() {
            if (latitudeField.acceptableInput && longitudeField.acceptableInput && (heightField.text.length === 0 || heightField.acceptableInput) && appState.setObserver(nameField.text, Number(latitudeField.text), Number(longitudeField.text), heightField.text.length === 0 ? NaN : Number(heightField.text), timeZoneField.editText)) {
                appState.minimumElevation = minimumField.value;
                window.following = false;
                worldMap.centerOn(appState.observerLongitude, appState.observerLatitude);
                return true;
            }
            inputError.visible = true;
            observerTabs.currentIndex = 0;
            return false;
        }
    }
    FileDialog {
        id: importDialog
        title: "导入轨道文件"
        nameFilters: ["轨道文件 (*.json *.tle *.txt)", "所有文件 (*)"]
        onAccepted: satellites.importFile(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: "保存轨道根数"
        fileMode: FileDialog.SaveFile
        nameFilters: ["轨道根数 (*.json)"]
        defaultSuffix: "json"
        onAccepted: satellites.exportSelected(selectedFile)
    }
    Shortcut {
        sequence: "Escape"
        enabled: window.expanded
        onActivated: window.expanded = false
    }
    Component.onCompleted: {
        window.cityResults = appState.findCities("");
        worldMap.centerOn(appState.observerLongitude, appState.observerLatitude);
    }
}
