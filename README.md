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
| [docs/system-gap-audit.md](docs/system-gap-audit.md) | **引擎整机系统缺口审计**（S1..S11 主系统 + 横切 + 梯队） |
| [docs/NEXT-STEPS.md](docs/NEXT-STEPS.md) | 交接/下一步指引（怎么跑、怎么判、决策点） |

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

## 当前状态（2026-09-09 复核）

**52 个 gtest 套件全绿零告警**（2026-09-09：地形链路 44 + L1/L2 系列：缓存/影像退化链/相机
运动族/渲染抽象与纹理管线/PngToRgba8 影像瓦→纹理数据腿）。
host 地形主链路闭环：相机（脚印/射线/视锥）→
LOD 选择（SSE+剪枝）→ 数据源（HTTP/PNG/Terrain-RGB）→ 每瓦查高 → 无缝 ECEF 网格 →
拾取，外加帧缓存增量、同级/跨级共享边审计（`SeamAudit`）、**带重叠环源采样（borderInset, B2）**
与固定机位基线。
**A4 并入已开工（B1+B2 切片）**：Terrain-RGB nodata 哨兵语义（隐式注册 -10000 /
min-max 排除 / 采样哨兵角归一化）已落入既有解码链（单实现）；带环源同级边闭合
已证（SeamAudit≈0，转写 gis-md 514 语义）；内置 DEM 为无环源（同级边差 ~1.8–4.7m，
接线项见 [a4-merge-plan.md](docs/a4-merge-plan.md) §7）。
判据/能力映射见 `docs/northstar/engine-targets.md` §5；地形判据状态逐条见
`docs/northstar/terrain.md`（全部 ❌——观感类需 GPU 平台/真机，机制类证据已登记）。


## Android 模拟器 demo（观感验证）

```bash
cd examples/android
export JAVA_HOME=/Users/yan/Library/Java/JavaVirtualMachines/jdk-17.0.20.1+1/Contents/Home  # 需完整 JDK(jlink)
export GIS_MD_VCPKG_INSTALLED=/Users/yan/Desktop/work/gis-md/scaffold/third_party/vcpkg/installed/arm64-osx
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.mapcplus.terrain/.MainActivity
```

- 数据源（`debug.mapc.src`，重启生效）：
  - **`nasa`（默认，2026-09-09 切到用户指定网络源）**：`https://mapoverlay.xinzhi.space/
    3dterrain/nasa/tiles/{z}/{x}/{y}.png` —— Mapbox Terrain-RGB **514×514（512 cell +
    1px 裙边回填环）**，覆盖 z6–12；native 经 JNI → Java HttpURLConnection（系统 TLS）
    拉取 → 引擎环模式解码。真机实测：M-near(z12) 2 瓦、M-mid(z12) 42 瓦出帧，
    截图 `docs/assets/nasa_station{1,2}_*.png`（distinct 350/877）。
  - `asset`：app 内置真实 terrarium DEM（`app/src/main/assets/dem`，缙云山 z10–13）
    离线兜底（`adb shell setprop debug.mapc.src asset`）。
- 机位：`adb shell setprop debug.mapc.station 1..5`（M-near/M-mid/M-graze/M-high/M-coarse）后重启 app。
- 手势：拖动=俯仰/航向，双指=高度；截图 `adb exec-out screencap -p > shot.png`。
- 截图集与机读指标见 `docs/northstar/terrain.md`「固定机位截图集」（`docs/assets/station1..5.png`）。
- ASCII 缩略证据包：`docs/assets/evidence.md`（文本环境快速预览五机位）。

