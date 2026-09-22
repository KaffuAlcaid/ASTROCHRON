import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

Dialog {
    id: dialog
    required property AppState clock
    property var cities: []
    signal locationApplied
    title: "观测地点"
    modal: true
    closePolicy: clock.hasObserver ? Popup.CloseOnEscape | Popup.CloseOnPressOutside : Popup.NoAutoClose
    anchors.centerIn: Overlay.overlay
    width: 490
    height: Math.min(720, Overlay.overlay ? Overlay.overlay.height - 32 : 720)
    function populate() {
        nameField.text = clock.observerName;
        latitudeField.text = clock.observerLatitude.toFixed(6);
        longitudeField.text = clock.observerLongitude.toFixed(6);
        heightField.text = clock.hasObserverHeight ? clock.observerHeight.toFixed(1) : "";
        timeZoneField.editText = clock.timeZone;
        minimumField.value = clock.minimumElevation;
        inputError.visible = false;
    }
    function openCities() {
        tabs.currentIndex = 1;
        open();
    }
    function apply() {
        if (latitudeField.acceptableInput && longitudeField.acceptableInput && (heightField.text.length === 0 || heightField.acceptableInput) && clock.setObserver(nameField.text, Number(latitudeField.text), Number(longitudeField.text), heightField.text.length === 0 ? NaN : Number(heightField.text), timeZoneField.editText)) {
            clock.minimumElevation = minimumField.value;
            locationApplied();
            return true;
        }
        inputError.visible = true;
        tabs.currentIndex = 0;
        return false;
    }
    onOpened: populate()
    Connections {
        target: dialog.clock
        function onElevationChanged() {
            if (dialog.opened && !dialog.clock.elevationBusy && dialog.clock.hasObserverHeight && !heightField.activeFocus)
                heightField.text = dialog.clock.observerHeight.toFixed(1);
        }
    }
    contentItem: ColumnLayout {
        spacing: 10
        TabBar {
            id: tabs
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
            id: fields
            visible: tabs.currentIndex === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: fields.availableWidth
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
                            font.family: Theme.numberFont
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
                            font.family: Theme.numberFont
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
                    text: "海拔（m，EGM2008）"
                }
                RowLayout {
                    TextField {
                        id: heightField
                        Layout.fillWidth: true
                        selectByMouse: true
                        font.family: Theme.numberFont
                        validator: DoubleValidator {
                            bottom: -12000
                            top: 100000
                            decimals: 1
                            locale: "C"
                        }
                    }
                    Button {
                        text: "查询海拔"
                        enabled: !dialog.clock.elevationBusy
                        onClicked: {
                            if (latitudeField.acceptableInput && longitudeField.acceptableInput && dialog.clock.setObserver(nameField.text, Number(latitudeField.text), Number(longitudeField.text), NaN, timeZoneField.editText)) {
                                if (!dialog.clock.automaticElevation)
                                    dialog.clock.lookupElevation();
                            } else
                                inputError.visible = true;
                        }
                    }
                }
                Label {
                    text: dialog.clock.elevationStatus
                    color: Theme.muted
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 11
                }
                Label {
                    text: dialog.clock.hasObserverHeight ? "椭球高 " + dialog.clock.ellipsoidHeight.toFixed(1) + " m" : "海拔待填写，观测计算暂按海拔 0 m 估算"
                    color: Theme.muted
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 11
                }
                CheckBox {
                    text: "选点后自动查询地形海拔"
                    checked: dialog.clock.automaticElevation
                    onToggled: dialog.clock.automaticElevation = checked
                }
                Label {
                    text: "查询时向 Open-Meteo 发送地点坐标。地形海拔采用约 90 m 分辨率的 Copernicus DEM，楼顶等位置可手动填写。"
                    font.pixelSize: 11
                    color: Theme.muted
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
                        text: "°"
                    }
                }
                Label {
                    text: "时区"
                }
                ComboBox {
                    id: timeZoneField
                    Layout.fillWidth: true
                    editable: true
                    model: dialog.clock.timeZones
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
            visible: tabs.currentIndex === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextField {
                Layout.fillWidth: true
                placeholderText: "搜索城市"
                selectByMouse: true
                onTextChanged: dialog.cities = dialog.clock.findCities(text)
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: dialog.cities
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    height: 44
                    text: modelData.name + "    " + modelData.latitude.toFixed(2) + "°, " + modelData.longitude.toFixed(2) + "°"
                    onClicked: {
                        dialog.clock.setObserver(modelData.name, modelData.latitude, modelData.longitude, NaN, modelData.timeZone);
                        dialog.populate();
                        tabs.currentIndex = 0;
                        dialog.locationApplied();
                    }
                }
            }
        }
        ListView {
            visible: tabs.currentIndex === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.clock.savedObservers
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
                        onClicked: dialog.clock.removeObserver(index)
                    }
                }
                onClicked: {
                    dialog.clock.loadObserver(index);
                    dialog.populate();
                    tabs.currentIndex = 0;
                    dialog.locationApplied();
                }
            }
            Label {
                anchors.centerIn: parent
                visible: dialog.clock.savedObservers.length === 0
                text: "暂无常用地点"
                color: Theme.muted
            }
        }
    }
    footer: DialogButtonBox {
        Button {
            text: "保存为常用地点"
            enabled: dialog.clock.hasObserver || tabs.currentIndex === 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: if (dialog.apply())
                dialog.clock.saveObserver()
        }
        Button {
            text: "关闭"
            visible: dialog.clock.hasObserver
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: "确定"
            enabled: dialog.clock.hasObserver || tabs.currentIndex === 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: if (dialog.apply())
                dialog.close()
        }
    }
    Component.onCompleted: cities = clock.findCities("")
}
