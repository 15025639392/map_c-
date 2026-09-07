# map_cplus 从 0 重建路线图（地形为最高优先）

> 阶段划分基于 gis-md `docs/gis/earth-engine-roadmap.md`（来源 commit `1b7e2907`），
> 但验收口径针对本仓的 **host native + 单测优先** 形态做了改写（本仓目前无 GPU/真机链路；
> 渲染后端待阶段 2+ 再决策，不影响核心算法先行）。
> 总原则（沿用 gis-md 路线图，本仓写死）：
> **先让地球正确出现 → 再让数据正确叠加 → 最后做复杂效果；先可测的数学与接口 → 再接真实网络数据；
> 单图层单 scheme → 多图层多 CRS；每阶段保持可运行；禁止跳跃。**
>
> 相邻文档：[北极星目标摘要](northstar/engine-targets.md) ·
> [地形判据活文档](northstar/terrain.md) · [gis-md 地形基线](northstar/terrain-gis-md-baseline.md)

---

## 0. 本仓与 gis-md 的关系（规则，先读）

| 事项 | 规则 |
|---|---|
| 代码来源 | **从 0 自写**。gis-md 是只读参考源（读架构/判据/根因档案），不直接拷贝实现（地形服务除外，见下） |
| 命名空间 / 布局 | 命名空间 `earth_engine`，目录 `src/earth_engine/{core,tiling,content,…}` 与 gis-md **同构**；核心库名 `earth_engine_core`——为阶段 6 无摩擦并入现成地形服务 |
| **地形服务合并点** | 阶段 6：把 gis-md 现成的**地形服务**（DEM 解码 / 高度图 provider / 瓦片树 / LOD / 位移模板链路）并入复用，只重写"接入本仓的部分"；这来自用户指示「地形服务使用 gis-md 现成的」 |
| 判据契约 | 地形北极星判据全文 = `docs/northstar/terrain-gis-md-baseline.md`；本仓状态跟踪 = `docs/northstar/terrain.md` |
| 依赖 | host 优先：cmake/ninja 复用 Android SDK 自带；googletest 走 FetchContent（env.sh 可离线复用 gis-md 源码缓存）；glm/nlohmann-json/curl 等在需要时先复用 gis-md vcpkg installed（只读），否则再 vcpkg/FetchContent |
| 单测 | gtest，每个 `tests/unit/<模块>/test_*.cpp` 一个 ctest 用例；`./test_native.sh` 是唯一最短回路 |
| 提交/推送 | 里程碑提交；开发成熟后推送 `git@github.com:15025639293/map_c.git`（origin/main） |
| 帧收敛纪律（远期） | 引擎是**按需渲染**时，任何依赖帧循环推进的异步收敛必须向帧门控申报（WorkLedger 票或状态谓词），禁止帧数节流；gis-md 六个洞的教训，见 baseline T-E4 |

## 1. 阶段进度表

| 阶段 | 目标 | 本仓验收（host） | 状态 |
|---|---|---|---|
| **0** | 项目骨架：CMake/presets/env/test_native.sh、模块目录、gtest 冒烟 | `cmake --preset native-tests && ctest` 全绿 | ✅ 2026-09-08 |
| **1** | 核心数学与坐标：WGS84 椭球、Cartographic、Vec3、Mat4、Ray、Rectangle、Transforms(ENU)、ECEF↔cartographic | geodesy/core 单测全绿（赤道/极区/高海拔/负高全覆盖） | ✅ 2026-09-08（见下） |
| **2** | 可旋转地球：相机模型（位置/朝向）、视线-椭球求交、射线拾取地基、渲染抽象（RenderDevice 接口层，先 host 空实现 + 用例） | 求交/拾取/矩阵单测绿；无 GPU 的帧状态机单测 | 🔄 部分（求交已落地，相机/渲染待续） |
| **3** | 单一 XYZ 底图：WebMercator/Geographic tile scheme、瓦片键/四叉树、Provider 接口 + HTTP（先本地 fixture） | scheme/投影/四叉树/提供者单测绿 | 🔄 部分（瓦片键/四叉树/scheme/SSE 已落地，Provider/HTTP 待续） |
| **4** | 多图层与多瓦片体系：TMS/XYZ/WMTS 差异、矩形覆盖/交叉、SSE(LOD 几何误差)、调度与预算骨架 | 调度/预算/选择器单测绿 | ⬜ |
| **5** | 矢量叠加与样式（低优先，先保地形主线） | — | ⬜ |
| **6** | **地形（最高优先）**：按 terrain.md 判据落地；**并入 gis-md 现成地形服务**；高度图 → 瓦片树 → LOD → 无缝（瓦界 <1m / 换代不可见 / 利用率 ≥1/4 于 T-E1） | terrain 判据表逐条回填 ✅ 需 host 可跑证据；观感类标注 🔒 待用户上屏拍板 | ⬜ |
| 7–10 | 绘制/测量/编辑、3D Tiles、环境系统、性能离线工程化 | 按需 | ⬜ |

