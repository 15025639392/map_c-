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
