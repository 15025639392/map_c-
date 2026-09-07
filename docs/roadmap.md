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

### LOD 瓦片选择（2026-09-08，阶段 4 选择机制）

- `tiling/TerrainLodSelector.{h,cpp}`——SSE 驱动的四叉树 LOD 选择：从根瓦宽优遍历，
  兴趣矩形外剪枝；停止 = 达 maxLevel 或 瓦 sse ≤ 阈值；未停细化四子。
  两个关键语义（测试钉住）：
  - **相机到瓦区域最近点的距离**（不是瓦中心——大瓦中心在地球对侧会把距离算成
    半个行星，根瓦被误停）；
  - 几何误差代理 = geometricErrorScale × 瓦 mercator 尺寸（瓦内地形位移 ≪ 瓦宽；
    真实值将来由源元数据误差表替换）。
- 单测 +1 套件（test_terrain_lod_selector）：远相机（300km,16px→z3~4 少量瓦）、
  近相机（1.5km,4px→z13）、中心点覆盖、maxLevel 无 runaway、世界外空集、确定性。
  当前 19 套件全绿。

### 地形帧总装（2026-09-08，阶段 6 装配前身）

- `content/TerrainDataSource.h`——`TerrainGrid`（解码栅格）+ `ITerrainDataSource` 接口
  （按瓦返回高度栅格；nullopt = 瓦不可用）。**将来并入 gis-md 现成地形服务 =
  给它适配一个实现**，装配与几何代码不动。
- `content/TerrainFrameAssembler.{h,cpp}`——把 `TerrainLodResult` 喂给数据源，
  每瓦：解码 → HeightmapTile（mercator 米查高）→ TerrainTileMesh（ECEF 网格），
  产出 `Frame{key, mesh, min/maxHeight}`；缺失瓦跳过（祖先回退属调度阶段）。
- 单测 +1 套件（test_terrain_frame_assembler）：真实选择结果装配、手工 3 邻瓦
  （A+东/南）装配后**跨帧同层共享边 ECEF 逐点重合**、flaky 源缺失瓦跳过、
  确定性。当前 20 套件全绿。
- **host"地形帧"主链路已闭环**：LOD 选择 → 解码 → 查高 → 无缝网格。

### 相机/拾取地基（2026-09-08，固定验收机位的输入侧）

- `camera/CameraView.{h,cpp}`——地表相机视图：位姿基（forward/right/up 施密特正交化，
  垂直下看 roll 退化有兜底）、NDC→射线（`rayThroughNdc`）、视锥四角射线打椭球 → **地表
  脚印矩形**（`groundFootprintRadians`，任一角看太空返回 nullopt）——将来视锥驱动瓦片
  选择与固定机位（M-near/M-mid…）的输入。
- `core/math/RayTriangle.h`——Möller–Trumbore 双面求交（header-only）。
- `content/TerrainPicking.{h,cpp}`——射线对装配后地形帧的拾取（全三角形最近命中，
  面法线外向化）。
- 单测 +3 套件（test_ray_triangle / test_camera_view / test_terrain_picking）：
  三角形命中/顶点/边/退化/无背面剔除；相机基正交、脚印含正下点且半宽量级合理、
  朝天 nullopt；拾取命中 k00 西北角顶点（=网格顶点 0）、朝天 miss、跨 4 帧拾最近帧。
  当前 23 套件全绿。
### 相机→地形帧端到端 + 跨层级边界（2026-09-08）

- `camera/TerrainCameraPipeline.{h,cpp}`——一步管线：相机地表脚印 → LOD 选择 →
  数据源解码 → ECEF 地形帧（`assembleTerrainFrameForCamera`）。host 侧
  "一帧 = 相机 → 地形瓦片"成形。
- 单测 +2 套件（test_terrain_camera_pipeline / test_terrain_cross_level）：
  - 端到端：俯瞰 12km 相机出帧、正下方点被覆盖、屏幕中心射线拾取到地形（海拔在
    函数值域）、朝天相机空帧；
  - **跨层级边界**（T-V5 另一半）：粗瓦 A(z) 与东邻 B(z) 的北/南半区子瓦（z+1）
    共享物理边界——偶数行**共享网格点 ECEF 逐点重合**（±1e-6 m）；子瓦奇数行是
    T 顶点（不同细分的自然结果，注明归 stage-6 remap 域处理）；父瓦中心 =
    四子瓦共同角点。
  当前 25 套件全绿。
### Provider 语义 + 本地 fixture（2026-09-08，阶段 3 的 provider 前身）

- `providers/TileUrlFormatter.{h,cpp}`——{z}/{x}/{y} URL 模板替换（单一格式化点，
  {tms_y}/{s} 将来在此扩展）；未知占位原样保留便于排查。
- `providers/ITileBytesSource.h`——瓦片字节源抽象（将来 HTTP/curl 只需实现它）。
- `providers/TerrainRgbTileSource.{h,cpp}`——Terrain-RGB 高度 Provider：
  字节源 → HeightmapCodec 解码 → TerrainGrid（ITerrainDataSource 实现；
  PNG 解码/网络属后续接入层，接口不变）。
- 单测 +2 套件（test_tile_url_formatter / test_terrain_rgb_source）：模板替换/重复
  占位/未知占位保留；fixture 字节解码逐点对照 fn（±0.06m 量化容差）、URL 传参、
  畸形/缺失拒绝、**RGB fixture → camera 管线 → 出帧 → 拾取**端到端。当前 27 套件全绿。

### 真实瓦片字节形态：PNG（2026-09-08）

- 依赖接入：CMake 复用 `GIS_MD_VCPKG_INSTALLED/include`（stb 等，env.sh 设定）；
  仅库内使用（测试不被第三方头污染 gtest 解析）。
- `providers/StbPngDecoder.{h,cpp}`——stb_image 解码 PNG → RGB 行（首行=顶=北，
  统一 3 通道，STBI_ONLY_PNG）。
- `providers/TerrainRgbPngTileSource.{h,cpp}`——Terrain-RGB **PNG** 高度源：
  PNG 字节 → 解码 → Terrain-RGB → TerrainGrid（真实源 NASA Terrain-RGB 的字节形态）。
- 单测 +1 套件（test_png_terrain_source）：测试内置**自写最小 PNG 编码器**
  （stored-deflate + CRC32/ADLER，不用第三方测试头）生成 fixture →
  库内 stb 解码互验（两条独立实现互相校验）；逐点对照 fn ±0.06m；垃圾字节/
  尺寸不符拒绝。当前 28 套件全绿。
- 下一步：curl HTTP 字节源（ITileBytesSource 的网路实现），或视锥精确剪枝/渲染抽象。

## 3. 合并点细节（阶段 6 执行时再展开）

地形服务并入清单（届时逐项对 gis-md `scaffold/src/earth_engine/` 核对、按许可证与来源注明 commit）：
DEM 高度图解码与重采样、Terrain-RGB/量网格的源语义、瓦片树与 LOD 选择（SSE）、
无缝（同级/跨级/remap 共享边高差 <1m）、GPU 位移模板 vs baked VBO 回退、页存储/页表。
**本仓不重写的理由**：地形数据服务（网络/解码/选择/无缝）在 gis-md 已是 16 万行引擎里被
186 单测 + 真机验收过的部分，重写只会重蹈其根因档案（terrain-gis-md-baseline.md 冻结档案）。

## 4. 变更纪律（北极星协议精简版，详见 terrain.md 更新协议）

动用户可见行为前先读对应北极星并说明「动 V 几」；判据编号只增不改；
【观感】像素判断归用户；结论必须带证据，不许"应该很小"。
