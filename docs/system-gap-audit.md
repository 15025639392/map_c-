# 系统缺口审计 —— 从"地形 host 闭环"到"地球引擎整机"

> 本文件回答：站在**地球引擎整机系统架构**角度，我们还缺哪些重要系统、为什么缺、
> 往哪个梯队推进、每套的最小验收是什么。坐标系 = [北极星模块表](northstar/engine-targets.md)
> §1 + [路线图阶段](roadmap.md) + 北极星「资源调度→上屏七段流水线」。
> 状态快照：2026-09-09，HEAD `15ae77a`。判据口径沿用仓库纪律
> （判据编号只增不改；【观感】像素判断归用户；结论带证据，不许"应该很小"）。

---

## 0. 现状一句话

已完成的是「**地形模块 host 机制全闭环 + Android 上屏出帧**」：44 gtest 全绿零告警、
干净重建/全新 clone 复核过；Android 模拟器 assets 版与 **NASA Terrain-RGB 514 网络源**
（z6–12）各五固定机位截图；A4「并入 gis-md 现成地形服务」host 执行面收官
（逐件清单见 [stage6-a4-host-closeout.md](stage6-a4-host-closeout.md)）。
从整机看它仍是"有坐标系、能铺地形、能简单上屏"的**裸引擎**——下面列的系统大多是
北极星模块表里有名、但本仓还只是地基或未开工的部分。

**已具备的系统层**（后面接缝都长在这些已有件上）：
数学/坐标（core）· 瓦片方案/LOD（tiling）· 地形内容与无缝机制（content，含环源/
哨兵/祖先回退/跨级吸附核/基准改正）· 相机视图/视锥/拾取（camera 地基）·
字节源/URL/网络（providers：CurlBytesSource、Java HttpURLConnection 桥）·
demo 场景与简易手势输入。

---

## 1. 主系统缺口（S1..S11）

| 编号 | 系统 | 为什么缺（缺失面） | 北极星归属 | 与现有代码的接缝 |
|---|---|---|---|---|
| **S1** | **渲染/图形系统** | 无 RTC 渲染器、无 DrawList/材质系统、无**页存储/位移模板/高度纹理**、无帧缓冲/tonemap | 观感判据 T-V1~T-V14、T-P1/T-P13 全卡在此 | **L2 已落**：`renderer/IRenderDevice`（host 口径）+ demo 侧 **Gles3RenderDevice** 实现并换用——
模拟器经设备渲染出帧（glErr=0，M-mid distinct 877 与改前一致）；观感判据 T-V* 可测化路径打通 |
| **S2** | **资源-调度-流水线** | 无并发加载/解码 worker、优先级队列、去重、磁盘/内存缓存、按瓦预算、**帧收敛申报**、换代状态机、失效取消。后果：M-coarse 首帧 323 瓦 ≈40s | 七段流水线判据（T-E1~T-E5、T-E4 帧收敛纪律、T-P7/T-P8） | 已有 ITerrainDataSource / ITileBytesSource / TerrainFrameCache / AncestorFallbackDataSource / 装饰器组合。**L1 已落**：TileCacheBytesSource + DiskTileCacheBytesSource（内存+磁盘缓存）→ 下一步并发/预算层 |
| **S3** | **场景/图层系统** | 无图层栈（顺序/透明度/生命周期/样式开关/内容路由）；现在是"固定 demo 场景"，接不了第二图层 | 影像/矢量/标注都挂在图层栈上 | 在 demoscene 之上加 Scene/Layer 抽象，复用现有帧集合形态 |
| **S4** | **影像系统** | 无影像 provider（XYZ/WMTS/…）、无纹理上传预算 | ★★ 影像（阶段 3/4） | 复用 ITileBytesSource + URL 模板。**L1 已落**：ImageryTileAvailability（缺瓦→祖先退化决议状态机，host）→ 下一步 provider + 真实源 |
| **S5** | **矢量与标注** | 零行：矢量瓦解码（MVT…）、样式/换肤、贴地不浮、字体图集标注、线宽/字号屏幕恒定 | ★ 矢量（阶段 5） | 拾取/查询走 TerrainPicking 扩展；高度贴地依赖现有查高服务 |
| **S6** | **相机导航系统**（成熟化） | demo 只有"拖动/双指"两条裸路径；缺穿地防护、病态俯仰兜底、LOD 感知控制、**多平台输入抽象** | ★ 相机/手势（阶段 2/7）：北极星相机判据（指下锚定/惯性收敛/不穿地） | CameraView/Frustum 已有。**L1 已落**：CameraMotion + TerrainGroundGuard + CameraNavController（运动/不穿地/联动控制器）→ 下一步手势输入层接线 |
| **S7** | **光照/大气/颜色系统** | 无太阳/天光模型、无大气散射/雾（空气透视）、无 tonemap/颜色管理（北极星：亮部超范围优雅压回） | ★ 光照/颜色（阶段 9） | demo 现简单半球漫反射着色器是出发点 |
| **S8** | **数据格式与内容注册** | 地形只吃 Terrain-RGB/Terrarium PNG；缺格式注册/内容类型（quantized-mesh/3D Tiles/glTF、JPEG/WebP、MVT/PMTiles）+ 每源元数据（availability 四叉树、geometricError、attribution） | B3 真实 geometricError；3D Tiles（阶段 8） | HeightmapCodec 族 + ITerrainDataSource 语义的注册点 |
| **S9** | **时间/动画系统** | 无 Clock/缓动层；相机动画、换代 geomorph/fade、数据进场动画无驱动 | 换代过渡（T-V12）机制族 | 跨级吸附核（content/SeamAudit snap）已备，动画系统给换代"何时吸/吸多少/多快" |
| **S10** | **天气系统** | 云/降水是世界坐标实体、被山挡、随太阳入夜变暗、可切换且过渡连续 | ○ 远期（weather） | 依赖 S1 渲染 + S7 光照 |
| **S11** | **交互/拾取系统化** | 拾取只有地形三角面；缺命中分层（地形/影像/矢量/标注）、屏幕空间查询、多指手势识别器抽象（tap/double/pinch/rotate…）、事件路由到图层 | 相机/手势判据族 + 矢量交互 | TerrainPicking 之上做 HitTester + 事件总线 |