## 2. 已完成内容（阶段 1 + 阶段 2/3 前置）

- `src/earth_engine/core/math/`：`MathUtils.h`（常量/角度换算/wrapLongitude/equalsEpsilon）、
  `Vec3.h`、`Mat4.{h,cpp}`（列主序 + Gauss-Jordan 逆）、`Ray.h`、`Rectangle.h`（弧度制）。
- `src/earth_engine/core/geodesy/`：`Cartographic.h`（弧度制，度构造助手）、
  `Ellipsoid.{h,cpp}`（WGS84 双向转换、大地法线、scaleToGeodeticSurface=法线垂足）、
  `Transforms.{h,cpp}`（ENU↔ECEF 刚体帧）。
- 单测：`tests/unit/{core,geodesy}/` 8 个 gtest 套件，覆盖赤道/两极/高海拔/负高往返、ENU 轴与
  有限差分互验、矩阵逆/转置、奇异矩阵、零向量归一化等边界。
- 设计决策（防呆）：
  - **Cartographic 内部一律弧度**，只有 `fromDegrees` 收度 → 度/弧度误用被单测钉住。
  - 高度单位米、可为负（地下）；ENU 帧的 up 与高度无关（大地法线）。
  - `scaleToGeodeticSurface` = 去大地高后的 ECEF（法线垂足），与正/反转换互为精确逆。

### 阶段 2/3 前置增量（2026-09-08）

- `core/math`：新增 `Vec2.h`（平面坐标基础类型，header-only）。
- `core/geodesy`：
  - `RayEllipsoid.{h,cpp}`——射线-椭球求交（缩放球二次方程）。入/出双根、起点在球内语义、
    `firstPositiveT()`（相机/拾取正向第一交点）；判别式噪声容忍用**相对项量级**判据
    （真实"微小错过"如距表面 1 m 平飞必须判 miss，绝对阈值会把它吞成相切——测试钉住了这条）。
  - `Projection.{h,cpp}`——`Projection` 接口 + `GeographicProjection`（等距圆柱 x=λa,y=φa）+
    `WebMercatorProjection`（EPSG:3857，纬度钳制 ±85.05112878°，正方形世界半宽 πa）+
    北向导数 `northSouthMetersPerRadian`（瓦片地面分辨率/SSE 将来复用）。
- 单测 +3 套件：`test_vec2` / `test_ray_ellipsoid` / `test_projection`，覆盖
  直下命中/极区/外切 miss/内部起点/300 km 相机对地命中（相对误差 ≤1e-9）、
  往返精度、180° 与纬度上限已知值、单调性、有限差分导数互验。当前共 11 套件全绿。
- 下一步：阶段 2 的相机模型与拾取地基、阶段 3 的瓦片键/四叉树与 Provider。

### 阶段 3 瓦片地基增量（2026-09-08）

- `tiling/TileKey.h`——瓦片键 z/x/y + 四叉树（parent/children/ancestor）、isValid、
  哈希/排序（行优先）、"z/x/y" 字符串（header-only）。
- `tiling/WebMercatorTileScheme.{h,cpp}`——Web Mercator 正方形世界瓦片网格：
  **XYZ 顶层原点**（y=0 最北行，y 向南，与 OSM/NASA Terrain-RGB URL 模板一致）；
  瓦片西南角/尺寸（投影米）、经纬矩形、点（投影米/经纬）→ 键；
  纬度越出 ±85.05112878° 判世界外（不静默钳制）。
- `core/geodesy/QuadtreeGeometricError.{h,cpp}`——屏幕空间误差
  `sse = e / (2d·tan(fov/2)) · viewportHeight`（**地形 LOD 细化的判定公式**）+ shouldRefine。
