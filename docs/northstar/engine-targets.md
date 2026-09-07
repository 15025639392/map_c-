# 地球引擎北极星目标 — map_cplus 摘要（从 gis-md 提取）

> 提取自 `/Users/yan/Desktop/work/gis-md`（commit `1b7e2907`，2026-09-07）。
> 北极星 = 「做到什么程度算好」的跨会话判据文档（`gis-md/docs/northstar/*.md`）。
> 本文是**目标摘要**：每条一句话 + 归属文档 + 本仓优先级；判据全文以各模块文档为准
> （地形 = [terrain.md](terrain.md) + [terrain-gis-md-baseline.md](terrain-gis-md-baseline.md)）。
> gis-md 为只读参考源；本仓实现按 [roadmap.md](../roadmap.md) 从 0 推进。

---

## 0. 引擎总目标形态（来自 gis-md README，原样提取）

- **C++17 移动优先的 3D 地球引擎**，自研内核，不套 Cesium/osgEarth，只把它们当算法对照系。
- 目标形态 = 可嵌入 App 的 `earth_engine_core` 静态库 + 各平台薄壳 demo。
- 能力：球面地形流式加载、影像瓦片合成、运行期可换样式的矢量图层、手势相机。
- 本仓对地形的定义与此一致：**地形是引擎最重的模块**（gis-md 中 tiling 34k 行 + content 17k 行，
  瓦片树/LOD/调度/页存储/位移模板都在地形链路里），故本仓把地形列为最高优先。

## 1. 各模块北极星一句话（gis-md 原文，含出处链接）

| 模块 | 北极星一句话（摘要） | 判据全文（gis-md docs/northstar） | 本仓优先级 |
|---|---|---|---|
| **地形** | 任意机位任意加载阶段地貌可辨（山脊/沟谷/坡面明暗），瓦界与换代不可见；每字节下载/每次烘焙/每个三角形都有像素在屏上消费 | `terrain.md`（已提取到本仓） | ★★★ 阶段 6（最高优先） |
| **矢量** | 45° 斜视 + 地平线在屏的 3D 地形上路网/面/标注贴地不浮、换代不闪、换肤不刷；线宽与标注屏幕像素恒定；整条链每级有预算、失效工作有上限 | `vector.md` | ★ 阶段 5 |
| 高德矢量链路 | 全球/近景/连续缩放视野地理覆盖与层级正确；数据从版本探测到 GPU 提交可追踪可收敛无重复工作；细瓦未就绪旧覆盖顶住，异步迟到不制造空洞/重影/错位/假收敛 | `amap-vector.md` | ○ 供应商专属，按需接入 |
| **影像** | 影像缺失时屏幕永不出现空白：退化路径是「更低分辨率的真实影像」而非「什么都不画」，且复用祖先纹理+UV 窗口、不为空洞多传一张纹理 | `imagery.md` | ★★ 阶段 3/4 |
| 光照/颜色 | 日落时天/地/太阳盘三者暖度同步推进；亮部可超显示范围被 tonemap 优雅压回；颜色进出空间单一治理 | `lighting.md` | ★ 阶段 9 |
| 相机/手势 | 手指按住地表一点，该点死死钉在指下（亚像素锚定）；惯性收敛不跑飞；不穿地不锁死病态俯仰；组合手势各轴独立 | `camera.md` | ★ 阶段 2/7 |
| 天气 | 云是世界里的东西：固定地理位置与海拔带、被山挡、远处融进空气透视、随太阳受光/入夜变暗；天气状态可切换且过渡连续 | `weather.md` | ○ 远期 |
| 资源调度→上屏链路 | （横切北极星，目标形态定义）七段流水线：需求侧每帧只声明“要什么”、供给侧按页/字节预算收敛到屏上；资源失败逐级退化不穿帮 | `pipeline.md` | ★★ 随各阶段横切遵守 |

## 2. 北极星三轴记账原则（本仓沿用，防止"看着绿、钱花冤"）

gis-md 的地形北极星是**四轴**形态：体验 / 性能 / 资源占用 / **资源调度效率**。
调度效率轴是教训的产物——T-E1 揭示：帧时正常、内存有界、画面能看，但**付了 8 倍数据只用 1 倍**
（源 514² 烘成 65²，利用率 1/62）。本仓任何"变快/变省/更清楚"的结论都必须带
**证据**（commit / 单测 / 计数 / 截图 / 真机数据），不许凭印象写"应该很小"。

## 3. 本仓目标路线（详细见 roadmap.md）

1. **阶段 0–1（当前）**：项目骨架 + 核心数学/坐标（椭球、Cartographic、ENU、Mat4、Ray、Rectangle）——
   地形所需的第一块地基。host native 单测已绿（2026-09-08）。
2. **阶段 2–4**：可旋转地球、瓦片方案（WebMercator/Geographic/XYZ-TMS）、单源底图流式。
3. **阶段 6（重点）**：地形——按 terrain.md 判据自写瓦片树/LOD/高度图链路；
   到这一步时**并入 gis-md 现成地形服务**（其命名空间/布局与本仓同构，见 roadmap「合并点」），
   而非重写数据服务。
4. 每到一个里程碑，回填本文「当前状态」与 terrain.md 跟踪表。

## 4. 当前状态（2026-09-08）

- 仓库骨架、构建（host native, cmake+ninja+googletest + gis-md vcpkg stb 头）、
  29 个 gtest 套件全绿（含 M-mid 型固定机位 host 基线）。
- 已完成：Vec3/Vec2/Mat4/MathUtils/Ray/RayTriangle/Rectangle；Ellipsoid（WGS84 双向转换、
  法线、地表投影）；Cartographic；Transforms（ENU↔ECEF）；射线-椭球求交；
  Geographic/WebMercator 投影；瓦片键/四叉树 + WebMercatorTileScheme + SSE；
  高度图编解码 + 规则网格采样 + HeightmapTile + TerrainTileMesh + TerrainLodSelector +
  TerrainDataSource/TerrainFrameAssembler（host 地形帧主链路）；CameraView + TerrainPicking +
  TerrainCameraPipeline + 跨层级共享网格点一致测试 + 固定机位基线；providers：URL 模板 +
  字节源 + Terrain-RGB/RGB + **PNG 全链路**；terrain.md「host 机制证据」节已立。
- 未开始：curl HTTP 网络源、渲染抽象；地形判据仍全部 ❌（见 terrain.md 跟踪表）。