> 注：你已知的"天气（S10）/手势（S6 输入层）"之外，S1/S2/S4/S5/S7/S8/S9/S11 与
> S3 图层栈、S6 导航本体是我判断的主要缺口。S4/S5 在北极星里权重 ★★/★，是
> "地形 + 底图 + 图层"能合屏的关键。

---

## 2. 横切系统（H1..H4）

| 编号 | 系统 | 缺什么 | 何时必须做 |
|---|---|---|---|
| H1 | 线程/内存/GPU 上传工程 | worker pool、上传队列、内存与页预算、防 churn 尖刺 | S1/S2 落地时 |
| H2 | 性能记账与离线工程化 | 帧时/内存探针、离线工具链 | 性能轴第一笔账（T-P*/T-E* 记账） | 已有 **首笔账**：IRenderDevice::DrawStats（draws/triangles/上传字节/驻留网格，host 用例 + 模拟器 ledger 日志） |
| H3 | 平台抽象与生命周期 | IO/网络/解码/日志平台口（现只有 AAssetManager + Java HTTP 两根硬线）、GL 上下文重建、能力探测与降级（T-P10 GPU on/off A/B）、诊断埋点 | 每次入平台（iOS/桌面/另一后端）前 |
| H4 | 无缝一致性治理 | 同级/跨级/换代的"边界统一采样"纪律横切 地形-影像-矢量 | 随 S4/S5 上屏即生效（现在只有地形 host 核） |

---

## 3. 推进梯队（建议顺序 + 理由 + 最小验收）

**梯队 1 —— 引擎"看得见+转得动"的开关**
- **S1 渲染抽象 + GPU 地形路径**：最小验收 = 后端决策（GLES3/Metal/抽象层）+ host 空实现 RenderDevice 用例 → 模拟器上把 CPU 网格换成 GPU 位移/高度纹理路径任一支，T-V* 从"无解"变"可测"。
- **S2 调度流水线（网络/解码/缓存/预算/换代）**：最小验收 = M-coarse 首帧 40s → 预算内收敛（并发拉取 + 磁盘缓存 + 按瓦预算），并立 T-E4 帧收敛申报的 host 状态机用例。它直接满足"每字节下载有账"。

