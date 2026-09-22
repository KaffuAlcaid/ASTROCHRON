# ASTROCHRON · 星纪

星纪是一个面向卫星跟踪与观测规划的桌面应用项目，以 SGP4/SDP4 轨道传播为基础，采用高信息密度、层级清晰的中文界面。

项目围绕以下功能展开：

* **全球卫星轨迹**：在可展开、缩放和平移的地图上查看轨迹，自由选择城市、昼夜线、覆盖范围等图层。
* **24 小时时间推演**：查看当前时刻前后各 12 小时的位置，联动地图、天空图和观测信息。
* **过境观测**：按观测地点查看方位角、高度角、距离，以及过境开始、最高点和结束的时间。
* **完整轨道信息**：保留卫星编号、历元、轨道根数与运动状态，方便查阅和比较。
* **本地观测地点**：通过城市选择、经纬度输入或地图选点保存常用位置。

首版面向 Windows 11，使用 C++ 与 Qt 开发。

## 数据来源与日期

资料更新可能存在延迟。轨道根数以各卫星的历元为准；星等资料采用来源标明的发布日期或观测截止日期。本地获取、导入和记录时间单独显示。

| 数据 | 来源 | 日期与版本 |
| --- | --- | --- |
| 轨道根数 | [CelesTrak](https://celestrak.org/NORAD/elements/) 或本地轨道文件 | 各卫星历元与本地获取时间见“资料”页 |
| 参考星等 | 内置 McCants / QuickSat 星等表、SeeSat-L 中国空间站测光记录；支持手动参数和文件导入 | QuickSat 文件日期 2020-09-14；中国空间站报告日期 2022-08-03，适用于当时构型。当前采用的来源与日期见“资料”页 |
| 观测天气 | [Open-Meteo](https://open-meteo.com/en/docs) | 预报时刻与获取时间随当前资料显示 |
| 世界地图与城市 | [Natural Earth](https://www.naturalearthdata.com/)，1:5000 万 | 陆地 4.1.0、湖泊 5.0.0、国界 5.1.0、城市 5.1.2 |
| 地形高程 | [Open-Meteo / Copernicus DEM](https://open-meteo.com/en/docs/elevation-api) | 2021 GLO-90，垂直基准 EGM2008 |
| 大地水准面 | [NGA / GeographicLib EGM2008](https://geographiclib.sourceforge.io/C++/doc/geoid.html) | 5′ 格网，文件日期 2009-08-29 |

### 参考资料

以下为截至 **2026-09-22** 核实的公开资料版本，供查阅卫星身份、运行状态和测光数据。当前计算采用的资料以应用“资料”页的来源与日期为准。

| 资料 | 来源 | 日期说明 |
| --- | --- | --- |
| GNSS 编号、轨道面、槽位及分配历史 | [IGS 卫星元数据](https://files.igs.org/pub/station/general/igs_satellite_metadata.snx) | 版本日期 2026-09-02 |
| 伽利略运行状态、轨道面与槽位 | [GSC 星座状态](https://www.gsc-europa.eu/system-service-status/constellation-information)、[轨道与技术参数](https://www.gsc-europa.eu/system-service-status/orbital-and-technical-parameters) | 资料核对日期 2026-09-22 |
| 伽利略历书 | [GSC Almanac](https://www.gsc-europa.eu/gsc-products/almanac) | 样本发布日期 2026-09-18 |
| 北斗身份、运行和健康状态 | [北斗测试评估中心](https://www.csno-tarc.cn/status/constellation) | 状态表发布时间 2026-09-22 |
| 基础星等目录 | [McCants / QuickSat](https://www.mmccants.org/programs/qsmag.zip) | 2020 版文件日期 2020-09-14；参考条件为 1000 km、满相时的最大亮度 |
| 星等与雷达截面积合并表 | [Stellarium](https://github.com/Stellarium/stellarium-data/tree/master/satellites) | 文件更新 2026-09-11；其中星等记录包含历史观测 |
| 卫星亮度实测 | [SCORE](https://score.cps.iau.org/) | 核对时库内观测截至 2026-09-22；每条观测分别记时 |
| 中国空间站历史测光 | [SeeSat-L](https://www.satobs.org/seesat/Aug-2022/0030.html) | 报告日期 2022-08-03，对应当时的空间站构型 |

公开资料的文件更新日期、观测截止日期和查询日期含义各异。卫星姿态、构型和服务状态随时间变化，光学观测还受大气与天气影响。

内置参考星等随程序提供，按卫星编号与国际编号匹配后自动参与计算。视星等采用漫反射球模型，结合实际距离与相位角估算大气外亮度。手动参数及导入资料优先；“恢复默认”使用对应的内置参考值。

参考项目：[ShenMian/tracker](https://github.com/ShenMian/tracker)。

本项目源码采用 [Apache-2.0 许可](LICENSE)。第三方软件与数据的许可见 [第三方声明](THIRD_PARTY_NOTICES.md)。
