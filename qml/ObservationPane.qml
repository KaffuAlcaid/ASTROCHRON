import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrochron

ColumnLayout {
    id: pane
    required property AppState clock
    required property SatelliteModel satellites
    required property WeatherModel weather
    property var skyPass: ({})
    property var obs: satellites.observation
    property var forecast: weather.forecast
    signal exportRequested
    signal photometryRequested
    spacing: 0
    function value(key, digits, suffix) {
        return obs[key] === undefined ? "待计算" : Number(obs[key]).toFixed(digits) + (suffix || "");
    }
    function weatherValue(key, unit, scale, digits) {
        return forecast[key] === undefined ? "暂无数据" : (forecast[key] / (scale || 1)).toFixed(digits || 0) + " " + unit;
    }
    function snapshotIndex() {
        for (let i = 0; i < satellites.snapshots.length; ++i)
            if (satellites.snapshots[i].id === satellites.snapshotId)
                return i;
        return 0;
    }
    component SectionTitle: Label {
        font.pixelSize: 13
        font.bold: true
        Layout.topMargin: 8
    }
    component Metric: ColumnLayout {
        property string label
        property string value
        property bool numeric: true
        spacing: 3
        Layout.fillWidth: true
        Label {
            text: parent.label
            color: Theme.muted
            font.pixelSize: 12
        }
        Label {
            text: parent.value
            font.family: parent.numeric ? Theme.numberFont : Theme.fontFamily
            font.pixelSize: 14
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.Wrap
        }
    }
    TabBar {
        id: tabs
        Layout.fillWidth: true
        onCurrentIndexChanged: Qt.callLater(function() { scroll.contentItem.contentY = 0; })
        DetailTab {
            text: "观测"
        }
        DetailTab {
            text: "轨道"
        }
        DetailTab {
            text: "天气"
        }
        DetailTab {
            text: "地影"
        }
        DetailTab {
            text: "资料"
        }
    }
    ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: scroll.availableWidth
            ColumnLayout {
                visible: tabs.currentIndex === 0
                Layout.fillWidth: true
                Layout.margins: Theme.inset
                spacing: 10
                Label {
                    text: pane.obs.name || "请选择卫星"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Button {
                    visible: pane.satellites.selectedId !== "0" && !pane.satellites.selectedWatched
                    text: "加入观测清单"
                    icon.source: "qrc:/icons/plus.svg"
                    icon.color: Theme.text
                    onClicked: pane.satellites.setWatched(pane.satellites.selectedId, true)
                }
                Label {
                    text: pane.obs.visibility || ""
                    color: pane.obs.elevation >= pane.clock.minimumElevation && pane.obs.illumination === 0 && pane.obs.sunElevation <= -6 ? Theme.accent : Theme.muted
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Label {
                    text: pane.skyPass.peakText ? "天空轨迹 · " + pane.skyPass.peakText : "天空轨迹"
                    color: Theme.muted
                    font.pixelSize: 11
                }
                SkyPlot {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 178
                    trajectory: pane.satellites.trajectory
                    observation: pane.obs
                    selectedTime: pane.clock.unixTime
                    passTime: pane.skyPass.peak || 0
                    minimumElevation: pane.clock.minimumElevation
                    gridColor: Theme.line
                    textColor: Theme.muted
                    futureColor: Theme.accent
                    pastColor: Theme.past
                }
                RowLayout {
                    Label {
                        text: "高度角"
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                    Label {
                        text: pane.value("elevation", 1, "°")
                        font.family: Theme.numberFont
                        font.pixelSize: 24
                        color: Theme.accent
                    }
                    Label {
                        text: pane.obs.elevationRate === undefined ? "" : pane.obs.elevationRate >= 0 ? "↑" : "↓"
                        color: Theme.accent
                        font.pixelSize: 18
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "方位角"
                            font.pixelSize: 12
                            color: Theme.muted
                        }
                        Label {
                            text: pane.value("azimuth", 1, "°") + " " + (pane.obs.direction || "")
                            font.family: Theme.numberFont
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignRight
                        }
                    }
                }
                RowLayout {
                    Label {
                        text: "距 " + pane.value("range", 0, " km")
                        font.family: Theme.numberFont
                        font.pixelSize: 14
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Label {
                        text: pane.obs.rangeRate === undefined ? "" : (pane.obs.rangeRate < 0 ? "接近 " : "远离 ") + Math.abs(pane.obs.rangeRate).toFixed(3) + " km/s"
                        font.family: Theme.numberFont
                        font.pixelSize: 14
                    }
                }
                SectionTitle {
                    text: "轨道状态"
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true
                    Metric {
                        label: "卫星高度"
                        value: pane.value("altitude", 2, " km")
                    }
                    Metric {
                        label: "运行速度"
                        value: pane.value("speed", 3, " km/s")
                    }
                    Metric {
                        label: "星下点纬度"
                        value: pane.value("latitude", 4, "°")
                    }
                    Metric {
                        label: "星下点经度"
                        value: pane.value("longitude", 4, "°")
                    }
                }
                SectionTitle {
                    text: "可见性"
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true
                    Metric {
                        label: "太阳高度角"
                        value: pane.value("sunElevation", 2, "°")
                    }
                    Metric {
                        label: "卫星受光"
                        value: pane.obs.lighting || "待计算"
                        numeric: false
                    }
                    Metric {
                        label: "视星等（估算）"
                        value: pane.obs.magnitude === undefined ? (pane.obs.magnitudeStatus || "待计算") : pane.value("magnitude", 1, " mag")
                        numeric: pane.obs.magnitude !== undefined
                    }
                    Metric {
                        label: "云量（预报）"
                        value: pane.weatherValue("cloud_cover", "%")
                    }
                }
                RowLayout {
                    Label {
                        text: "大气外 · 漫反射球模型"
                        font.pixelSize: 11
                        color: Theme.muted
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                    Button {
                        text: "星等参数"
                        icon.source: "qrc:/icons/settings-2.svg"
                        icon.color: Theme.text
                        enabled: pane.satellites.selectedId !== "0"
                        onClicked: pane.photometryRequested()
                    }
                }
                SectionTitle {
                    text: "无线电"
                }
                RowLayout {
                    Label {
                        text: "标称频率"
                        color: Theme.muted
                        font.pixelSize: 13
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: pane.satellites.receiveFrequency.toFixed(6)
                        selectByMouse: true
                        font.family: Theme.numberFont
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignRight
                        validator: DoubleValidator {
                            bottom: 0
                            top: 1000000
                            decimals: 6
                            locale: "C"
                        }
                        onEditingFinished: if (acceptableInput)
                            pane.satellites.receiveFrequency = Number(text)
                    }
                    Label {
                        text: "MHz"
                        font.pixelSize: 12
                    }
                }
                FieldRow {
                    label: "校正后"
                    value: pane.obs.doppler === undefined ? "待计算" : (pane.satellites.receiveFrequency + pane.obs.doppler / 1e6).toFixed(6) + " MHz"
                    valueSize: 15
                }
                FieldRow {
                    label: "Δf"
                    value: pane.obs.doppler === undefined ? "待计算" : (pane.obs.doppler >= 0 ? "+" : "") + (pane.obs.doppler / 1000).toFixed(3) + " kHz"
                }
                Label {
                    visible: pane.obs.heightEstimated === true
                    text: "观测计算暂按海拔 0 m 估算"
                    color: Theme.past
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Label {
                    visible: Math.abs(pane.obs.epochAge || 0) > 7
                    text: "根数历元与所选时刻相隔 " + Math.abs(pane.obs.epochAge || 0).toFixed(1) + " d，预报精度可能下降"
                    color: Theme.past
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
            }
            ColumnLayout {
                visible: tabs.currentIndex === 1
                Layout.fillWidth: true
                Layout.margins: Theme.inset
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
                        enabled: pane.satellites.selectedId !== "0"
                        onClicked: pane.satellites.copyDetails()
                    }
                    IconButton {
                        icon.source: "qrc:/icons/download.svg"
                        tip: "保存轨道根数"
                        enabled: pane.satellites.selectedId !== "0"
                        onClicked: pane.exportRequested()
                    }
                }
                Repeater {
                    model: pane.satellites.orbitFields
                    FieldRow {
                        required property var modelData
                        label: modelData.label
                        value: modelData.label.startsWith("历元（") ? modelData.value.replace("T", "\n") : modelData.value
                        valueSize: 13
                    }
                }
                SectionTitle {
                    visible: pane.satellites.relatedObjects.length > 1
                    text: "关联目标（" + pane.satellites.relatedObjects.length + "）"
                }
                Repeater {
                    model: pane.satellites.relatedObjects.length > 1 ? pane.satellites.relatedObjects : []
                    ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 3
                        Label {
                            text: modelData.name
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                        }
                        Label {
                            text: modelData.id + " · " + modelData.internationalId
                            color: Theme.muted
                            font.family: Theme.numberFont
                            font.pixelSize: 12
                        }
                    }
                }
            }
            ColumnLayout {
                visible: tabs.currentIndex === 2
                Layout.fillWidth: true
                Layout.margins: Theme.inset
                spacing: 12
                RowLayout {
                    Label {
                        text: "观测天气"
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    IconButton {
                        icon.source: "qrc:/icons/refresh-cw.svg"
                        tip: "更新天气"
                        enabled: pane.weather.enabled && !pane.weather.busy
                        onClicked: pane.weather.refresh()
                    }
                }
                Label {
                    text: pane.weather.status
                    color: Theme.muted
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }
                FieldRow {
                    label: "预报时刻"
                    value: pane.forecast.time || "暂无数据"
                }
                FieldRow {
                    label: "总云量"
                    value: pane.weatherValue("cloud_cover", "%")
                }
                FieldRow {
                    label: "低云量"
                    value: pane.weatherValue("cloud_cover_low", "%")
                }
                FieldRow {
                    label: "中云量"
                    value: pane.weatherValue("cloud_cover_mid", "%")
                }
                FieldRow {
                    label: "高云量"
                    value: pane.weatherValue("cloud_cover_high", "%")
                }
                FieldRow {
                    label: "水平能见度"
                    value: pane.weatherValue("visibility", "km", 1000, 1)
                }
                FieldRow {
                    label: "相对湿度"
                    value: pane.weatherValue("relative_humidity_2m", "%")
                }
                FieldRow {
                    label: "降水概率"
                    value: pane.weatherValue("precipitation_probability", "%")
                }
                FieldRow {
                    label: "获取时间"
                    value: pane.weather.fetchedAt || "暂无数据"
                    Layout.topMargin: 12
                }
                Label {
                    text: "数据来源：<a href='https://open-meteo.com/en/docs'>Open-Meteo</a>"
                    color: Theme.muted
                    font.pixelSize: 11
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label {
                    text: "天气数值来自气象模式预报。视星等取决于卫星标准星等、距离和相位角；薄云、消光和姿态变化会影响实际亮度。"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                    color: Theme.muted
                }
            }
            ColumnLayout {
                visible: tabs.currentIndex === 3
                Layout.fillWidth: true
                Layout.margins: Theme.inset
                spacing: 6
                Label {
                    text: "地影事件"
                    font.bold: true
                }
                Repeater {
                    model: pane.satellites.shadowEvents
                    ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 36
                        contentItem: RowLayout {
                            Label {
                                text: modelData.timeText
                                font.family: Theme.numberFont
                                font.pixelSize: 13
                                Layout.fillWidth: true
                            }
                            Label {
                                text: modelData.name
                                font.pixelSize: 13
                                color: Theme.muted
                            }
                        }
                        onClicked: pane.clock.seek(modelData.time)
                    }
                }
                Label {
                    visible: pane.satellites.shadowEvents.length === 0
                    text: "本时段内暂无地影进出事件"
                    font.pixelSize: 12
                    color: Theme.muted
                }
            }
            ColumnLayout {
                visible: tabs.currentIndex === 4
                Layout.fillWidth: true
                Layout.margins: Theme.inset
                spacing: 12
                Label {
                    text: "轨道资料"
                    font.bold: true
                }
                FieldRow {
                    label: "根数历元（UTC）"
                    value: pane.satellites.elementEpoch.replace("T", " ") || "来源未注明"
                }
                Label {
                    text: "本地获取时间"
                    color: Theme.muted
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: pane.satellites.snapshots
                    textRole: "label"
                    valueRole: "id"
                    currentIndex: pane.snapshotIndex()
                    onActivated: pane.satellites.loadSnapshot(currentValue)
                }
                Label {
                    text: pane.snapshotIndex() > 0 ? "历史根数回放" : "使用最近获取的根数"
                    color: Theme.accent
                    font.pixelSize: 13
                }
                Label {
                    text: "数据来源"
                    color: Theme.muted
                }
                TextEdit {
                    text: pane.satellites.sourceText
                    Layout.fillWidth: true
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: Theme.text
                    font.pixelSize: 12
                }
                Label {
                    text: "资料更新可能存在延迟。根数历元表示轨道参数的参考时刻，获取时间表示资料保存到本机的时间。"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Label {
                    text: "根数按协调世界时记时，观测时间采用地点时区。光学条件按卫星受阳光照射、太阳高度角低于 -6° 筛选；实际可见性还与星等、天气和地形有关。"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.pixelSize: 12
                }
                SectionTitle {
                    text: "星等资料"
                }
                FieldRow {
                    label: "来源"
                    value: pane.satellites.photometry.source || "暂无参考星等"
                    numeric: false
                }
                FieldRow {
                    label: "资料日期"
                    value: pane.satellites.photometry.sourceDate || "来源未注明"
                }
                FieldRow {
                    visible: pane.satellites.photometry.builtin === true
                    label: "采用资料"
                    value: "内置参考值"
                    numeric: false
                }
                FieldRow {
                    visible: pane.satellites.photometry.builtin !== true
                    label: pane.satellites.photometry.manual ? "记录时间" : "导入时间"
                    value: pane.satellites.photometry.recordedAt || "尚无记录"
                }
                SectionTitle {
                    text: "天气资料"
                }
                FieldRow { label: "来源"; value: "Open-Meteo" }
                FieldRow { label: "预报时刻"; value: pane.forecast.time || "暂无数据" }
                FieldRow { label: "获取时间"; value: pane.weather.fetchedAt || "尚无记录" }
                SectionTitle {
                    text: "地图与高程"
                }
                Label {
                    text: "Natural Earth，1:5000 万\n陆地 4.1.0 · 湖泊 5.0.0\n国界 5.1.0 · 城市 5.1.2\n高程：Open-Meteo / Copernicus DEM 2021 GLO-90\n大地水准面：NGA / GeographicLib EGM2008，5′ 格网\n格网文件日期：2009-08-29"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.pixelSize: 12
                    lineHeight: 1.5
                }
                SectionTitle { text: "参考资料" }
                Label {
                    text: "以下为截至 2026-09-22 核实的公开资料版本。当前计算采用的资料以上方记录为准，各来源更新可能存在延迟。"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Repeater {
                    model: [
                        {name: "IGS 卫星元数据", url: "https://files.igs.org/pub/station/general/igs_satellite_metadata.snx", date: "版本日期：2026-09-02"},
                        {name: "GSC 伽利略星座与槽位资料", url: "https://www.gsc-europa.eu/system-service-status/constellation-information", date: "资料核对日期：2026-09-22"},
                        {name: "GSC 伽利略历书", url: "https://www.gsc-europa.eu/gsc-products/almanac", date: "样本发布日期：2026-09-18"},
                        {name: "北斗测试评估中心星座状态", url: "https://www.csno-tarc.cn/status/constellation", date: "状态表发布时间：2026-09-22"},
                        {name: "McCants / QuickSat 星等表", url: "https://www.mmccants.org/programs/qsmag.zip", date: "2020 版文件日期：2020-09-14"},
                        {name: "Stellarium 卫星合并表", url: "https://github.com/Stellarium/stellarium-data/tree/master/satellites", date: "文件更新：2026-09-11；星等含历史观测"},
                        {name: "SCORE 卫星测光资料", url: "https://score.cps.iau.org/", date: "核对时库内观测截至：2026-09-22"},
                        {name: "SeeSat-L 中国空间站测光记录", url: "https://www.satobs.org/seesat/Aug-2022/0030.html", date: "报告日期：2022-08-03；对应当时构型"}
                    ]
                    ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 3
                        Label {
                            text: "<a href='" + modelData.url + "'>" + modelData.name + "</a>"
                            textFormat: Text.RichText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            onLinkActivated: link => Qt.openUrlExternally(link)
                        }
                        Label {
                            text: modelData.date
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 11
                            color: Theme.muted
                        }
                    }
                }
                SectionTitle { text: "计算与许可" }
                Label {
                    text: "轨道传播：Vallado SGP4\n太阳位置：Astronomy Engine\n第三方许可与署名：程序目录中的 THIRD_PARTY_NOTICES.md 和 licenses 文件夹。"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.pixelSize: 12
                    lineHeight: 1.5
                }
            }
        }
    }
}
