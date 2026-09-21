import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

ApplicationWindow {
    id: window
    width: 1320
    height: 820
    minimumWidth: 900
    minimumHeight: 640
    visible: true
    title: "ASTROCHRON · 星纪"
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 13

    property bool expanded: false
    property bool choosingLocation: false
    property color backgroundColor: appState.darkTheme ? "#202628" : "#ffffff"
    property color surfaceColor: appState.darkTheme ? "#292f31" : "#f3f5f5"
    property color textColor: appState.darkTheme ? "#e4ebed" : "#263135"
    property color mutedColor: appState.darkTheme ? "#a8b7bc" : "#64757b"
    property color lineColor: appState.darkTheme ? "#414e52" : "#d6dfe1"
    property color accentColor: appState.darkTheme ? "#55c8ae" : "#087e6f"
    property color oceanColor: appState.darkTheme ? "#233136" : "#eaf1f3"
    property var cityResults: []

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

    AppState { id: appState }

    component IconButton: ToolButton {
        id: button
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

    header: ColumnLayout {
        spacing: 0
        ToolBar {
            Layout.fillWidth: true
            implicitHeight: 46
            background: Rectangle { color: window.backgroundColor }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 12
                Label { text: "星纪"; font.pixelSize: 20; font.bold: true }
                Label { text: "ASTROCHRON"; font.pixelSize: 11; color: window.mutedColor }
                Rectangle { width: 1; height: 20; color: window.lineColor }
                Label { text: "观测地图" }
                Item { Layout.fillWidth: true }
                Label { text: appState.timeText; font.pixelSize: 12 }
                Label { text: appState.timeZoneName; color: window.mutedColor; font.pixelSize: 11 }
                IconButton {
                    icon.source: "qrc:/icons/moon.svg"
                    tip: appState.darkTheme ? "浅色界面" : "深色界面"
                    onClicked: appState.darkTheme = !appState.darkTheme
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: window.lineColor }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 38
            color: window.surfaceColor
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 16
                Label { text: "观测地点：" + appState.observerName; font.bold: true }
                Label {
                    text: (appState.observerLatitude >= 0 ? "北纬 " : "南纬 ") + Math.abs(appState.observerLatitude).toFixed(4) + "°"
                    font.pixelSize: 12
                }
                Label {
                    text: (appState.observerLongitude >= 0 ? "东经 " : "西经 ") + Math.abs(appState.observerLongitude).toFixed(4) + "°"
                    font.pixelSize: 12
                }
                Label { text: appState.hasObserverHeight ? "海拔 " + appState.observerHeight.toFixed(0) + " 米" : "海拔待填写"; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                IconButton { icon.source: "qrc:/icons/settings-2.svg"; tip: "观测地点设置"; onClicked: observerDialog.open() }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: window.lineColor }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            visible: !window.expanded
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            color: window.surfaceColor
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                Label { text: "城市"; font.bold: true }
                TextField {
                    id: citySearch
                    Layout.fillWidth: true
                    placeholderText: "搜索城市"
                    selectByMouse: true
                    onTextChanged: window.cityResults = appState.findCities(text)
                }
                ListView {
                    id: cityList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: window.cityResults
                    currentIndex: -1
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: ItemDelegate {
                        required property var modelData
                        width: ListView.view.width
                        height: 48
                        contentItem: Column {
                            spacing: 2
                            Label { width: parent.width; text: modelData.name; color: window.textColor; elide: Text.ElideRight }
                            Label {
                                text: modelData.latitude.toFixed(2) + "°, " + modelData.longitude.toFixed(2) + "°"
                                font.pixelSize: 11
                                color: window.mutedColor
                            }
                        }
                        onClicked: {
                            appState.setObserver(modelData.name, modelData.latitude, modelData.longitude, NaN, modelData.timeZone)
                            worldMap.centerOn(modelData.longitude, modelData.latitude)
                        }
                    }
                }
                Label { visible: cityList.count === 0; text: "暂无匹配城市"; color: window.mutedColor }
                Rectangle { Layout.fillWidth: true; height: 1; color: window.lineColor }
                Button {
                    Layout.fillWidth: true
                    text: "观测地点设置"
                    icon.source: "qrc:/icons/map-pin.svg"
                    icon.color: window.textColor
                    onClicked: observerDialog.open()
                }
            }
        }
        Rectangle { visible: !window.expanded; width: 1; Layout.fillHeight: true; color: window.lineColor }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 12
                Layout.topMargin: 7
                Layout.bottomMargin: 7
                Label { text: "全球地图"; font.bold: true }
                Item { Layout.fillWidth: true }
                IconButton {
                    icon.source: "qrc:/icons/minus.svg"; tip: "缩小"
                    enabled: worldMap.zoom > 1
                    onClicked: worldMap.zoomAt(1 / 1.5, worldMap.width / 2, worldMap.height / 2)
                }
                Label { text: worldMap.zoom.toFixed(1) + "×"; Layout.preferredWidth: 42; horizontalAlignment: Text.AlignHCenter }
                IconButton {
                    icon.source: "qrc:/icons/plus.svg"; tip: "放大"
                    enabled: worldMap.zoom < 12
                    onClicked: worldMap.zoomAt(1.5, worldMap.width / 2, worldMap.height / 2)
                }
                IconButton { icon.source: "qrc:/icons/globe.svg"; tip: "全球视图"; onClicked: worldMap.resetView() }
                IconButton {
                    icon.source: "qrc:/icons/map-pin.svg"; tip: "地图选点"
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
                        Rectangle {
                            x: modelData.pointX - 1.5; y: modelData.pointY - 1.5
                            width: 3; height: 3; radius: 1.5
                            color: window.textColor
                        }
                        Text {
                            x: modelData.x; y: modelData.y
                            text: modelData.name
                            font.family: window.font.family
                            font.pixelSize: 11
                            color: window.textColor
                            style: Text.Outline
                            styleColor: window.oceanColor
                        }
                    }
                }
                Rectangle {
                    visible: stationLayer.checked
                    x: worldMap.observerPosition.x - width / 2
                    y: worldMap.observerPosition.y - height / 2
                    width: 9; height: 9; rotation: 45
                    color: appState.darkTheme ? "#efb478" : "#bf702c"
                    border.width: 1
                    border.color: window.backgroundColor
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: window.choosingLocation ? Qt.CrossCursor : pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real previousX
                    property real previousY
                    onPressed: mouse => { previousX = mouse.x; previousY = mouse.y }
                    onPositionChanged: mouse => {
                        if (pressed && !window.choosingLocation) {
                            worldMap.panBy(mouse.x - previousX, mouse.y - previousY)
                            previousX = mouse.x; previousY = mouse.y
                        }
                    }
                    onClicked: mouse => {
                        if (window.choosingLocation) {
                            const point = worldMap.coordinateAt(mouse.x, mouse.y)
                            appState.setObserver("地图选点", point.y, point.x, NaN, appState.timeZone)
                            window.choosingLocation = false
                        }
                    }
                    onWheel: wheel => { worldMap.zoomAt(Math.pow(1.0015, wheel.angleDelta.y), wheel.x, wheel.y); wheel.accepted = true }
                    onDoubleClicked: mouse => worldMap.zoomAt(1.5, mouse.x, mouse.y)
                }
            }
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: 9
                Layout.rightMargin: 9
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                spacing: 8
                CheckBox { id: borderLayer; text: "国界"; checked: true }
                CheckBox { id: lakeLayer; text: "湖泊"; checked: true }
                CheckBox { id: cityLayer; text: "城市"; checked: true }
                CheckBox { id: daynightLayer; text: "昼夜线"; checked: true }
                CheckBox { id: stationLayer; text: "观测地点"; checked: true }
                CheckBox { id: gridLayer; text: "经纬网"; checked: true }
            }
        }
    }

    footer: Rectangle {
        color: window.surfaceColor
        implicitHeight: timeLayout.implicitHeight + 24
        Rectangle { width: parent.width; height: 1; color: window.lineColor }
        ColumnLayout {
            id: timeLayout
            anchors.fill: parent
            anchors.margins: 12
            spacing: 2
            RowLayout {
                Label { text: "时间轴"; font.bold: true }
                Label { text: appState.offsetText; color: window.mutedColor; Layout.leftMargin: 8 }
                Item { Layout.fillWidth: true }
                Button {
                    text: "回到现在"
                    icon.source: "qrc:/icons/rotate-ccw.svg"
                    icon.color: window.textColor
                    onClicked: appState.resumeLive()
                }
            }
            Slider {
                Layout.fillWidth: true
                from: -720; to: 720; stepSize: 1
                value: appState.minuteOffset
                onMoved: appState.minuteOffset = Math.round(value)
            }
            RowLayout {
                Label { text: appState.startTimeText; font.pixelSize: 11; color: window.mutedColor }
                Item { Layout.fillWidth: true }
                Label { text: "前后各 12 小时"; font.pixelSize: 11; color: window.mutedColor }
                Item { Layout.fillWidth: true }
                Label { text: appState.endTimeText; font.pixelSize: 11; color: window.mutedColor }
            }
        }
    }

    Dialog {
        id: observerDialog
        title: "观测地点"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 390
        onOpened: {
            nameField.text = appState.observerName
            latitudeField.text = appState.observerLatitude.toFixed(6)
            longitudeField.text = appState.observerLongitude.toFixed(6)
            heightField.text = appState.hasObserverHeight ? appState.observerHeight.toFixed(0) : ""
            timeZoneField.editText = appState.timeZone
            inputError.visible = false
        }
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "名称" }
            TextField { id: nameField; Layout.fillWidth: true; selectByMouse: true }
            Label { text: "纬度（北纬为正）" }
            TextField { id: latitudeField; Layout.fillWidth: true; selectByMouse: true; validator: DoubleValidator { bottom: -90; top: 90; decimals: 6; locale: "C" } }
            Label { text: "经度（东经为正）" }
            TextField { id: longitudeField; Layout.fillWidth: true; selectByMouse: true; validator: DoubleValidator { bottom: -180; top: 180; decimals: 6; locale: "C" } }
            Label { text: "海拔（米）" }
            TextField { id: heightField; Layout.fillWidth: true; selectByMouse: true; validator: DoubleValidator { bottom: -12000; top: 100000; decimals: 1; locale: "C" } }
            Label { text: "时区" }
            ComboBox { id: timeZoneField; Layout.fillWidth: true; editable: true; model: appState.timeZones }
            Label { id: inputError; text: "请输入有效的地点名称、坐标和时区。"; color: "#b85142"; visible: false; Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "取消"; onClicked: observerDialog.close() }
                Button {
                    text: "确定"
                    onClicked: {
                        if (latitudeField.acceptableInput && longitudeField.acceptableInput && (heightField.text.length === 0 || heightField.acceptableInput) &&
                            appState.setObserver(nameField.text, Number(latitudeField.text), Number(longitudeField.text), heightField.text.length === 0 ? NaN : Number(heightField.text), timeZoneField.editText)) {
                            worldMap.centerOn(appState.observerLongitude, appState.observerLatitude)
                            observerDialog.close()
                        } else inputError.visible = true
                    }
                }
            }
        }
    }

    Shortcut { sequence: "Escape"; enabled: window.expanded; onActivated: window.expanded = false }
    Component.onCompleted: {
        window.cityResults = appState.findCities("")
        worldMap.centerOn(appState.observerLongitude, appState.observerLatitude)
    }
}
