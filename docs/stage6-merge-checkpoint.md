# 阶段 6 合并检查点（并入 gis-md 现成地形服务）

> 目的：把目标条款③「到地形阶段时把 gis-md 现成地形服务并入复用而非重写」转成
> **可执行清单**。阶段 6 开工时照此走，避免"重写数据服务"或"整段盲拷"。
> 本仓 host 侧已预留接缝：见下"接口映射"。

## 0. 原则

- 不重写：DEM 解码/重采样/瓦片树/无缝的**机制**已在 gis-md 被 186 单测 + 真机验收
  （terrain-gis-md-baseline.md 冻结档案记录了全部根因），重写只会重蹈。
- 只适配：把 gis-md 能力对到本仓 `ITerrainDataSource / HeightmapTile / TerrainFrameCache`
  这一组 host 接口上；本仓的几何/选择/缓存代码不动。
- 逐件并入、逐件带来源：每次并入注明 gis-md commit 与文件路径，可回滚。

## 1. 接口映射（本仓 host 件 ← gis-md 概念）

| 本仓（已有，测试背书） | gis-md 概念（scaffold/src/earth_engine/…） | 并入动作 |
|---|---|---|
| `providers/TerrainRgbPngTileSource` + `ITerrainDataSource` | `content/HeightmapTerrainContentProvider` / DEM 解码链 | 把其 worker 解码/缓存策略适配成 ITerrainDataSource 实现 |
| `content/HeightmapSampler` / `HeightmapTile`（mercator 米查高） | `DecodedHeightmapSampler` / HeightSource 查高 | 语义对齐（本仓 row0=北 已按 T-P11 教训） |
| `tiling/TerrainLodSelector`（SSE） | Tileset / TileSelection / SSE 细化 | 真实 geometricError 元数据（量化网格表）替换代理系数 |
| `content/TerrainFrameCache`（增量） | 调度/缓存/帧收敛 | 缺瓦请求/淘汰接真实网络源；帧收敛申报纪律 |
| `TerrainFrameAssembler::Frame.min/maxHeight` | 瓦包围体 min/max（T-P13 教训） | 并入后确认真机包围体不再恒 0 |
| —（待建） | 跨级无缝：`TerrainEdgeHeightLut`/边吸附/remap（T-V5 整链） | host 侧先把"T 顶点补齐/吸附"数值原型化，再并 GPU 路径 |
| —（待建） | `TerrainPageStore` 页存储 | 渲染抽象落地后再并入 |

## 2. 并入步骤

1. **源盘点**：逐目录列 gis-md `content/`、`tiling/`、`providers/` 与地形相关的 .h/.cpp，
   记录 commit；识别依赖（nlohmann-json、glm、curl、stb——本仓已通过
   `GIS_MD_VCPKG_INSTALLED/include` 就绪）。
2. **host 先行件**：把 gis-md 单测里与坐标/解码/采样/无缝数值相关的 case 转写/对照
   到本仓测试（AGENTS 对齐纪律：先转 case 再改实现）。
3. **按件并入**：解码链 → 查高 → 选择元数据 → 帧缓存接线；每件一步一绿。
4. **命名空间/布局核对**：双方同为 `earth_engine`、目录同构（core/geodesy、tiling、
   content、providers），摩擦点应只在 include 路径与类型名差异。
5. **判据回填**：每并入一件，按 `docs/northstar/terrain.md` 更新协议回填跟踪表
   （机制类给 commit/单测；观感类标注 🔒 待用户上屏）。
6. **AI_INDEX 式索引**：本仓新增 `docs/source-index.md` 维护符号→文件→行号（可选守卫）。

## 3. 验收口径

- host：并入后 `./test_native.sh` 全绿 + 移植 case 绿；固定机位基线不回退。
- 观感：真机 M-near/M-mid/M-graze/M-high/M-coarse 五机位截图，判据表逐条改判
  （像素判断归用户）。

## 4. 不改动/风险

- 不把 gis-md 的渲染后端（GLES/Metal）一并拷入本仓——渲染抽象另行决策；
- 本仓现有自写件若与并入实现重复（如采样），以"并入实现为准 + 本仓件退役"或
  "语义对拍后二选一"，禁止双实现长期并存（T-P6 教训）；
- 每次并入必须在本仓可编译、可回滚（单 commit 一步）。
