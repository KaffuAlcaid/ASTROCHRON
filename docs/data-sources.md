# 数据来源

[返回首页](../README.md) · [计算说明](calculations.md) · [第三方声明](../THIRD_PARTY_NOTICES.md)

适用版本：`v0.1.0-preview`，公开参考资料核对截至 **2026-09-22**

轨道根数以卫星历元为准，测光资料以原始观测日期或来源日期为准，网页查询、文件更新和本地获取时间分别记录，资料更新可能存在延迟

## 应用采用的数据

| 数据 | 来源 | 版本与日期 |
| --- | --- | --- |
| 卫星轨道根数 | [CelesTrak GP](https://celestrak.org/NORAD/elements/) 或导入的 OMM / TLE 文件 | 各卫星历元见“轨道”页，本地获取时间见“资料”页 |
| 基础参考星等 | [Mike McCants / QuickSat](https://www.mmccants.org/programs/qsmag.zip) | 内置 `qs.mag` 文件日期 2020-09-14 |
| 中国空间站参考星等 | [Jay Respler / SeeSat-L](https://www.satobs.org/seesat/Aug-2022/0030.html) | 报告日期 2022-08-03，对应当时构型 |
| 天气 | [Open-Meteo Weather Forecast API](https://open-meteo.com/en/docs) | 按小时预报，界面显示预报时刻与获取时间 |
| 世界地图与城市 | [Natural Earth](https://www.naturalearthdata.com/)，1:5000 万 | 陆地 4.1.0、湖泊 5.0.0、国界 5.1.0、城市 5.1.2 |
| 地形海拔 | [Open-Meteo Elevation API](https://open-meteo.com/en/docs/elevation-api) / Copernicus DEM | 2021 GLO-90，约 90 m 分辨率，垂直基准 EGM2008 |
| 大地水准面 | [NGA / GeographicLib EGM2008](https://geographiclib.sourceforge.io/C++/doc/geoid.html) | 5′ 格网，文件日期 2009-08-29 |

地图、城市、大地水准面和默认星等表随程序提供，轨道数据从 CelesTrak 获取或由用户导入，天气和地形海拔通过联网查询获取

### 轨道目录

CelesTrak 按数据分组提供 GP JSON，程序保留 NORAD ID、COSPAR 国际编号、历元及轨道参数，并将获取结果保存为本地快照

同一份文件或快照含有相同 NORAD ID 的多条有效根数时，采用历元最新的一条；历元相同时保留文件中先出现的记录，原始文件内容随快照保存

OMM 使用 UTC、SGP4 与 TEME 参考系，`TIME_SYSTEM`、`MEAN_ELEMENT_THEORY`、`REF_FRAME` 省略时按这些约定读取，显式填写时须与约定一致；载入提示给出异常记录的原因，部分记录可用时另列出跳过的异常记录数，多条异常仅显示最后一条原因

本地目录汇集已获取的对象，观测清单保存用户选择的目标，来源分组记录数对应某一次下载结果，这些数量各有含义，实际导航服务状态以官方星座状态资料为准

空间站已对接舱段按组合体归组，中国空间站以天和核心舱 `TIANHE / CSS` 为代表，具有独立轨道的航天器保留为单独对象

成功获取的同一来源采用至少 2 h 的下载间隔，失败后的手动重试通常至少等待 60 s，并遵循服务端 `Retry-After` 指定的更长等待时间

### 星等资料

QuickSat 的参考条件为距离 1000 km、满相时的最大亮度，内置表按 NORAD ID 和 COSPAR 国际编号共同匹配

中国空间站采用 2022-08-03 报告中三次观测得到的 QuickSat 本征星等平均值 **0.87 mag**，对应 2022 年当时的空间站构型，舱段、对接飞船和姿态变化均会影响亮度

手动参数优先于导入资料，导入资料优先于内置资料，导入星等表时保留已有手动参数，来源日期与本地保存时间分开显示，计算方法见[视星等](calculations.md#视星等)

### 地图与城市

使用 Natural Earth 的 `land`、`lakes`、`admin_0_boundary_lines_land` 和完整的 `populated_places` 图层，城市属性包含多语言名称

地图采用等距圆柱投影，经度与纬度以矩形坐标显示，Natural Earth 城市用于地图标注和地点搜索，具体观测位置可继续通过坐标或地图选点确定

### 天气与海拔

天气查询的范围参数为过去 1 天、预报 2 天，按小时取值，定期刷新间隔为 1 h；可查阅的实际时段以返回的预报资料为准

地形海拔采用 Copernicus DEM，建筑物内或楼顶观测可手动填写高度，程序结合 EGM2008 大地水准面起伏换算观测者的椭球高

## GNSS 与测光参考资料

以下资料用于查阅身份、PRN 分配历史、轨道面、服务状态与亮度观测；本版自动获取的数据范围见上表，参考网页的内容可结合应用中的卫星编号查阅

| 资料 | 来源 | 日期说明 |
| --- | --- | --- |
| GNSS 编号、轨道面、槽位与分配历史 | [IGS 卫星元数据](https://files.igs.org/pub/station/general/igs_satellite_metadata.snx) | 文件版本日期 2026-09-02 |
| 伽利略运行状态 | [GSC 星座状态](https://www.gsc-europa.eu/system-service-status/constellation-information) | 核对日期 2026-09-22 |
| 伽利略轨道面与槽位 | [GSC 轨道与技术参数](https://www.gsc-europa.eu/system-service-status/orbital-and-technical-parameters) | 核对日期 2026-09-22 |
| 伽利略历书 | [GSC Almanac](https://www.gsc-europa.eu/gsc-products/almanac) | 样本发布日期 2026-09-18 |
| 北斗身份、运行和健康状态 | [北斗测试评估中心](https://www.csno-tarc.cn/status/constellation) | 状态表发布时间 2026-09-22 |
| 星等与雷达截面积合并表 | [Stellarium 卫星资料](https://github.com/Stellarium/stellarium-data/tree/master/satellites) | 文件更新日期 2026-09-11，其中包含历史测光记录 |
| 卫星亮度实测 | [SCORE](https://score.cps.iau.org/) | 核对时库内观测截至 2026-09-22，每条观测单独记时，CC BY 4.0 |

一条亮度观测对应特定时刻、距离、相位与姿态，将实测亮度用于参考星等还需明确换算条件

## 联网与本地保存

| 操作 | 发送的信息 | 本地保存 |
| --- | --- | --- |
| 获取轨道 | CelesTrak 分组名称 | 轨道资料、来源与获取时间 |
| 查询天气 | 观测地点经纬度、预报变量和时间范围 | 当前会话使用的预报 |
| 查询海拔 | 观测地点经纬度 | 观测地点海拔与来源 |
| 导入轨道或星等 | 本地读取所选文件 | 导入资料与日期 |

轨道与星等数据库为 `%LOCALAPPDATA%\ASTROCHRON\ASTROCHRON\orbits.sqlite`，观测地点、清单和界面设置保存在 Windows 用户配置中

应用文件夹可单独移动，已有本地轨道资料时可离线查看地图、推算轨迹与过境，天气和在线目录查询需要网络连接

各数据的署名、原始分发说明和许可文件位置见[第三方声明](../THIRD_PARTY_NOTICES.md)
