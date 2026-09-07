# A4 并入实施预案（gis-md 现成地形服务 → 选择性适配）

> 状态：**预案（2026-09-08）**。执行条件：用户确认并入形态（默认=选择性适配，
> 见 engine-targets §6）且 demo 观感初判不阻塞。文档回答"按什么顺序、把哪个 gis-md
> 件对到哪个本仓接口、怎么验收"，不臆造 gis-md 符号细节——源盘点以
> stage6-merge-checkpoint 第 2 步在 gis-md 实测后填行号。

## 0. 目标与红线

- 目标：让 gis-md 里被真机验证过的**地形数据/数值服务**进入本仓链路，替换
  demo 里的程序化/本地简化实现，而非把本仓从 0 的机制件重写。
- 红线：不整仓 vendor gis-md core；不拷 GLES/Metal 渲染后端；本仓自写件与并入件
  二选一或语义对拍，禁双实现长期并存（T-P6 教训）；每步单 commit 可回滚。

## 1. 候选并入件（按依赖从小到大的建议顺序，逐项盘点后拍板）

| 序 | gis-md 概念区（以 checkpoint 盘点为准） | 本仓接口落点 | 依赖风险 |
|---|---|---|---|
| B1 | 高度图 decode worker / 源语义（HeightmapTerrainContentProvider 的 worker 部分） | `ITerrainDataSource`（新实现替换 DemAssetSource 的自解码） | 中：网络/线程/缓存策略耦合，需拆 |
| B2 | 每瓦"已解码高度→查高"数值（DecodedHeightmapSampler 族语义） | `HeightmapSampler/HeightmapTile` 语义对照 + host 对拍测试 | 低：纯数值，先转 case |
| B3 | 真实 geometricError 元数据（源随瓦元数据） | `TerrainLodSelector` 误差代理替换 | 低-中：terrarium 无元数据→量化网格源才有 |
| B4 | 跨级无缝数值核（边高度 LUT/吸附） | content 无缝模块（host T-顶点修复原型） | 中：需先做 host 数值原型 |
| B5 | 调度/缓存/帧收敛（tileset→帧申报） | `TerrainFrameCache` + 帧收敛纪律 | 高：与渲染/后台线程耦合最重 |

## 2. 每个并入件的最小验收（host 先行）

1. 在 gis-md 侧定位实现与对应单测（record commit）；
2. 转写/对照其单测到本仓（数值容差同源）；
3. 适配成我们的接口实现 → `./test_native.sh` 全绿 + 移植 case 绿；
4. 固定机位基线不回退（test_fixed_station_baseline）；
5. Android 模拟器该路径出帧（dem 模式切换实现验证）。

## 3. 与本仓 Android demo 的接法

- B1 落点：`DemAssetSource` 之后新 `GisMdDemSource`（assets 或 HTTP 字节统一走
  ITileBytesSource）；切换点 = `demo_scene` 的 `useDem_` 分支扩展，不碰渲染。
- 渲染后端不并入：本仓 GLES 渲染器保持自写；gis-md 数值/服务只喂数据。

## 4. 文档/判据联动

- 每并入一件：更新 `docs/source-index.md`（来源 commit 标注）、
  `docs/northstar/terrain.md` 跟踪表（机制类附证据；观感类待用户截图拍板）。

## 5. 风险与显式不做的项

- 不做：glTF/模型/矢量等非地形件；gis-md 的 GPU 位移模板/页存储（渲染后端就绪再评估）；
- 风险：gis-md 件常依赖 nlohmann-json/glm/curl——本仓已有 vcpkg include 通道（host/NDK 均可），
  但 Android 端 curl 未链（assets/本地源先行，HTTP 留到接入层决策）；
- 依赖双方命名空间同为 earth_engine：并入时只适配接口签名，不做批量改名。

## 6. 下一动作（执行时第一步）

按 stage6-merge-checkpoint 第 1-2 步：在 gis-md 列 B1 相关文件与单测清单 + commit，
写入本预案附表后开做 B1 的 worker 拆分适配。

---

## 7. 执行记录（B1 已开工，2026-09-08 续）

> 会话按 NEXT-STEPS 决策点「A4 B1 开工」默认选择性形态执行；第一步 = 本预案 §6 的
> 源盘点 + host 先行数值件。附表见下。

### B1 源盘点（gis-md commit `bf25c639`，2026-09-07，HEAD）

| gis-md 文件（行锚点） | 角色 | 并入动作 |
|---|---|---|
| `providers/HeightmapTerrainProvider.{h,cpp}`（decodeTile 318–404；隐式哨兵注册 330–336/372–375；`kTerrainRgbNoDataFloorMeters` 65） | **decode worker 主体**：字节 → PNG/RGB → 高度 | 数值语义并入（本轮：哨兵注册 + min/max 排除 + 采样归一化）；请求/线程/缓存壳（81–277）留在本仓 `ITileBytesSource` + 调用方 |
| `providers/TerrainProvider.{h,cpp}`（`DecodedHeightmap` 27–124；`assignHeights` 29–67；采样 70–142） | 解码产物语义：16bit 全局格点量化（code0=nodata）、min/max 单一产地、borderInset/哨兵角归一化采样 | 本轮并入**哨兵语义**部分；量化/borderInset 未并入（见差值表） |
| `content/HeightmapTerrainContentProvider.{h,cpp}` | worker 的渲染端组装（decode → glTF 网格 + 裙墙/geomorph） | **不并入**（渲染形态与本仓自写几何不同构） |
| `providers/ImageTileBodyCheck.h` | 响应体魔数白名单（200+CDN XML 硬化） | 未并入（网络硬化项，接真实网络源时做） |

