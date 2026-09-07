# map_cplus — 地球引擎（C++17，从 0 重建）

在 `/Users/yan/Desktop/work/map_cplus` 用 C++17 **从 0 重建**移动优先 3D 地球引擎，
**地形为最高优先模块**。规格与判据来自 `/Users/yan/Desktop/work/gis-md`（只读参考源）：
北极星文档回答「做到什么程度算好」，路线图回答「按什么顺序从 0 建」；
到**地形（阶段 6）**时并入 gis-md **现成的地形服务**而非重写数据链路。
开发成熟后推送到 `git@github.com:15025639293/map_c.git`。

## 文档

| 文档 | 作用 |
|---|---|
| [docs/northstar/engine-targets.md](docs/northstar/engine-targets.md) | 地球引擎北极星目标摘要（从 gis-md 提取） |
| [docs/northstar/terrain.md](docs/northstar/terrain.md) | **地形模块判据活文档**（本仓状态跟踪） |
| [docs/northstar/terrain-gis-md-baseline.md](docs/northstar/terrain-gis-md-baseline.md) | 地形判据契约全文（gis-md 快照，勿手改） |
| [docs/roadmap.md](docs/roadmap.md) | 从 0 重建分阶段计划 + 合并点规则 |

## 快速上手（host native，macOS）

```bash
./test_native.sh              # 全量单测（内部会 source env.sh）
./test_native.sh test_ellipsoid  # 跑单个套件
```

`env.sh` 自动找本机 cmake/ninja（Android SDK 自带）与 googletest 源码缓存
（优先复用 gis-md 已 FetchContent 的副本，离线可用），无需手动装依赖。

## 目录

```
map_cplus/
├── CMakeLists.txt / CMakePresets.json   # native / native-tests 两个 preset
├── env.sh / test_native.sh              # 构建环境 + 单测入口
├── docs/
│   ├── roadmap.md                       # 分阶段计划（从 gis-md 路线图改编）
│   └── northstar/                       # 北极星活文档（地形优先）
└── src/earth_engine/                    # 核心库 earth_engine_core
    ├── core/                            # math（Vec/Mat/Ray/Plane…）+ geodesy（椭球/投影/ENU/SSE…）
    ├── tiling/                          # 瓦片键/四叉树/WebMercatorScheme/LOD 选择
    ├── content/                         # 高度图编解码/查高/ECEF 网格/地形帧/缓存/拾取
    ├── camera/                          # CameraView/视锥/地形帧管线
    └── providers/                       # URL 模板/字节源/HTTP/PNG/Terrain-RGB
└── tests/unit/                          # gtest：core/geodesy/tiling/content/camera/providers
```

## 当前状态（2026-09-08）

**32 个 gtest 套件全绿零告警**。host 地形主链路闭环：相机（脚印/射线/视锥）→
LOD 选择（SSE+剪枝）→ 数据源（HTTP/PNG/Terrain-RGB）→ 每瓦查高 → 无缝 ECEF 网格 →
拾取，外加帧缓存增量与固定机位基线。判据/能力映射见
`docs/northstar/engine-targets.md` §5；地形判据状态逐条见 `docs/northstar/terrain.md`
（全部 ❌——观感类需 GPU 平台/真机，机制类证据已登记）。
