# WORK-PARTITION —— 多窗口并行任务分区契约（2026-09-09）

> 目的：让**多个独立会话/窗口**在同一仓库上并行推进不同类型的任务而不互相踩。
> 每个新窗口 = 全新会话（无对话记忆）——本文件 + 各任务简报就是它的全部上下文。
> 请先读：`README.md`（跑法）→ 本文 → 对应任务简报 → `docs/system-gap-audit.md` §4.1
> （系统怎么接的总指引）。

## 0. 铁律（所有窗口必须遵守）

1. **只碰自己名下的文件**（见任务简报「范围」）；共享文件一律不改，需要新增共享件时
   在本窗口任务简报里声明，集成时统一处理。
2. **不做 `git checkout/switch/branch`**（共享工作树，切分支会踢别人）；
   完成一个可交付点用 `git add <自己名下文件> && git commit -m "…"`（文件互不重叠，
   git 能处理交错提交）；**不 push origin**（集成者统一推）。
3. **构建/测试只限自己的目标**：`cmake --build build/native-tests --target <自己的测试名>`
   或 `ctest -R <自己的测试名>`。**不要并发跑全量 `./test_native.sh`**（共享 build 目录
   会竞态）；全量回归由集成会话统一跑。
4. 每窗口只新增**一个**测试文件（在名下目录），套件名 = 任务代号；CMake 测试是
   `file(GLOB_RECURSE tests/unit/*.cpp)` 自动注册，新增文件无需改 CMake。
   引擎源文件需登记进 `src/earth_engine/CMakeLists.txt` 的对应 MAP_*_SOURCES——
   只允许**在列表末尾追加自己新增的文件行**，且本窗口提交前先 `git pull --rebase`？——
   不：共享工作树直接提交即可；若 CMakeLists 已被别的窗口改过，先 `git add CMakeLists`
   一起提交（冲突极小，追加式）。
5. 判据/证据纪律照旧：结论带证据；【观感】像素判断归用户；判据编号只增不改。
6. 设备/模拟器**只有一台**：需要出帧/装机的任务串行，host 优先的任务可并行。

## 1. 当前并行任务（三窗口，互不重叠）

| 窗口 | 任务（代号） | 系统 | 目录范围 | 里程碑 |
|---|---|---|---|---|
| 窗口 1 | T1 tile-cache | S2 资源调度第一步 | `src/earth_engine/providers/`（新增 TileCache*）+ `tests/unit/providers/test_tile_cache.cpp` | host 绿 + 计数证据 |
| 窗口 2 | T2 imagery-degrade | S4 影像第一步（状态机） | `src/earth_engine/imagery/`（新目录）+ `tests/unit/imagery/test_imagery_degrade.cpp` | host 绿 + 状态机语义 |
| 窗口 3 | T3 camera-inertia | S6 导航 host 第一步 | `src/earth_engine/camera/`（新增 CameraTween/惯性模型）+ `tests/unit/camera/test_camera_motion.cpp` | host 绿 + 确定性用例 |

## 2. 任务简报

---

### T1 tile-cache —— 瓦片字节缓存（包 ITileBytesSource）

**背景**：真实 NASA 514 源 M-coarse 首帧 323 瓦 ≈40s，因每瓦都走网络且无缓存。
北极星：每字节下载有账（S2/七段流水线）。
**上下文**：读 `providers/ITileBytesSource.h`、`providers/CurlBytesSource.h`、
`providers/TileUrlFormatter.h`、`tests/unit/providers/test_http_bytes_source.cpp`（回环 HTTP
起停模式）、`tests/unit/content/test_terrain_frame_cache.cpp`（缓存语义/计数风格）。
**交付**：`providers/TileCacheBytesSource.{h,cpp}`——装饰 `ITileBytesSource`：
- 键 = URL 字符串；容量上限（构造参数，默认如 64 瓦 / 总字节上限二选一，简单先行）；
- 命中直接返回副本；未命中转内层并缓存；报告 `hit/miss/evict/bytes` 计数；
- 语义与线程安全声明（demo 现在同步调用，先不做并发，注明即可）。
登记进 `src/earth_engine/CMakeLists.txt` MAP_PROVIDERS_SOURCES（追加两行）。
**测试** `tests/unit/providers/test_tile_cache.cpp`：命中/未命中/淘汰（容量 1）/计数/
内层失败（nullopt 不入缓存）透传。
**验收**：`cmake --build build/native-tests --target test_tile_cache && ctest -R test_tile_cache`
绿；提交消息带计数结论。
**不改**：providers 现有文件、host 其他套件、demo 设备代码。