**gis-md 单测 → 本仓转写/对照**：

| gis-md 测试（行锚点） | 本仓落点 |
|---|---|
| `content/test_heightmap_terrain.cpp`：decode 契约 135–171；隐式哨兵契约组 585–625 | `tests/unit/content/test_decode_nodata_semantics.cpp`（新，35/35 含） |
| `tiling/test_decoded_heightmap_sampler.cpp`：哨兵环排除/全哨兵传播 390–417；boderInset 0.5 无缝逐位等 326–357 | 哨兵环/传播 case → 同上（顶点栅格形态）；borderInset/514 源 case 留 B2 |
| 同上 142–280（渲染网格一致采样/迟滞档位） | GPU 位移域，不转写 |

### 语义差值（本仓 host 链 vs gis-md decode worker）

| 语义 | gis-md | 本仓此前 | 本轮 |
|---|---|---|---|
| Terrain-RGB RGB(0,0,0) nodata 底值 | 隐式注册 -10000 哨兵（单一来源常量） | 无（-10000 当合法高度） | ✅ 注册 + min/max 排除 + 采样哨兵角归一化 |
| 瓦 min/max | 排除 no-data（唯一产地） | 全量扫描 | ✅ 有哨兵表时排除；无表逐位不变 |
| 采样遇哨兵角 | 仅有效角加权再归一化；全哨兵 → 哨兵上抛 | CLAMP 纯双线性 | ✅ 同上（哨兵表存在时） |
| 16bit 全局格点量化（CPU 常驻账） | code = round(h/0.125)，瓦间同高同码 → 无缝逐位等 | double 直通 | ⬜ 资源轴，GPU/页存储域再接（T-E1 记账时） |
| borderInset 0.5 / 514 重叠环 | 半像素内缩 + 邻瓦重叠回填 → 同级边逐位等 | 顶点栅格（inset 0） | ⬜ B2（cell-registered 源语义） |
| 响应体魔数检查 | 白名单 + 12B 下限 | 无 | ⬜ 网络硬化项 |

**落地纪律**：哨兵语义落在既有单实现上（`HeightmapCodec::kTerrainRgbNoDataFloorMeters`
常量、`TerrainGrid::noDataValues`、`HeightmapSampler`/`HeightmapTile` 可选哨兵参数、
两个 Terrain-RGB 源注册点、`TerrainFrameAssembler` 透传）——**不新建并行类型**
（T-P6 双实现禁令）；未注册哨兵时行为逐位不变（host 34 套件旧断言零改动）。
验收：`./test_native.sh` 35/35（新增 test_decode_nodata_semantics）+ 固定机位基线不回退。

**下一步（B1 余项 / B2 起点）**：见 terrain.md「host 机制证据」更新与 NEXT-STEPS。

### B1/B2 中间实测登记（2026-09-09，assets 配准形态）

用**完整 PNG unfilter**（此前简化解析会把 filter≠0 的行读成噪声，误导了一次误判）
解码内置 assets 后实测：

- **数据干净**：缙云山 terrarium z10–13 全部采样瓦 0% nodata（之前"边界瓦 46% 黑"是
  解析假象）；机位区瓦高程连续平滑（z13 中心 159–684m）。
- **配准形态 = 无重叠环的全局连续 post 网格**：相邻瓦边界像素**不重复、不共享列**
  （A255 vs B0 差 ≈ 瓦内相邻像素差，即 A255 与 B0 是相邻 post），每瓦 256 个 post、
  瓦界落在两瓦 post 之间。
- 推论（T-V5 机制边界）：本仓顶点栅格式贴边网格在瓦界处双侧各取边界外/内最近
  post → 同级共享边 mesh 差 ≈ |坡度|×像元间距。实测：**z13 均值 ~1.8m（med 1.0,
  p90 3–5）／z12 均值 ~3.9–4.7m（p90 8–10, max 24m）**。
- 关闭路径（后续轮）：B2 带重叠环源语义（瓦自带 1px 邻瓦回填 → 双侧取到同一
  瓦界值，gis-md 514 模型的 256/257 变体）或 B4 边 LUT/吸附（渲染/网格侧统一取边）。
  **宿主仪器已立**：`content/SeamAudit` + `test_seam_audit`（顶点栅格 ≈0 自证；
  无环连续栅格如实报差 → 回归门禁）。判据观感（瓦界肉眼不可见）仍待用户拍板。

### B2 能力切片入账（2026-09-09，borderInset 采样 + 环源 seam 闭合证明）

- 已并（单实现，默认零扰动）：`TerrainGrid::borderInset`（默认 0=顶点栅格，行为逐位
  不变）→ `HeightmapTile` 透传 → `TerrainTileMeshBuilder` 节点**落位与采样解耦**：
  落位按瓦界网格分数；采样按配准内缩 px = inset + f·((w−1)−2·inset)。
- 转写 gis-md 514 语义 → `tests/unit/content/test_ring_source_seam`：半像元内缩采样
  在共享边界给位级一致值；带环源（每瓦 C cell + 1px 回填，缓冲 C+2）帧级 SeamAudit
  ≈0（节点段数 8 与互质 5 均过）——即无环资产实测 1.8–4.7m 差距的**机制关闭路径**
  已证（与 test_seam_audit 无环 fixture 的 ~10m 报差成对照）。
- 接线余项（需数据/设备侧决策）：内置 assets 本身无环 → 要么解码侧读邻瓦边界
  回填环（AAssetManager 4 邻瓦 1px strip），要么换带环真实源（如 AWS Mapzen
  含环瓦片，须验证其 256 变体环宽）；模拟器出帧验证。判据观感仍待用户。
