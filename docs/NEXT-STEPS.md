# NEXT-STEPS —— 从哪里继续（2026-09-08 快照）

> 给下一位会话/用户的一页指引。仓库/远端同步（36+ 提交，HEAD=origin/main）。

## 现在能跑什么
- Host：`./test_native.sh` → 34/34 绿（从 0 自写引擎核心：坐标/投影/瓦片/SSE/选择/高度图/
  PNG/HTTP/网格/查高/缓存/拾取/视锥 + 五固定机位回归）。
- Android 模拟器观感 demo：`examples/android`（README 有步骤）。
  真 DEM 内置（terrarium，缙云山 z10–13）；五机位 `adb shell setprop debug.mapc.station 1..5`
  + 重启；手势拖动看图；截图 `adb exec-out screencap -p > x.png`。

## 看什么 / 怎么判（观感，像素归你）
- 正片：`docs/assets/station1..5.png`（= M-near/M-mid/M-graze/M-high/M-coarse）；
  ASCII 速览：`docs/assets/evidence.md`；指标：`docs/northstar/terrain.md`「固定机位截图集」。
- 判据口径：观感判据 T-V* 目前 ❌ 待拍板；机制证据已立（terrain.md「host 机制证据」）。

## 可选下一步（优先级建议）
1. **观感调优**（30–60 min/项）：构图/法线/光照/网格密度 → 改 `demo_scene.cpp` 重建重截图。
2. **A4 选择性并入 gis-md 现成地形服务**（按 `docs/a4-merge-plan.md` B1→B5）：
   先把 `HeightmapTerrainContentProvider` 的 decode worker 拆出适配 `ITerrainDataSource`
   （assets/HTTP 字节统一走 `ITileBytesSource`）；每步 host 单测 + 基线 + 模拟器出帧。
   形态确认点：选择性适配（默认） vs 整体 vendor。
3. **EGM96 undulation**：`core/geodesy/HeightDatumCorrector` 已备（网格双线性+单测）；
   下载 EGM96 网格接入 `HeightmapTile` 上游即启用（当前默认恒等、口径已记录）。
4. **真实网络源**：`CurlBytesSource` host 已通（回环实测）；设备侧接公网 terrarium 需
   处理模拟器网络/配额，assets 方式已是离线等价。

## 决策点清单（一句话即可触发）
- 「station N 的 X 要改」→ 我调 A3.2；
- 「A4 B1 开工」→ 按预案盘点+适配（默认选择性形态）；
- 「启用 EGM96」→ 我接网格改正；
- 「收尾」→ 我做最终验收汇总并把判据表/证据打最后状态。

## 重要纪律（动观感前读）
- 判据编号只增不改；【观感】像素判断归用户；【机制】自证要带证据；
- 改动前先读 `docs/northstar/terrain.md` 更新协议 + `docs/roadmap.md` 变更纪律；
- host 全绿是底线（`./test_native.sh`），模拟器基线 `test_fixed_station_baseline` 不回退。
