# ASTROCHRON · 星纪

星纪是一个面向卫星跟踪与观测规划的桌面应用项目，以 SGP4/SDP4 轨道传播为基础，采用高信息密度、层级清晰的中文界面。

项目围绕以下功能展开：

* **全球卫星轨迹**：在可展开、缩放和平移的地图上查看轨迹，自由选择城市、昼夜线、覆盖范围等图层。
* **24 小时时间推演**：查看当前时刻前后各 12 小时的位置，联动地图、天空图和观测信息。
* **过境观测**：按观测地点查看方位角、高度角、距离，以及过境开始、最高点和结束的时间。
* **完整轨道信息**：保留卫星编号、历元、轨道根数与运动状态，方便查阅和比较。
* **本地观测地点**：通过城市选择、经纬度输入或地图选点保存常用位置。

首版面向 Windows 11，使用 C++ 与 Qt 开发。
轨道数据来自 [CelesTrak](https://celestrak.org/NORAD/elements/)，地图数据来自 [Natural Earth](https://www.naturalearthdata.com/)。

参考项目：[ShenMian/tracker](https://github.com/ShenMian/tracker)。

本项目源码采用 [Apache-2.0 许可](LICENSE)。第三方软件与数据的许可见 [第三方声明](THIRD_PARTY_NOTICES.md)。
