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
| **S1** | **渲染/图形系统** | 无 RTC 渲染器、无 DrawList/材质系统、无**页存储/位移模板/高度纹理**、无帧缓冲/tonemap | 观感判据 T-V1~T-V14、T-P1/T-P13 全卡在此 | **L2 已落**：`renderer/IRenderDevice`（host 口径）+ demo 侧 **Gles3RenderDevice** 实现并换用 +
高度纹理数值核（HeightTextureCodec）+ 模板核（DisplacementTemplate，host 对拍 <0.25m）——
**设备位移实验（2026-09-09）**：GLES3 模拟器驱动下 vertex shader 纹理采样不可靠（位移几何异常），
已回退；替代路径 = 位移向量属性通道（每瓦 CPU 计算位移向量、以 attribute4 上传，shader
`p = aPos + aDisp`）——**已落地并通过模拟器像素对照**：基准椭球模板 + 位移属性上屏几何与
baked 逐像素一致（disp1_station2.png vs baked0_station2.png：Δpx 75/2592000≈0.003%、
单像素 Δ≤5/765、glErr=0x0），证明 attribute 通道位移机制成立；共享模板驱动的 vertex
纹理采样位移（无逐瓦位移属性）仍为模拟器驱动受限面，登记为真机/后续轮路线
模拟器经设备渲染出帧（glErr=0）；**DrawList-lite** 每瓦独立上传/绘制（draws=42/live=42）；
**纹理通道已通**：Texture2D + UV sampler（合成棋盘叠影，distinct 877→937，upBytes 含
UV/纹理账，截图 tex_overlay_Mmid.png）——影像同屏/GPU 高度纹理通道就绪；观感判据可测化打通 |
| **S2** | **资源-调度-流水线** | 无并发加载/解码 worker、优先级队列、去重、磁盘/内存缓存、按瓦预算、**帧收敛申报**、换代状态机、失效取消。后果：M-coarse 首帧 323 瓦 ≈40s | 七段流水线判据（T-E1~T-E5、T-E4 帧收敛纪律、T-P7/T-P8） | 已有 ITerrainDataSource / ITileBytesSource / TerrainFrameCache / AncestorFallbackDataSource / 装饰器组合。**L1 已落**：TileCacheBytesSource + DiskTileCacheBytesSource（内存+磁盘缓存）。**S2 第一刀已落（demo 接线）**：瓦片字节缓存上提为**跨相机重建持久成员**（ring/amap/label 三路×512 共享底层字节源；NASA 重建只补新瓦）——设备证据：R1 miss 42 → R2 miss 70(hit2) → R3 miss 72(hit20/20)，网络请求/重建从全量降为个位数；guard 查高瓦同走持久 ring 缓存；像素基线 Δpx=0。**S2 二刀已落（demo 接线）**：逐瓦 GL 纹理句柄持久映射（img/lbl/hgt，TileKey→handle：命中免
  解码/新建、新瓦才解码上传、FIFO 上限淘汰释放；修复每重建句柄泄漏）——设备证据 R1 new42 →
  R2 hit2 new23（重建只补新瓦），像素基线 Δpx=0，重建 ~0.1s 级。**S2 三刀已落（demo 接线）**：磁盘瓦片缓存层（DiskTileCacheBytesSource；Java filesDir/tilecache →
  ring/amap/label 子目录；链 = 内存→磁盘→网络）——设备证据：冷启 ring write=42（全量网络 ~8s）；
  warm 重启 ring hit=42 write=0 pass=0（零网络，~3s 总出帧）。剩余：并发 worker/优先级/预算/帧收敛申报 |
