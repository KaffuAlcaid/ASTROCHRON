# 构建

## 地图资源

环境要求：Node.js 24 或更高版本。

```powershell
npm ci --ignore-scripts
npm run prepare:map
```

原始数据位于 `assets/natural-earth/source/`，生成资源默认写入 `.cache/generated/world-map.json`。指定输出文件：

```powershell
node tools/prepare-map.mjs .cache/generated/world-map.json
```

资源包含 Plate Carree 平面坐标下的陆地和湖泊三角网格、国界线，以及城市属性。两级几何细节分别采用约 15 千米和 2.5 千米的简化间隔。

原始数据保持 WGS84 经纬度。绘图坐标以角度为单位，横轴朝东、纵轴朝南。跨越正负 180 度经线的多边形由投影工具切分，城市位置保留独立的经度与纬度。

## 本地工具目录

便携工具链位于 `.cache/toolchains/`，与临时构建产物一起由 Git 忽略。`node_modules/` 由 `package-lock.json` 重建。
