# NEXT-STEPS —— 从哪里继续（2026-09-09 快照）

> 给下一位会话/用户的一页指引。仓库/远端同步（HEAD=origin/main）。
> **A4 host 机制面已收官**（2026-09-09）：逐件入账与停点清单见
> `docs/stage6-a4-host-closeout.md`——继续 A4 前先读它。

## 现在能跑什么
- Host：`./test_native.sh` → 53/53 绿。地形链路 44 + L1/L2：缓存×2/退化链/瓦源装配
（ImageryTileSource：决议→取瓦→纹理数据）/相机族/渲染抽象（纹理+UV 设备验证）
/PngToRgba8——host 先行。
- Android 模拟器观感 demo：`examples/android`（README 有步骤）。
  真 DEM 内置（terrarium，缙云山 z10–13）；五机位 `adb shell setprop debug.mapc.station 1..5`
  + 重启；手势拖动看图；截图 `adb exec-out screencap -p > x.png`。

## A4 已开工（B1/B2 切片，2026-09-08 续）
- 源盘点（gis-md `bf25c639` 文件/单测/语义差值）与拆分设计 → `docs/a4-merge-plan.md` §7。
- B1 首块已并入：Terrain-RGB nodata 哨兵语义（RGB(0,0,0) 隐式注册 -10000 → min/max 排除 +
  采样哨兵角归一化），落在既有单实现（无哨兵时行为逐位不变）；转写 case =
  `test_decode_nodata_semantics`。
- B2 切片已并入：`TerrainGrid::borderInset` + mesh 落位/采样解耦（默认 0 逐位不变）；
  带环源（cell-registered + 1px 回填）同级边闭合证明 = `test_ring_source_seam`。
- 余项（按 a4-merge-plan §7 差值表）：16bit 全局格点量化（GPU/页存储域）、
  ImageTileBodyCheck（接真实网络源时）、网络/线程/缓存策略接 ITileBytesSource 的调度化。
- **实测登记（2026-09-09）**：内置 assets 为无重叠环连续栅格 → 同级共享边 mesh 差
  ≈|坡度|×像元（z13 均值 1.8m / z12 3.9–4.7m，max 24m）；**B2 切片已证带环源
  SeamAudit≈0**（test_ring_source_seam，转写 gis-md 514 语义）→ assets 接线项 =
  解码侧回填环或换带环源（详见 a4-merge-plan §7 与 terrain.md 机制证据）。

## 看什么 / 怎么判（观感，像素归你）
- 正片：`docs/assets/station1..5.png`（= M-near/M-mid/M-graze/M-high/M-coarse）；
  ASCII 速览：`docs/assets/evidence.md`；指标：`docs/northstar/terrain.md`「固定机位截图集」。
- 判据口径：观感判据 T-V* 目前 ❌ 待拍板；机制证据已立（terrain.md「host 机制证据」）。

## 可选下一步（优先级建议）
1. **A4 并入续（B1 余项 → B2）**：按 `docs/a4-merge-plan.md` §7 差值表推进——量化语义
   （资源轴记账）→ borderInset/514 重叠环采样（cell-registered 源语义 + 无缝逐位对拍，
   接 B4 边吸附地基）；每步 host 单测 + 基线。
2. **观感调优**（30–60 min/项）：构图/法线/光照/网格密度 → 改 `demo_scene.cpp` 重建重截图。
3. **EGM96 undulation（机制已备，剩数据文件）**：`HeightDatumCorrector`（网格双线性+单测）
   与接入装饰器 `HeightDatumCorrectingDataSource`（host 40 套件含测试）都已落地；
   下载 EGM96 网格（NGA/GeographicLib；本沙箱外网不可达未成）→ 装载文件 → 喂给
   GridHeightDatumCorrector + 装饰器即启用；设备出帧后定默认开关。
4. **真实网络源（host 已接 NASA 514）**：用户指定端点 `https://mapoverlay.xinzhi.space/
   3dterrain/nasa/tiles/{z}/{x}/{y}.png`（Mapbox Terrain-RGB，514×514 带 1px 环，覆盖
   z6–12）已 host 全链接通并烟测（MAPC_LIVE_NET=1 跑 test_nasa_ring_source）；
   设备侧接线（模拟器网络/配额 + demo 数据源切换）与 assets 替代决策为余项。

## 决策点清单（一句话即可触发）
- 「station N 的 X 要改」→ 我调 A3.2；
- 「A4 B1 开工」→ 按预案盘点+适配（默认选择性形态）；
- 「启用 EGM96」→ 我接网格改正；
- 「收尾」→ 我做最终验收汇总并把判据表/证据打最后状态。

## 重要纪律（动观感前读）
- 判据编号只增不改；【观感】像素判断归用户；【机制】自证要带证据；
- 改动前先读 `docs/northstar/terrain.md` 更新协议 + `docs/roadmap.md` 变更纪律；
- host 全绿是底线（`./test_native.sh`），模拟器基线 `test_fixed_station_baseline` 不回退。