**梯队 2 —— 同屏内容**
- **S4 影像系统**（与地形同屏、缺瓦退化链）；**S6 相机导航成熟化**（惯性/防穿地/flyTo + 手势识别器层）。

**梯队 3 —— 内容与表达**
- S5 矢量/标注（45° 贴地、换肤、标注）· S7 光照/大气/颜色 · S8 格式与内容注册 · S9 时间/动画 · S3 图层栈 + S11 交互系统化。

**梯队 4 / 远期**
- S10 天气；roadmap 阶段 7–10：绘制/测量/编辑、3D Tiles、环境系统、性能离线工程化。

**横切**：H3 平台抽象在每次入新平台前做；H1 随 S1/S2 做；H2 与 H4 随"第一条性能/第一条多图层"顺手立账。

每梯队/系统的推进继续遵守：**host 先行（先转 case 再改实现）→ 每步单 commit 全绿 →
固定机位基线不回退 → 判据/证据按 terrain.md 更新协议回填**。观感类验收（像素）归用户。

---

## 4. 怎么接：给下一位会话（本会话不传话，接缝都写在这）

> 目标：全新会话读完本文 + 下列文件即可开做，不需要本会话上下文。
> 通用纪律：host 先行（先转 case 再改实现）→ 单 commit 全绿 → 固定机位基线不回退 →
> 判据/证据按 `northstar/terrain.md` 更新协议回填。观感（像素）归用户。

### 4.0 开新会话先读（10 分钟内）
1. `README.md`（跑法）→ 2. 本文 §0/§3 → 3. `northstar/engine-targets.md` §5（能力×判据×缺口）
→ 4. `roadmap.md` 阶段表 → 5. `NEXT-STEPS.md`。代码摸底：`src/earth_engine/` 目录清单 +
`tests/unit/` 按模块套件名（source-index.md 有符号→文件索引）。跑基线：`./test_native.sh`（44/44）。

### 4.1 各系统接缝与第一步（S/H 编号同 §1/§2）

**S1 渲染/图形系统 —— 第一步先做"接口，不是渲染器"**
- 接缝：新增 `src/earth_engine/renderer/`（与 roadmap「RenderDevice 接口层，先 host 空实现 + 用例」对齐）；demo 的 GL 直写在 `examples/android/app/src/main/cpp/demo_scene.cpp`（VAO/VBO/shader/网格上传可抽象成第一对接口）。
- 顺序：①`RenderDevice`（createBuffer/uploadMesh/compileShader/draw）+ host 空实现 + 用例（fake 设备记录调用序列）→ ②GLES3 实现，demo 换用 → ③再谈 GPU 位移/高度纹理（页存储）。
- 决策点：后端形态（GLES3 先行？抽象到 Metal/Vulkan？）一句话即可定调。

**S2 资源-调度-流水线 —— 第一步解决"首帧 40s"**
- 接缝全在现有接口上：`content/ITerrainDataSource`、`providers/ITileBytesSource`、
  `content/TerrainFrameCache`（增量/淘汰雏形）、`content/AncestorFallbackDataSource`、
  装饰器组合（`tests/unit/content/test_decorator_composition.cpp` 演示了怎么包）。
- 顺序：①瓦片磁盘/内存缓存（包 ITileBytesSource，host fixture 先测命中/淘汰）→
  ②并发拉取（demo 端 Java HTTP 现为同步，先做"预取下一带"即可）→
  ③帧收敛申报状态机（T-E4 host 用例）→ ④换代过渡（接 S9 动画 + 现有跨级吸附核）。
- 验收句：M-coarse 首帧 <N 秒且每瓦去重/预算有计数（证据走日志/单测计数，不写"应该快"）。

**S4 影像系统 —— 地形同屏的最小切片**
- 接缝：新增 `src/earth_engine/imagery/`；瓦片字节复用 `ITileBytesSource`+URL 模板
  （`providers/TileUrlFormatter`）；合成需要 S1 的纹理上传（可先 CPU 合成到帧？不——
  先立 host 语义：缺瓦退化链 = 祖先真实影像而非空洞，写状态机单测）。
- 顺序：①影像瓦元数据（覆盖/层级）与退化链 host 单测 → ②provider 接入真实源 →
  ③与地形同屏（S1 后）。
- 决策点：首个影像源（真实 XYZ/WMTS 端点 or fixture）？