- 单测 +3 套件（test_tile_key / test_tile_scheme / test_quadtree_geometric_error），
  覆盖四叉树关系、子瓦片边界共边、往返、世界外、SSE 单调性与退化输入。当前 14 套件全绿。

### 地形内容地基（2026-09-08，朝阶段 6 数据链路）

- `content/HeightmapCodec.{h,cpp}`——高度图像素编解码（纯函数，无图像 IO）：
  Terrain-RGB（Mapbox，h=-10000+(RGB)·0.1，量化 0.1m）+ Terrarium（Mapzen）；
  编码取整 + 越界钳制；整张行缓冲解码（行级 stride，首行=北，与 XYZ 顶行一致）。
  与 gis-md 的编解码语义对齐（其 HeightmapTerrainContentProvider 用 NASA Terrain-RGB）。
- `content/HeightmapSampler.{h,cpp}`——规则网格采样：nearest + 双线性（CLAMP_TO_EDGE），
  双线性精确重建线性场；供将来"查高/贴地/无缝边吸附"复用。
- 单测 +2 套件（test_heightmap_codec / test_heightmap_sampler）含解码→采样集成冒烟。
  当前 16 套件全绿。

### 瓦片→内容装配（2026-09-08）

- `content/HeightmapTile.{h,cpp}`——一块已解码高度图的内容封装（"瓦片→内容"装配产物）：
  解码网格 + 瓦片覆盖；**网格点贴瓦片边界**（DEM post 惯例，65×65 贴 64×64 瓦）；
  采样映射走 **mercator 米**（col 在 x 线性、row 在 y 线性，高纬不做经纬线性——
  gis-md T-P11 的教训直接进设计）；`sampleHeightAt(经纬)`（越瓦 nullopt）、
  像素↔地理互转、min/max 高度（将来包围体/剔除用，T-P13 教训）。
- 单测 +1 套件（test_heightmap_tile）：真实瓦键上的合成线性面，角/格点/行中点采样
  精确值、像素↔地理往返、瓦外 nullopt、1×N 退化、行序钉死（row0=瓦北边）。
  当前 17 套件全绿。

### 地形几何（2026-09-08，T-V5/T-V1 的机制地基）

- `content/TerrainTileMesh.{h,cpp}`——`TerrainMeshData`（ECEF 顶点/索引/平滑法线）+
  `TerrainTileMeshBuilder::build(HeightmapTile, ellipsoid, nodesPerEdge)`：
  节点贴瓦片边界（第 0/n 行列在瓦边），节点像素坐标 = i/n·(w-1) 双线性采样；
  三角形外向绕序（{a,c,b}/{b,c,d}）；平滑法线 = 邻接面法线平均。
  **几何与内容解耦**：节点数独立于高度图分辨率（T-E1 原则 host 侧体现）。
- 单测 +1 套件（test_terrain_tile_mesh）：拓扑/顶点计数/角点贴瓦角、平坦面
  法线外向且 ≈ 大地法线、山丘高度回读逐节点一致、**同级东西/南北相邻瓦共享边
  顶点 ECEF 逐点重合（±1e-6 m，T-V5 无缝契约的机制前提）**、退化网格拒绝。
  当前 18 套件全绿。
- 下一步：相机/拾取 + Provider 接口，再往后是"瓦片树→查高→网格"装配
  （把 HeightmapTile+TerrainTileMesh 挂到 SSE 选择结果上，逼近阶段 6）。

## 3. 合并点细节（阶段 6 执行时再展开）

地形服务并入清单（届时逐项对 gis-md `scaffold/src/earth_engine/` 核对、按许可证与来源注明 commit）：
DEM 高度图解码与重采样、Terrain-RGB/量网格的源语义、瓦片树与 LOD 选择（SSE）、
无缝（同级/跨级/remap 共享边高差 <1m）、GPU 位移模板 vs baked VBO 回退、页存储/页表。
**本仓不重写的理由**：地形数据服务（网络/解码/选择/无缝）在 gis-md 已是 16 万行引擎里被
186 单测 + 真机验收过的部分，重写只会重蹈其根因档案（terrain-gis-md-baseline.md 冻结档案）。

## 4. 变更纪律（北极星协议精简版，详见 terrain.md 更新协议）

动用户可见行为前先读对应北极星并说明「动 V 几」；判据编号只增不改；
【观感】像素判断归用户；结论必须带证据，不许"应该很小"。