---

### T2 imagery-degrade —— 影像缺瓦退化链状态机（S4 语义先行）

**背景**：影像北极星「影像缺失时屏幕永不空白：退化 = 更低分辨率**真实影像**而非空洞」。
S4 第一步只做 host 状态机，不上屏（纹理/合成等 S1 渲染系统后再接）。
**上下文**：读 `tiling/TileKey.h`（parent/children）、`tiling/WebMercatorTileScheme.h`、
`docs/northstar/engine-targets.md` §1（影像一句话）。目录按 gis-md 同构精神新建
`src/earth_engine/imagery/`。
**交付**：`imagery/ImageryTileAvailability.{h,cpp}`：
- 输入：源 availability（z 范围或每瓦 bool 谓词）+ 请求键；
- 输出：请求键的**供应决议**——自身可用 / 祖先可用（给出实际键）/ 全不可用（空）——
  状态机含"数据到达后请求重评"的输入接口（可先纯函数 + 轻量状态）；
- 纯 host、无 IO。
登记 `src/earth_engine/CMakeLists.txt`（新建 MAP_IMAGERY_SOURCES 列表，追加到文件）。
**测试** `tests/unit/imagery/test_imagery_degrade.cpp`：自身/祖先多级/全缺/到达重评/
不越层到"假数据"（空 = 明确空，不许给 0 值冒充）。
**验收**：自己目标构建+ctest 绿；提交消息注明状态机决策表。

---

### T3 camera-motion —— 相机运动模型 host 化（惯性/flyTo 地基，S6）

**背景**：相机北极星：惯性收敛不跑飞、不穿地、组合手势各轴独立；demo 只有裸拖动/双指。
S6 第一步在 host 把"运动模型"做出来（不接 GL/输入），为将来手势识别器提供语义。
**上下文**：读 `camera/CameraView.h`、`core/geodesy/RayEllipsoid.h`、`core/math/MathUtils.h`、
`tests/unit/camera/test_camera_view.cpp`。
**交付**：`camera/CameraMotion.{h,cpp}`：
- 两模型任选或都做（简报写清）：①速度/阻尼惯性（每帧衰减，速度阈值停机→"收敛"谓词）；
  ②flyTo 插值（位置/朝向南—北平滑，参数化时长/缓动）；
- 确定性（纯函数步进，输入初值+dt 序列 → 输出状态序列），数值边界（穿地不做 clamp，
  只保证不 NaN/不跑飞发散：步长有界、速度有上限）；
登记 `src/earth_engine/CMakeLists.txt` MAP_CAMERA_SOURCES（追加两行）。
**测试** `tests/unit/camera/test_camera_motion.cpp`：收敛停机、无发散（大步长 dt 界）、
flyTo 端点到达 + 单调、零速度不动、确定性（同种子同序列）。
**验收**：自己目标构建+ctest 绿；提交消息带数值结论（收敛帧数、界）。

---

## 3. 集成（主会话/指定窗口做一次）

1. `git pull`（共享树无分支即跳过）→ `./test_native.sh` 全量绿（44 + 3 新套件 = 47）；
2. 计数口径文档（README / engine-targets / NEXT-STEPS / source-index / 对应判据表证据行）统一；
3. 更新 `docs/system-gap-audit.md` §4.1 对应系统行的状态 → 单 commit 推送 origin；
4. 模拟器验证项（若某任务涉及）排队到集成后。

## 4. 开启窗口（给用户的粘贴行）

窗口 1：`读 docs/WORK-PARTITION.md 的 T1 任务简报并执行，遵守铁律；完成后本窗口只提交不推送`
窗口 2：`读 docs/WORK-PARTITION.md 的 T2 任务简报并执行，遵守铁律；完成后本窗口只提交不推送`
窗口 3：`读 docs/WORK-PARTITION.md 的 T3 任务简报并执行，遵守铁律；完成后本窗口只提交不推送`
