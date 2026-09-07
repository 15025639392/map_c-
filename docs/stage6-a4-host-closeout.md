# Stage 6 / A4 host 机制面收官记录（2026-09-09）

> 本文件回答：A4「并入 gis-md 现成地形服务」在 **host 可证面**上做到哪一步、
> 每件证据在哪、哪些余项停在哪类决策上。判据观感（T-V*）仍全部 ❌ 待用户上屏
> 拍板（本文件不改判据表）；机制类证据见 `northstar/terrain.md`「host 机制证据」。
> 仓库纪律：每步单 commit 可回滚、host 全绿、固定机位基线不回退——全程遵守。

## 0. 基线

- 开工 HEAD：`a59b45c`（34 套件，final-audit-2026-09-08 之后）。
- 收官 HEAD：`f0d83cf`，**host 43/43 全绿零告警**；`./test_native.sh` 与全新 clone
  口径均复核过（详见 README「当前状态」）；固定机位基线（frames=2 z=[8..9]
  triangles=256 height=[250.6..346.1]）全程未回退。

## 1. 逐件入账（commit → 件 → host 证据）

| A4 行 / 切片 | 落地（commit） | host 证据（套件） | 状态 |
|---|---|---|---|
| 源盘点 + 拆分设计（gis-md `bf25c639`） | `7837a11`（docs a4 §7） | —（契约表） | ✅ |
| B1：Terrain-RGB nodata 哨兵语义（隐式注册 -10000 / min-max 排除 / 采样哨兵角归一化；无哨兵零扰动） | `1570c80` | test_decode_nodata_semantics | ✅ |
| SeamAudit 仪器 + **assets 配准实测**（无环连续栅格：z13 均值 1.8m / z12 3.9–4.7m） | `8fe6a14` + `9540e4c` | test_seam_audit | ✅ |
| B2 切片：borderInset（cell-registered + 1px 回填环）采样 → 同级边闭合（SeamAudit ≈0，转写 gis-md 514 语义） | `c915e94` | test_ring_source_seam | ✅（能力；资产无环 → 接线余项） |
| B5 前身：AncestorFallbackDataSource（缺瓦上溯 + 重采样，父数据子几何） | `1a2f2ec` | test_ancestor_fallback | ✅（host 半） |
| 网络硬化：ImageTileBodyCheck（PNG/JPEG/WebP 白名单） | `9ded18e` | test_image_tile_body_check | ✅ |
| EGM96 接入路径：HeightDatumCorrectingDataSource（恒等零拷贝默认） | `5e3e9d8` | test_height_datum_correcting_source | ✅（数据文件余项） |
| B4 取证：auditCrossLevelTVertexGap（跨级 T-顶点量级；平坦地形也有椭球弦垂 ~0.1m） | `6755b6f` | test_cross_level_tvertex | ✅ |
| B4 数值原型：snapChildBoundariesToCoarse（吸附粗弦 → 审计 <1e-6m，拓扑不变） | `8c240ce` | test_cross_level_tvertex | ✅（CPU 拼帧内核） |
| 装饰器组合端到端（相机 × 回退 × EGM96） | `11aaa81` | test_decorator_composition | ✅ |
| 真实资产字节回归（Terrarium host 解码 + 边差锁档） | `d2d8167` | test_terrarium_asset_decode | ✅ |
| 真实 DEM 相机帧（M-near 型正下 3km/z13，模拟器 demo host 替身） | `f0d83cf` | test_terrarium_asset_decode | ✅ |
| 干净重建/全新 clone 复核（41/41 口径）+ env.sh NDK 注记 | `a2b6ae7` | — | ✅ |

## 2. 差值表余项（host 不可证 / 域外，登记停点）

| 余项 | 所属域 | 停点（仓库文档依据） |
|---|---|---|
| 16bit 全局格点量化语义（资源轴） | GPU/页存储 | 现 CPU 链栅格瞬态无常驻内存压力；GPU 高度纹理/页存储域接入（T-E1 记账时） |
| assets 解码侧回填环接线（关闭 1.8–4.7m 同级边差） | 数据/设备 | 源注册模型（cell vs corner）离线不可判定 + 需模拟器出帧验证；能力与门禁已备（B2 + SeamAudit） |
| EGM96 真实网格文件 | 数据/网络 | 本沙箱外网不可达（NGA/GeographicLib 未成）；接入路径与配方已备 |
| B3 真实 geometricError 元数据 | 源元数据 | terrarium 无元数据；量化网格源（QM 元数据表）接入时替换代理系数 |
| B4 remap/边 LUT 的渲染域落地 | 渲染/调度 | CPU 吸附核已证；GPU 位移/边 LUT 待渲染后端决策 |
| B5 全调度（帧收敛申报/预算） | 渲染/调度 | 按需渲染契约（roadmap 帧收敛纪律）落地时 |
| 观感判据 T-V1/V6/V12… 拍板 | 用户（像素） | 模拟器截图已备（docs/assets/station1..5.png），待用户判 |

## 3. 判据与文档状态

- `northstar/terrain.md`：判据表保持 ❌/🔒 未虚标；「host 机制证据」节逐件登记
  （本文件 §1 对应套件）；更新协议未破（判据编号未改、观感判据未替用户判）。
- `a4-merge-plan.md` §7：执行记录（盘点/实测/入账）齐。
- `source-index.md` / README / engine-targets / NEXT-STEPS：符号与计数口径一致
  （43/43）。

## 4. 结论

A4「并入 gis-md 现成地形服务」的 **host 可证执行面到此收官**：12 个切片全部单
commit 落地、43 套件全绿、证据与判据口径一致。剩余项按上表停靠在渲染/调度/
数据/设备/用户域——各自有明确决策点与已备好的接缝（装饰器/门禁/配方），
不属本 host 执行面的未完工作。
