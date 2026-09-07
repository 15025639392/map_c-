# L1/L2 运行收官记录（2026-09-09，goal 执行 27 轮）

> 本文件记录本次自主协调运行的产出（ultimate-goal §3 波次计划执行到哪、
> 证据在哪、剩余开放项），供下一位会话/用户接续。判据口径不变：
> 观感归用户；机制带证据；判据编号只增不改。

## 1. 本运行做了什么（波次 → 件 → 证据）

| 波次 | 内容 | 提交 | host 证据 |
|---|---|---|---|
| Wave-1 | S2 字节缓存（TileCacheBytesSource）、S4 退化决议（ImageryTileAvailability）、S6 运动模型（CameraMotion） | `907e699` 起 | tile_cache / imagery_degrade / camera_motion |
| Wave-2 | S2 磁盘缓存（DiskTileCacheBytesSource）、S6 贴地（TerrainGroundGuard）+ 控制器（CameraNavController） | `10635bf` `22c30b3` | 对应套件 |
| Wave-3 | L2 渲染抽象：IRenderDevice（host 口径）→ GLES3 实现 → demo 换用 → DrawList 每瓦 → 纹理通道 | `bfa6a8e`…`1472475` | render_device_interface；设备截图 drawlist/tex_overlay |
| Wave-4 | H2 首笔账（DrawStats）、S4 数据腿（PngToRgba8） | `2f192b8` `f2938e9` | 设备 render ledger；png_rgba_texture |
| Wave-6 | S4 装配链（ImageryTileSource） | `10eca77` | imagery_tile_source |
| Wave-7/9 | **真实影像**：高德卫星 style=6（JPEG 256）五机位同屏 | `ecaa90f` `846922e` | amap_satellite_st{1..5}.png（distinct 7.7k–16k） |
| Wave-10/11 | RGBA/alpha（keepAlpha）→ 高德注记 style=8 叠加层 | `3a6e755` `6cf793b` | amap_labels_Mmid.png；decodeKeepAlpha 用例 |
| Wave-12 | 图层开关 debug.mapc.img/lbl | `4418807` | hypsometric_no_img_Mmid.png（img=0 回归 877） |
| R26 | 复核：native 从零 54/54、全新 clone 54/54 | `ff6e25a` | — |

**host 54/54 恒绿零告警**（自 Wave-1 起每波单 commit、基线不回退）；android-arm64 core 随波交叉编译通过；模拟器设备验证覆盖渲染/纹理/真实影像/注记/图层开关。

## 2. 状态对照（ultimate-goal §3）

- **L1**：资源调度第一步（内存+磁盘瓦片缓存 + demo 共享缓存）✅；影像语义先行（退化决议 + 瓦源装配 + 数据腿）✅；相机运动/贴地/控制器/手势速率映射 ✅。
- **L2**：渲染抽象接口（IRenderDevice 全语义 + host 口径）✅ → GLES3 实现 ✅ → demo 换用 + DrawList 每瓦 + 纹理通道 ✅ → **真实影像内容三层同屏（DEM 高度 + 卫星 + 注记）** ✅；DrawStats 性能账 ✅。
- **未做（开放项）**：GPU 高度纹理/位移模板与页存储（T-E1 深化；host 核/对拍/frag 上屏已✅，
  顶点位移几何受 GLES 模拟器驱动 vertex 纹理采样限制，真机/位移属性通道待续）、
  影像真实源换任意源、矢量图层/标注原生化、图层栈 S3 化、光照大气颜色、天气、
  观感判据（T-V*）用户拍板（截图已备多套）。

## 3. 给接续者

- 读 `docs/ultimate-goal.md`（§3 波次/§4 规则）、`docs/system-gap-audit.md`（S/H 状态行已实时更新）、`docs/NEXT-STEPS.md`、`docs/WORK-PARTITION.md`（若再并行）。
- demo 运行/图层开关/机位：`README.md` Android 段。
- 截图证据：`docs/assets/`（station/NASA/amap/hypsometric/gles/drawlist/tex/realtex 各批）。