**S5 矢量与标注 —— 等 S1/S6 的半成品不上屏**
- 接缝：新增 `src/earth_engine/vector/`、`style/`；贴地高度查询用现有查高链
  （HeightmapTile/装饰器）；命中分层在 `content/TerrainPicking` 之上扩展 HitTester。
- 顺序：①矢量瓦解码 + 样式求值（host 单测：样式→几何预算）→ ②贴地（45° 判据机
  制侧）→ ③标注（字体图集，依赖 S1）。

**S6 相机导航成熟化 —— 在 demo 手势壳上叠系统层**
- 接缝：`camera/CameraView`（基/射线/脚印）+ Frustum 已有；输入壳在
  `examples/android/.../CameraTouchController.java`；新增 `src/earth_engine/camera/`
  内 `CameraController`（导航）+ `interaction/` 手势识别器抽象。
- 顺序：①host 化导航状态机（位置/朝向插值、穿地探测用 ray-ellipsoid + 地形查高）
  → ②惯性/阻尼 host 用例（确定性、不收敛即 bug）→ ③Android 手势识别器层替换裸
  拖动/双指。
- 判据钩子：北极星相机判据（指下锚定、惯性收敛、不穿地、病态俯仰兜底）。

**S7 光照/大气/颜色 —— demo 着色器起步**
- 接缝：`examples/android/.../demo_scene.cpp` 的 kVs/kFs（半球漫反射）；新增
  `src/earth_engine/environment/`（大气散射参数）与 `renderer` 的材质输入。
- 顺序：①光照参数与 tonemap 的 host 数值用例（颜色不超界/压回单调）→ ②上屏 A/B。

**S8 数据格式与内容注册 —— 先立"格式表"，不急着解码**
- 接缝：`content/HeightmapCodec` 族、`providers/*`、`ITerrainDataSource` 语义；
  新增 `content/ContentTypeRegistry`（format → decode → metadata 工厂）。
- 顺序：①注册表 + 元数据（availability/geometricError）host 用例 →
  ②quantized-mesh/3D Tiles 读入（B3 的 geometricError 因此落地）→ ③JPEG/WebP 解码
  扩展（stb 已支持，扩注册即用）。

**S9 时间/动画系统 —— 小但先立 Clock**
- 接缝：新增 `src/earth_engine/animation/`（Clock/Tween）；换代 geomorph 的量与
  时机喂给现有 snap 核（`content/SeamAudit::snapChildBoundariesToCoarse` 是"吸多少"
  的数值源）。
- 顺序：①Clock/Tween host 用例 → ②相机 flyTo 用（配合 S6）→ ③换代 fade 用。

**S3 图层栈 / S11 交互系统化 / S10 天气**
- S3：先定义 Scene/Layer 接口 + 一个"地形层"实现迁移 demoscene 帧集合，host 测图层
  顺序/可见性。
- S11：HitTester（地形→矢量→标注命中）host 用例先行；手势识别器抽象归 S6 族。
- S10：依赖 S1+S7，最后做；先只立"天气实体=世界坐标+被地形遮挡"的 host 语义测试。

**H1–H4 横切**：H3 平台口在 S1 接 demo 时顺带抽（IO/网络/日志三根线先接口化）；
H1 随 S2 并发落地；H2 性能账在第一笔性能改动时立；H4 无缝纪律在 S4/S5 上屏时对齐
地形口径（同源边界采样）。

### 4.2 决策点清单（一句话即可触发，参照 NEXT-STEPS 风格）
- 「渲染后端定 GLES3/Metal/抽象」→ 开 S1 第一步；
- 「S2 开工」→ 先做瓦片缓存 + M-coarse 首帧目标；
- 「影像接真实源 X」→ 开 S4；
- 「导航成熟化」→ 开 S6（flyTo/惯性/防穿地任选其一先做）；
- 「天气/手势之外的系统清单」→ 见本文 §1/§3。

---

## 5. 关联文档

- 北极星模块表 / 能力×判据×缺口：`docs/northstar/engine-targets.md`
- 路线图阶段与验收口径：`docs/roadmap.md`
- 地形判据状态与机制证据：`docs/northstar/terrain.md`
- A4 并入执行与停点：`docs/a4-merge-plan.md`、`docs/stage6-a4-host-closeout.md`
- 交接指引：`docs/NEXT-STEPS.md`
