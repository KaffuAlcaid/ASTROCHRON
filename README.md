<p align="center">
  <img src="assets/app-icon/astrochron-256.png" alt="ASTROCHRON 图标" width="112" height="112">
</p>

<h1 align="center">ASTROCHRON · 星纪</h1>

<p align="center">用于卫星跟踪、过境预测和观测规划的桌面应用</p>

<p align="center">简体中文 | <a href="README.en.md">English</a></p>

![星纪主界面：卫星地图、观测信息与过境预报](docs/images/overview.png)

## 下载

适用于 Windows 11 x64，前往 [Releases](https://github.com/KaffuAlcaid/ASTROCHRON/releases/latest) 下载 Windows 安装器或免安装 ZIP

使用 ZIP 时，请完整解压后运行 `ASTROCHRON.exe`

## 功能

- **卫星跟踪**：在地图和天空图中查看卫星位置与轨迹，浏览当前时刻前后各 12 小时的位置变化
- **过境预报**：根据观测地点预测卫星过境时间、最高高度角和光学观测条件
- **观测计划**：收藏常用卫星，将过境计划导出为 CSV 表格或 ICS 日历文件
- **观测信息**：查看卫星方位、高度角、距离、估算亮度和当地天气，计算无线电多普勒频移
- **轨道数据**：从 CelesTrak 获取轨道数据，或导入 TLE / OMM 文件，保存和查阅历史轨道资料

支持简体中文和英文界面

## 快速开始

1. 启动程序，选择界面语言并设置观测地点
2. 联网获取轨道数据，或在“卫星目录”中导入 TLE / OMM 文件，然后选择要观测的卫星
3. 查看过境预报，点击一条记录查看该次过境，再拖动时间轴浏览卫星位置的变化

## 文档

| 文档 | 内容 |
| --- | --- |
| [使用手册](docs/user-guide.md) | 安装与更新、观测操作和参数设置 |
| [数据来源](docs/data-sources.md) | 数据提供方、资料日期与本地保存 |
| [计算说明](docs/calculations.md) | 轨道传播、过境、视星等与多普勒计算 |
| [SGP4 轨道传播原理](docs/orbit-model.md) | 从平均根数到 TEME 状态及本地观测方向的计算过程 |

## 数据与计算

卫星轨道数据来自 [CelesTrak](https://celestrak.org/NORAD/elements/)，轨道传播采用 Vallado SGP4 实现，支持近地与深空轨道

地图与城市数据来自 [Natural Earth](https://www.naturalearthdata.com/)，天气和地形海拔由 [Open-Meteo](https://open-meteo.com/) 提供，亮度估算参考 McCants / QuickSat 星等表及中国空间站历史测光资料

轨道预测会受轨道数据的时效性和卫星机动影响，时间轴中的过去位置也由模型推算，实际亮度还会随卫星姿态、构型和大气条件变化

## 致谢

参考项目：[ShenMian/tracker](https://github.com/ShenMian/tracker)

制作灵感来自《仰望夜空的星辰》（見上げてごらん、夜空の星を）

## 许可证

ASTROCHRON 源码采用 [Apache-2.0](LICENSE) 许可

第三方软件与数据遵循各自的许可和使用条款，详见[第三方声明](THIRD_PARTY_NOTICES.md)