| **S3** | **场景/图层系统** | 无图层栈（顺序/透明度/生命周期/样式开关/内容路由）；现在是"固定 demo 场景"，接不了第二图层 | 影像/矢量/标注都挂在图层栈上 | **L3 已落（地基）**：`scene/LayerStack`（host：顺序/开关/透明度/生命周期/差分账，5 用例；59/59）——demo 图层开关已以栈为事实源（dem→imagery→label→debug，nav0 像素不变 Δpx=0）。剩余：宿主渲染句柄挂栈（矢量/影像层内容路由化） |
| **S4** | **影像系统** | 无影像 provider（XYZ/WMTS/…）、无纹理上传预算 | ★★ 影像（阶段 3/4） | 复用 ITileBytesSource + URL 模板。**L1/L2 已落**：退化决议 + ImageryTileSource（装配链，host）+ PngToRgba8 + **真实影像源已接：高德卫星**（JPEG 256 XYZ：webst01.is.autonavi.com/appmaptile?style=6）——
解码器放开 PNG-only 支持 JPEG；demo 每瓦高德卫星纹理（mercator uv 北=顶）+ shader 双模式
（影像 albedo/高度着色）；M-mid 42/42 瓦出图 distinct 16195（截图 amap_satellite_Mmid.png）；
S4→渲染全链真实内容出图打通；观感/朝向归用户 |
| **S5** | **矢量与标注** | 矢量瓦解码（MVT…）、样式/换肤、贴地不浮、字体图集标注、线宽/字号屏幕恒定 | ★ 矢量（阶段 5） | **L3 已落（最小切片，host）**：`vector/VectorGrounding`（GeoJSON 点子集解码 + 贴地投影（地表椭球高+浮空 offset → ECEF；无数据不落点）+ 样式键映射；5 用例；60/60）——demo 上屏/标图与 MVT 解码属阶段 5 |
| **S6** | **相机导航系统**（成熟化） | demo 只有"拖动/双指"两条裸路径；缺穿地防护、病态俯仰兜底、LOD 感知控制、**多平台输入抽象** | ★ 相机/手势（阶段 2/7）：北极星相机判据（指下锚定/惯性收敛/不穿地） | CameraView/Frustum 已有。**L1 已落**：CameraMotion + TerrainGroundGuard + CameraNavController（运动/不穿地/联动控制器）。**L3 slice A 已落**：引擎层相机制 **MapCameraSystem**（`camera/MapCameraSystem.{h,cpp}`，turret 语义：正下中心经纬+高度+heading/pitch；手势增量→GestureToMotion 速率→CameraMotion 惯性/阻尼→TerrainGroundGuard 不穿地→flyTo 目标先贴地抬升；host 用例 14/14，57/57）——demo 手势层接线：nav=1 时 Java 只送屏幕增量、每 GL 帧引擎步进并回灌位姿（`debug.mapc.nav`/`debug.mapc.flyto`/`debug.mapc.panprobe`），nav=0 直连基线**像素级不变**（nav0_station2 vs baked0_station2 Δpx=0）。**L3 slice B 已落**：中心平移轴（pan）——双指质心增量经 ENU 地面投影→米/秒（与旋转/缩放同阻尼/收敛/上限、同帧组合），中心经纬球面小步积分，贴地防护随中心走（移入高地抬升到净空之上）；demo 双指拖动=平移（捏合缩放保留、单指仍旋转/俯仰）；设备证据 logcat pose lon 106.440→106.404（~4km 西移）+ 截图 nav1_pan_*.png（Δpx≈44.6%）。
  **L3 slice C 已落**：LOD 感知灵敏度包络（视距≤3000m 降速至 0.35×、≥8000m 全速，线性过渡；旋转/缩放/
  平移同乘；可配参）host 用例×2（16/16）——station2 15km 观感不变、station1 3km 生效，观感交用户。
  剩余：多平台输入抽象/tap·double/HitTester |
| **S7** | **光照/大气/颜色系统** | 无太阳/天光模型、无大气散射/雾（空气透视）、无 tonemap/颜色管理（北极星：亮部超范围优雅压回） | ★ 光照/颜色（阶段 9） | demo 现简单半球漫反射着色器是出发点 |
| **S8** | **数据格式与内容注册** | 地形只吃 Terrain-RGB/Terrarium PNG；缺格式注册/内容类型（quantized-mesh/3D Tiles/glTF、JPEG/WebP、MVT/PMTiles）+ 每源元数据（availability 四叉树、geometricError、attribution） | B3 真实 geometricError；3D Tiles（阶段 8） | HeightmapCodec 族 + ITerrainDataSource 语义的注册点 |
| **S9** | **时间/动画系统** | 无 Clock/缓动层；相机动画、换代 geomorph/fade、数据进场动画无驱动 | 换代过渡（T-V12）机制族 | 跨级吸附核（content/SeamAudit snap）已备，动画系统给换代"何时吸/吸多少/多快" |
| **S10** | **天气系统** | 云/降水是世界坐标实体、被山挡、随太阳入夜变暗、可切换且过渡连续 | ○ 远期（weather） | 依赖 S1 渲染 + S7 光照 |
| **S11** | **交互/拾取系统化** | 拾取只有地形三角面；缺命中分层（地形/影像/矢量/标注）、屏幕空间查询、tap/double 手势、事件路由到图层 | 相机/手势判据族 + 矢量交互 | **L3 已落（输入层）**：`interaction/PointerGestureRecognizer`（平台无关触摸流→单指旋转/双指平移+捏合；7 用例；demo nav=1 已走该层）。剩余：HitTester 命中分层 + tap/double + 事件路由 |

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

**S6 相机导航成熟化 —— 引擎层相机制已落（slice A 三轴 + slice B 中心平移），剩余输入抽象/LOD**
- 接缝：`camera/` 组件族（CameraView/Frustum/CameraMotion/GestureToMotion/
  TerrainGroundGuard/CameraNavController）之上新增**引擎相机制 MapCameraSystem**
  （turret 语义，`debug.mapc.nav=1` demo 已接线；host 套件 test_map_camera_system 14 用例；
  设备证据：nav1_station2_preset/glide、nav1_flyto_*、nav1_guardfloor_*、nav1_pan_* 截图 + logcat）。
- 已落：惯性滑行（抬手衰减收敛）、贴地防护（flyTo 目标 10m 被抬到 ground+5=237m）、
  flyTo 平滑插值（目标高度先贴地抬升）、按住制动、**中心平移**（双指质心→ENU 地面米/秒，
  与旋转/缩放同帧组合；贴地随中心走；设备 logcat lon 106.440→106.404）；nav=0 基线像素不变。
- 剩余：①tap/double 手势 + HitTester 命中分层（S11）→ ②flyTo/pan 进默认观感入口
  （现为 prop/探针触发）→ ④pan 时贴地回落策略（移入低地是否降高，现保持绝对椭球高）。
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
