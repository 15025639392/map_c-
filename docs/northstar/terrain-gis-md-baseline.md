# 地形模块北极星 — 四轴判据（gis-md 基线快照）

> **本文件 = 从 gis-md 提取的完整判据表权威副本（快照）。**
>
> - 来源：`/Users/yan/Desktop/work/gis-md/docs/northstar/terrain.md`
> - gis-md commit：`1b7e2907`（2026-09-07，分支 terrain-seam-remap-ablation）
> - 提取日期：2026-09-08；提取目的：作为 map_cplus 从 0 重建地形模块时的**判据契约**与**当前值/证据基线**。
> - 本快照内「当前值 / 证据」列 = gis-md 既有引擎（16 万行 C++）在 2026-09-07 的状态，
>   **不是本仓状态**。本仓是全新从 0 重建（见 docs/roadmap.md），状态逐条按
>   docs/northstar/terrain.md 的跟踪节回填。
> - 更新：只在从 gis-md 重新提取时整体替换（保留本头）；日常改判请写进
>   docs/northstar/terrain.md 的「本仓状态」节，不许改本文件。

---

# 地形模块北极星 — 四轴判据

**这份文档回答「做到什么程度算好、现在到哪了、花了多少、钱有没有花冤」。**
不回答「代码在哪」(那是 `AI_INDEX.md`),也不回答「当时怎么修的」(那是 `docs/issues/*`)。
本文是活的,随每次专项收官更新。

> **2026-09-05 按四轴形态重写**(体验 / 性能 / 资源占用 / 资源调度效率)。
> 编号全部沿用,**只增不改**;被重写措辞的判据在证据列注明「措辞重写」,原措辞见 git 历史。
> 旧版的 T-V10 交接与 E′ 记账整段保留在「冻结档案」节。

---

## 北极星一句话

> 任意机位、任意加载阶段,地形都读得出真实地貌(山脊 / 沟谷 / 坡面明暗可辨),
> 瓦界与换代对用户不可见;为此付出的**每一字节下载、每一次烘焙、每一个三角形,
> 都有对应的像素在屏上消费**。

前半句是体验轴,后半句是调度效率轴。性能轴与资源轴是它们的预算约束。

---

## 怎么用

- **编号是跨会话稳定锚点**:`T-V*` 体验、`T-P*` 性能与资源(沿用历史,债与判据混编,
  见 D 节索引)、`T-E*` 调度效率(2026-09-05 新立)。只增不改。
- **状态**:✅ 达成(有证据) ⚠️ 有缺口 ❌ 未做 🔒 待你拍板。
- **每条判据必填四列**:度量方法 / 当前值 / 目标值 / 代价。**度量方法先于目标值**——
  目标定不下来写 🔒,但没有度量方法的判据不许入表(它会在不同会话被不同口径量,
  T-V5 踩过:判据说"无缝",量的是"不透天")。
- **类型决定谁判**:【机制】我自证(计数 / 帧时 / 测试红绿 / 日志);【观感】像素归你,
  我只钉机位 + 给截图。
- **代价列 = 资源轴的子账**:一条体验判据新增的开销,同时在资源轴有一行。
  没量化写「未量化」,**不许填"应该很小"**。

### 三条元规则(替换旧版「不做定期 review」)

1. **判据不许写解法。** 写"坡面明暗按源数据分辨率起伏可辨",不写"高度纹理烘到 257²"。
   解法写进证据列;换了解法,判据不动。(T-V1 原措辞「三角形不横跨数十像素」就是
   把解法当判据,它把投资引向几何密度,而真正的瓶颈在纹理分辨率——见 T-E1。)
2. **删除 / 替换一个渲染表示 = 本文逐条改判**,与 AI_INDEX 行号同级,是收官必做项;
   「已判死」表同时复审(死因带着当时的架构上下文)。
3. **横切根因双向互引。**(2026-09-08 又一例:帧收敛申报的第六个洞——待 finalize 积压无票导致
   真机冷启动饿死,记在 `pipeline.md` K′ 节,本文 T-E4 有一行指过去。)
   UV/投影、缓存、帧收敛申报不属于单一模块;一处记债,
   相关模块必须有一行指过去。(I-P5 与 T-P11 是同一根因,各记一半、隔了三周才连上。)

### 固定验收机位(本文所有「当前值」的采集条件)

| 机位 | 参数(`debug.ee.cam*` 启动属性) | 用途 |
|---|---|---|
| **M-near** | 106.44E 29.70N,camH 3000 m,pitch −60°,heading 20° | 近景质感(缙云山) |
| **M-mid** | 同点,camH 15000 m,pitch −45° | 瓦界 / 换代 / remap 密集区 |
| **M-graze** | 同点,camH 8000 m,pitch −10° | 掠视透天、远景兜底 |
| **M-high** | 同点,camH 60000 m,pitch −30° | 加载期体面、多档共存 |
| **M-coarse** | 同点,camH 250 km,pitch −20° | fade 压平区、全球兜底 |

隔离条件:`debug.ee.noamap=1`(纯地形,矢量不上屏)。稳态 = `FrameGate idle` 后。
模拟器(swiftshader)只出机制读数与结构性观感,帧时以真机为准。

---

## A. 体验轴(T-V)

| # | 判据 | 类型 | 状态 | 度量方法 | 当前值 | 目标值 | 代价 | 证据 / 差距 |
|---|---|---|---|---|---|---|---|---|
| **T-V1** | **近景地貌可辨**:M-near 机位,山脊 / 沟谷 / 坡面明暗按源数据的分辨率起伏,不出现网格级"软包"(措辞重写 2026-09-05;原措辞「三角形不横跨数十像素」是解法) | 观感 | ❌ | 截图 + 法线场有效分辨率(纹素/瓦)+ 几何节点间距(m) | 法线场 **65²/瓦**;几何 coarse 65² ≈ 150 m/节点;M-near 截图整屏软包 | 🔒 归你(我押法线场 ≥257²、几何 dense 档在 SSE 驱动下可达) | 见 T-E1 / C1 / T-P9′ | 根因不是几何密度,是**信息利用率**(T-E1):514² 源烘成 65² 高度+法线纹理,着色也只有 65²。多档密度(`f69c39c5`)已回退——它只解几何一半,且在纹理 65² 下看不出收益。正解 = 几何靠档位、细节靠纹理,两者解耦 |
| **T-V2** | 坡面不出现逐三角形亮度台阶(刻面) | 观感 | ⚠️ | 截图 + 法线场命中率探针 | 法线场命中 93.47% 屏幕像素 | 100% 且观感无刻面 | 一张法线贴图 + 槽 23;未量化 | 法线与几何解耦已落地;收益被 T-V1 的 65² 法线场闸住——法线场分辨率太低,"逐三角面刻面"变成了"逐纹素软包" |
| **T-V3** | 地形有可读的明暗起伏(relief 不被光照压平) | 观感 | ✅ 代码落地,观感待拍板 | 真机 A/B 截图 | 线性 Lambert `clamp(NdotL*0.9+0.3)`(`1a939be70`) | 归你 | 0 | 单一治理点 `TerrainSurfaceLightGLSL.h`;剩 MSL sunTint 参数化(B4) |
| **T-V5** | **稳态瓦界连续**:FrameGate idle 后,任意相邻瓦(同级 / 跨级 / remap 瓦)共享边高差 < 1 m,无透天、无台阶(措辞加严 2026-09-05:原判据只量"不透天") | 机制 | ⚠️ | ①`appRate` 透天率 ②同级边探针 `logSameLevelEdgeDiag`(**须扩到 remap 瓦**,现只比对自有 DEM 瓦) ③M-mid 稳态截图 | 透天 = 0(旧证据);**M-mid 45 s 稳态有一条台阶缝**(`plainClip=1` 恒不归零) | 0 处台阶 | — | 台阶缝最像成因:上采样祖先(无自有 DEM)→ remap 拒绝 → 画祖先 baked VBO+discard,与邻居 GPU 位移面高度来源不一致(结构性回退 #2 撞 remap)。**先加 remap 拒绝原因计数器坐实**(0.5 h) |
| **T-V6** | 加载窗口不露"海平面凹坑" | 观感 | ❌ **改判**(2026-09-05) | M-graze / M-high 20 s 截图 | fill 代理**已关**:`enableTerrainFillProxy=false`(`706fb156` 关,boot `fillProxy=0`);M-graze 远处平椭球 + 天际线 | 归你 | 关闭后 0;开启时见 T-P3 | 旧版写「真机 demo 已开启」已不成立。是有意退役还是顺手关的,迁移提交没记——🔒 归你 |
| **T-V7** | 源未覆盖区回落平滑椭球,而非粗叶子 + 巨型裙墙 | 机制+观感 | ⚠️ | 单测 + M-coarse 截图 | 机制通(`082c75250`);z0-5 观感未验 | 归你 | 未量化(减少 z0-5 无效 HTTP) | 边界:不隐藏覆盖区与椭球区的真实高度悬崖(设计文档 §4.4) |
| **T-V9** | 运动中不出现黑格 / 黑带(瓦片画不满自己的地理范围) | 机制 | ⚠️ 差手捏合一轮 | `spanMis` 计数 + dark 均值 | 9480 帧 `spanMis=0`;本轮五机位全部 `spanMis=0` | 0 | 0 | 根因 = 模板键漏跨度,已修;详见冻结档案 |
| **T-V10** | 瓦片边界不出现发白细线 | 观感 | ✅(2026-08-16 你判) | 真机同机位 A/B + `seam_line_detect.py` | 竖线消失 | 保持 | 0 | ⚠️ 原核心证据「GPU TERR 置 OFF 即消失」依赖的开关已删(T-P10),**不可再复现**;三条未结见冻结档案 |
| **T-V12** | **加载期换代无硬矩形边**:粗细瓦交替期不出现可见的矩形补丁边界(新立 2026-09-05) | 观感 | ❌ | M-high 20 s 截图;换代帧数 | M-high 20 s 有明显矩形补丁(`remap=22 plainClip=14`) | 归你(淡入 / morph 过渡) | 未量化 | 与 T-V6 同属"加载期体面";T-V6 管平坑,本条管硬边 |
| **T-V13** | **坡面无人为条纹**:斜向"搓衣板"条纹不来自烘焙 / 量化(新立 2026-09-05) | 观感 | 🔒 | 换 257² 纹理复测:源条纹保留、烘焙放大消失 | M-mid / M-high 均有一致方向的斜条纹 | 若为 SRTM 源条纹 → 接受并记;若为烘焙放大 → 随 T-E1 消 | — | 三候选:SRTM 源本身、Terrain-RGB 8-bit 双线性、65² 上三点差分放大。不单独立项,随 T-E1 复测定性 |
| **T-V14** | **近景页纹素不阶梯**:300 m / −60° 机位,页存储面填充与页线的边缘不出现整数倍放大的纹素台阶,页 zoom 随后代瓦 SSE 走(新立 2026-09-05,「后代自己当页瓦」) | 观感 + 机制 | ⚠️ 机制落地,像素待判 | `PageDet zMax=` 与后代瓦 z 之差(≤6 且不被祖先 z12+6 钳);同机位 `debug.ee.descpages 0/1` 截图 | 模拟器 300 m/−6°:`descTiles=44 zMax=18`(选中瓦 z≤13,上限 19);机制 host 三测。模拟器 300 m/−60° 两次(120 s / 300 s)整屏落在单张 z13 瓦、`realTerrainTiles=0`、帧 8 s,DEM 始终没到 → 该机位模拟器给不出证据。2026-09-07 VOG-AL00:release(`.codexverify`)无 `amap-vector.json`(读内部 files 目录,非 debuggable 包 adb 写不进)→ `dem=(none)` 无地形;debug 包 run-as 塞入配置后 dem 生效但整屏黑、零帧日志(未究因)。近景像素证据仍待 PHK110 | 页纹素 ≈ 屏幕像素(zMax = 后代 z + SSE 档),截图无台阶 | T-P16 | 真机截图待 PHK110 回连;像素判断归你 |

**T-V4**(交互期上传不硬冻结 ✅ `294bff2ed`)、**T-V8**(Metal 对等 ❌,见 T-P1/T-P9)、
**T-V11**(模板 VBO 有界 ✅ `e7c47937`)三条无口径变化,详见 D 节索引。

---

## B. 性能轴(帧内时间)

预算基准 **已定(2026-09-05,你拍板):60 fps 稳,含较弱设备**。口径 = 单帧 p95 < 16.7 ms、
慢帧率(total >= 25 ms)< 1%,弱机(V1818T 档)同样适用——弱机 131 ms 地板不再是"已到底",
是欠账(见 F 表复审)。下表目标值不再标 `推断`。

| # | 判据 | 状态 | 度量方法 | 当前值 | 目标值 | 证据 / 差距 |
|---|---|---|---|---|---|---|
| **T-P7** | 新瓦入视的高度烘焙不产尖刺 | ⚠️ | `buildBreakdown` 的 `rebuild=` p95/max(release) | coarse 档无帧预算,16 ms/瓦(PHK110 release,2026-08-18) | ≤ 4 ms/帧 | 修向 = coarse 也加逐帧预算(仿 `denseBudget`);**先 simpleperf 归因**,勿凭 logcat 动手(曾误判在 Tileset 选择) |
| **T-P14** | **8 ms 主线程墙钟跨 producer 单账不分账**:地形/内容 finalize 与影像上传共用一个累加器,先执行者吃饱、后执行者当帧全停 | ❌ 未做(2026-09-05 立,核实自 I-P7 的横切根因) | 分别打 `TileUpdateUploadRunResult.terrainUploadMs` 与 `rasterUploadMs`,看 8 ms 被谁吃掉 | 未量化 | 每 producer 有独立时间账,或至少给影像上传留保底时间片 | `FrameResourceBudget::recordElapsed(FrameResourceLane, double)` **lane 参数是匿名的**——全部耗时进同一个 `mainThreadElapsedMs_`(`.cpp:259-261`);`mainThreadTimeExpired()` 是 `canFinalize` 的**第一道**检查(`.cpp:181-183`),满了就对所有 lane 一律拒。执行序 `TileUpdateUploadRunner::run`(`.h:47-62`)**地形/内容在前、影像在后**,共用同一 budget → 8 ms(`Tileset.h:111` 默认)系统性地先喂地形。**这是影像 I-P2 残余病灶的地形侧一半**,双向互引 imagery.md **I-P7**(发起侧 2026-09-05 已修)与 **I-P11**(上传排序未做);影像上传要真正提速,本条是前置 |
| **T-P15** | **Terrain / Content 分道装饰性**:两条 lane 在每一层闸口都并成一个池,地形高程与 glTF 内容在网络配额上互挤且无法互相保留 | ❌ 未做(2026-09-05 立,I-P7 同族)**⚠️ 调度口径债,非帧时** | `FrameResourceBudgetSnapshot` 已分别导出 `terrainContentNetworkRequestsIssued` 与 `contentNetworkRequestsIssued`,直接看两者比值 | 未量化 | 先判**是否有意为之**;若要分账则两道各自限额 + 各自 Scene producer | lane 由 `provider.providesTerrainQuadtree()` 选(`TileLoadRequestDispatcher.h:77-80`、`TileLoadScheduler.h:362-363`),但下游**四层全部合并**:`producerForLane` 两者同归 `Producer::Terrain`(`.cpp:14-19`,Scene 仲裁器分不出)、`stageForLane` 同归 `NetworkRequest`、`networkRequestLimit` 同吃 `maxTerrainContentNetworkRequestsPerFrame`(缺省回落 20)、`canIssue`/`tryIssue` 同增同判 `terrainContentNetworkRequestsIssued_`。`contentNetworkRequestsIssued_` 被写入并导出,**但没有任何闸读它**(只流向 `SceneTilesetDiagnostics`)——纯观测量,不是预算。**与 I-P7 同族**:那边是优先级传丢,这边是 lane 分账丢;两者都让「谁该先拿资源」在到达闸口时已无从判断 |
| **T-P8** | 模板淘汰重建不成 churn 尖刺 | 未量化 | `TEMPLATE_BUILD` 频次 / 首见次数(=T-E3) | M-high 一次扫描建 29 coarse + 3 dense | 重建率 < 10% | 池容量 128/16 相对峰值可见 103 瓦有余量;真机漫游观测 |
| **T-P2** | 非 GLES 后端 CPU 烘焙成本 | ✅ 已量化;**将删**(E″) | host Release 微基准 | coarse 0.087 ms / dense 0.938 ms 每瓦 | Metal 入口接上后物理删除 CPU 烘焙 | 修复归属 T-V8;方向见 E″ |
| **T-P3** | fill 代理构建成本 | ✅ 已量化;**当前路径已关**(T-V6) | host 微基准 | 0.072 ms/瓦;128 瓦一帧 10.6 ms | 不立项 | 若 T-V6 重开再看 |
| **T-P4** | HDR 变体常数 provisional | 有主(L-P3) | — | — | — | 随 HDR 唤醒 |
| **弱机地板** | V1818T GPU 帧时 | ❌ **欠账**(60 fps 含弱机目标下) | GPU 计时 | **131 ms/帧**(texop-bound,`a744801c2` 后) | < 16.7 ms | ALU/pass/texop 三层杠杆已用尽(F 表),达标只剩**分辨率类杠杆**(render scale / 动态分辨率)与 T-E1 后重算 texop 账;**此差距(131 → 16.7)量级说明弱机档需要独立的画质档位决策**,🔒 归你。PHK110 50-60 fps 正常。⚠️ **T-E1 的修法会加 texop**(法线全分辨率采样 ≈ +1 bilinear/像素 ≈ +6 ms@Adreno512),必须真机复测后才能记 ✅ |

---

## C. 资源占用轴(驻留与上限)

| # | 项 | 状态 | 度量方法 | 当前值 | 上限 / 目标 | 证据 / 差距 |
|---|---|---|---|---|---|---|
| **C1** | 高度图 CPU 常驻 | ⚠️ | `CpuAcct hm=` | **0.53 MB/瓦**(514² uint16),`maximumCachedBytes=192 MB`、300 瓦上限 → 地形可到 ~160 MB | 随 T-E1:GPU 纹理已含全信息后,CPU 侧降采样常驻(129² ≈ 33 KB/瓦,查高 / 贴地 / 碰撞够用) | 现在全分辨率常驻是烘焙源 + CPU 查高的需要;T-E1 落地前不能降 |
| **T-V11** | 模板 VBO 池 GPU 有界 | ✅ | 池容量 × 层大小 | coarse 132 KB×128 ≈ 16.9 MB + dense 2.02 MB×16 ≈ 32.3 MB | ≈ 49 MB 封顶 | `e7c47937`;淘汰释放路径 host 自证 |
| **C2** | 高度 / 法线纹理 array GPU | ✅ 有界 | 层数 × 层大小 | coarse 65²×4B×256 ≈ 4.3 MB + dense 257²×4B×48 ≈ 12.7 MB | ≈ 17 MB | ⚠️ T-E1 若把全档烘到 257²:256 层 × 264 KB = 68 MB,需把层数降到 ~96(峰值可见 103 瓦够)→ 净增约 +20 MB,**记入 T-E1 代价** |
| **C3** | 实例流 | ✅ | 字节/实例 | 144 B(T-P11 +16 B) | — | `TerrainInstanceBatcher::InstanceRecord` static_assert |
| **T-P1** | Metal 从未绑高度纹理 → Metal 侧 GPU 位移休眠 | ❌ | — | — | — | 与 T-V8 同根;修复前 Metal 端口不可用(T-P9) |
| **T-P5** | 模板键碰撞元凶 | ✅ 结构不可达 | 看门日志 `TEMPLATE_SCHEME_MISMATCH` | 9480 帧 0 次 | 0 | 详见冻结档案 |

---

## D. 资源调度效率轴(T-E,新立 2026-09-05)

> 这一轴回答「付出的资源里有多少真正到了屏幕」。前三轴可以全绿而这一轴一片红——
> T-E1 就是:帧时正常、内存有界、画面能看,但付了 8 倍的数据只用了 1 倍。

| # | 判据 | 状态 | 度量方法 | 当前值 | 目标值 | 代价 | 证据 / 差距 |
|---|---|---|---|---|---|---|---|
| **T-E1** | **高度数据利用率**:屏上消费的高度 / 法线信息 ÷ 下载解码的信息 | ❌ | 烘焙输出纹素 ÷ 源纹素(每瓦) | **65² / 514² ≈ 1/62** | ≥ 1/4(257² 纹理) | 见 C2(+20 MB GPU)、B 弱机地板(+1 bilinear/像素);CPU 常驻反降(C1) | 高度纹理尺寸绑死在模板栅格(`gridSize+1`)。正解 = 纹理与模板解耦:烘到 257²,顶点仍走 64/256 格模板位移,片元法线场按纹理全分辨率采。同时推动 T-V1 / T-V2 / T-V13 / C1,并顺带消掉 coarse/dense 双 array 拆批。AI 代理流 ~1 天 + 真机 texop 复测 |
| **T-E2** | **祖先回退过绘率**:回退帧里祖先几何被重画的倍数 | ✅(T-P11 后) | `plainClip` 命令数 ÷ 回退瓦数 | M-high 20 s `plainClip=14`→45 s `2`;稳态 M-mid `remap=40 plainClip=1` | ≈ 1(仅结构性回退走 plainClip) | 0 | T-P11 修好 remap 前是 4× 过绘 + discard 破 early-z。残余 plainClip 归 T-V5 的上采样祖先问题 |
| **T-E3** | **模板 / 高度层重建率**:淘汰后重入视野的重建 ÷ 首见 | 未量化 | `TEMPLATE_BUILD` 与层池 evict 计数 | — | < 10% | — | = T-P8 的度量口径 |
| **T-E4** | **按需渲染空转**:idle 判定前多跑的帧数,以及 idle 后不该跑却跑的帧 | ⚠️ | `FrameGate` 日志 + ShadowVerify(debug) | 停手后 ~16 帧 / 8 s 才 idle(接缝档案) | 🔒 **→ 2026-09-08 真机(PHK110)发现反向病:不是空转,是饿死**——冷启动发出约 13 帧后帧与加载全停(callback 间隔 181 s),根因是「HTTP 已到、待主线程 finalize」的载荷既无 `WorkLedger` 票也不唤醒(内容票是 Landing,到终态才放)。修法见 `pipeline.md` K′ 节(`d8a904d2`:待 finalize 积压持 Pumped 票),修后冷启动 8 s 收敛、正常 idle。**T-E4 的度量因此要分两侧:空转(跑多了)与饿死(该跑不跑),后者此前无判据** | — | 收敛链每一环都要申报(CLAUDE.md 帧收敛纪律);16 帧里哪些是必要收敛、哪些是白烧,未拆 。**2026-09-07 模拟器 M-high 冷启 240 s 从未 idle**(`pipeline.md` S-E7 / J 节) |
| **T-E5** | **离屏工作占比**:烘焙 / 上传 / 重钳里落在视锥外的比例 | ⚠️ 引擎级首测 | 逐阶段加"屏内 / 屏外"计数(已落地:`.tail` 行 `dw/db/uw/ub`,`pipeline.md` S-E1) | 模拟器 7 min:地形解码字节 64% 给不在需求集的瓦,上传字节 86% 在需求集内但仅 6% 给正在上屏的瓦(`pipeline.md` J 节) | 🔒 | — | 矢量侧 P4 曾量出 98.7% 候选屏外;引擎级口径与真机数见 `pipeline.md` S-E1 |

---

## E. 判据编号索引(全部编号一览,只增不改)

| 编号 | 轴 | 状态 | 一句话 |
|---|---|---|---|
| T-V1 | 体验 | ❌ | 近景地貌可辨(措辞重写) |
| T-V2 | 体验 | ⚠️ | 无刻面 |
| T-V3 | 体验 | ✅/待拍 | relief 不被压平 |
| T-V4 | 体验 | ✅ | 交互期上传不硬冻结(`294bff2ed`) |
| T-V5 | 体验 | ⚠️ | 稳态瓦界连续(措辞加严) |
| T-V6 | 体验 | ❌ 改判 | 加载期不露平坑(路径已关) |
| T-V7 | 体验 | ⚠️ | 未覆盖区回落椭球 |
| T-V8 | 体验 | ❌ | Metal 对等(三处未接线) |
| T-V9 | 体验 | ⚠️ | 运动中无黑带 |
| T-V10 | 体验 | ✅ | 无发白细线 |
| T-V11 | 资源 | ✅ | 模板 VBO 有界 |
| T-V12 | 体验 | ❌ | 加载期无硬矩形边(新) |
| T-V13 | 体验 | 🔒 | 坡面无人为条纹(新) |
| T-V14 | 体验 | ⚠️ | 近景页纹素不阶梯(新,后代自己当页瓦) |
| T-P1 | 资源 | ❌ | Metal 不绑高度纹理 |
| T-P2 | 性能 | ✅ 量化 | CPU 烘焙成本 |
| T-P3 | 性能 | ✅ 量化 | fill 代理成本 |
| T-P4 | 性能 | 有主 | HDR 常数 |
| T-P5 | 资源 | ✅ | 模板键碰撞不可达 |
| T-P6 | 验证债 | ⚠️ | shader 无执行级守卫(`test_glsl_compile` 覆盖编译,不覆盖数值) |
| T-P7 | 性能 | ⚠️ | 烘焙尖刺 |
| T-P8 | 性能 | 未量化 | 模板重建 |
| T-P9 | 资源 | ❌ | Metal 无退路(开关已删) |
| T-P10 | 验证债 | ⚠️ | GPU on/off A/B 器材消失 |
| T-P11 | 体验根因 | ✅ | remap 采错纬度行(已修 `782b2c94`) |
| T-P12 | 资源调度 | ⚠️ 诊断已落地 | 页存储 release 冷启偶发零页(15 km,此前 3 冷启 1 次:`pages=0/0` + 帧门控 idle)。2026-09-05 加 `PageStall` 看门狗:喂了瓦但池里零页持续 ≥2 s,每 3 s 报上次 determination 结局(没跑 / 被拒 / 静止跳过 / 瓦全被 RealTerrain 闸掉 / 跑了零页)+ 各队列长度;`logPageStallIfStuck` 在静止跳过、determination 末尾、tick 三处挂钩。加诊断后 3 次冷启均正常(21/24、19/24、24/24),未再复现;下次出现看 `PageStall` 行。模拟器上 15 km / 1200 m 冷启各报一条 `frame=2/8 realTerrainTiles=0`:模拟器一帧 ~1 s,2 s 宽限只够 2–8 帧,是启动瞬态不是卡死(宽限按墙钟,真机不会碰到) |
| 近景页预算 | 资源调度 | ✅ 机制 | 「后代自己当页瓦」前置:`Config::maxVisiblePages=384`(池 512 的 3/4),一次 determination 唯一页数超预算时把最细一档 cell 退档合并到父页直到达标(`PageDet budgetDemote=` 记退了几档);host `TerrainPageStoreBudget.*`。当前三机位唯一页 35–73,预算不触发,后代页瓦落地后模拟器 300 m/−6° 唯一页 186、`budgetDemote=0`,预算仍未触发;真机街道级视角待量 |
| T-P13 | 体验根因 | ❌ | 地形瓦包围体「紧」高度真机全为 0/0(`clampH=0/0(t44)`),祖先 heightmap 整块范围又是 10 km 瓦的 540 m。贴地体高与相机 / 剔除都吃这对数;2026-09-05 已在 SceneRenderPipeline 把退化紧范围当 loose、改为瓦内 9×9 采 DEM(按代次缓存)兜住,根因(GPU 位移路径没把 heightmap min/max 写回包围体)未修 |
| T-P14 | 性能 / 横切 | ❌ | 8 ms 主线程墙钟跨 producer 单账,地形先吃饱、影像当帧全停(互引 imagery.md I-P7) |
| T-P15 | 调度口径 | ❌ | Terrain/Content 分道装饰性,四层闸口全合并;`contentNetworkRequestsIssued_` 是观测量非预算(I-P7 同族) |
| T-P16 | 性能 / 资源 | ❌ 未量化 | 「后代自己当页瓦」的双份账:祖先(z12,64×64 cell walk)与 remap 后代同帧都进 determination;z≤18 页两边同 key 共用,z>18 页是净增(街道级视角上百页,预算 384 兜底)。模拟器稳态 `pageStore=0.12 ms`,冷启帧 `psUvp=45 psIndir=37 ms`(模拟器 ~1 s/帧,不算数);真机未量。减法方向:祖先只在其任一后代回落 plainClip 时才走 walk(需命令构建期回填) |
| T-E1 | 调度 | ❌ | 高度数据利用率 1/62 |
| T-E2 | 调度 | ✅ | 祖先回退过绘率 |
| T-E3 | 调度 | 未量化 | 模板重建率 |
| T-E4 | 调度 | ⚠️ | 按需渲染空转 |
| T-E5 | 调度 | 未量化 | 离屏工作占比 |

---

## E″. 已定方向(2026-09-05,你拍板):后端中立的「渲染进 array 层」入口

**决定**:GPU 侧所有"往 texture2DArray 某一层写内容"的工作(地形高度/法线烘焙、页存储
页合成、将来的线光栅)统一走一个 `RenderDevice` 后端中立入口——
`beginRenderToArrayLayer(texture, layer)` + 提交一批 2D 三角形/全屏 pass。
GLES 实现 = FBO + `glFramebufferTextureLayer`(现有 `setFramebufferColorLayer`);
Metal 实现 = render pass `colorAttachment.slice`。**只有一条 GPU 路径,不再保 CPU 孪生。**

**为什么**:此前"非 GLES 回落 CPU 烘焙/合成"被写成平台限制,实为代码库现状——
Metal 完全支持渲染进 array 层(自带 MSAA resolve / tile memory),缺的是
`RenderDeviceMetal` 的离屏入口(`supportsOffscreenPostProcess(){return false;}`)、
MSL 版 bake/合成 shader、高度纹理槽绑定(T-P1)、PSO 像素格式(L-P3)。全是"没写",
没有一条是"做不到"。为一个将来要删的 CPU 孪生付双实现的维护费不值(T-P6 已兑现过
一次分叉事故)。

**推论与时序**:
- T-P2「非 GLES 回落 CPU 烘焙」→ 改判为**将删**:Metal 入口接上后 CPU 烘焙整段物理删除。
- T-P6 里「CPU 那份不能删,没有 device 的场合它是唯一可执行实现」的前提**不再成立**:
  没有 device 的场合只有 host 测试,host 该走方案 A(离屏 GL 跑真 shader 逐 texel 对拍),
  不靠 CPU 孪生冒充。
- Metal 端口现状本就不可用(T-P9),不必为它保 CPU 路径;GLES 先实现,Metal 等设备到手
  一次补齐(接线 ~2–3 天 AI 代理流,**验收需 Metal 设备,是唯一硬约束**)。
- 帧内推进的 GPU 写层工作一律走 `WorkLedger` 票 + 每帧页数预算(帧收敛纪律)。

**页合成 GPU 化的账**(对照现状 `composeCpu≈1.8 s/60 帧`,31 页,worker):
worker 只剩投影 + 三角化(CDT 已有);渲染线程每页一次小 draw,无 PBO 上传;
GPU 每页一次 256² RTT ≈ 0.1 ms 量级;换肤 = 重画;RTT 可开 MSAA,边缘反而更好。
线进页时线段数是面的 10×,CPU 扫描线先撑不住,GPU 画三角形不在乎——这是它的真正收益点
(见 vector.md VE-8)。

**风险**(方案设计规矩,全列):①GLES 先行期 Metal 端口从"地形平"变"地形平 + 无面填充"
(已接受);②同帧"RTT 写层 → 地形采样同层"要走现有 completion serial /
`SubmissionLease`;③精度:Mercator 绝对坐标(~2e7 m)进 float32 会在 z17 页(0.6 m/px)抖 1–2 px,GLES/Metal 无 fp64——但**投影本身可在 GPU**:同地形 RTC,CPU 用 double 算每瓦一个原点 + 每页一个仿射(2×3),顶点以相对原点的 float 偏移下发,shader 里逐顶点投影(ln·tan 在一页内用页中心一阶展开,误差 ~1e-7 页宽);worker 只剩三角化 + 减原点;
④host 只能 `test_glsl_compile` + 真机,数值级归方案 A。

**页路径不三角化**(2026-09-05 补):平面多边形用 **stencil 奇偶法**——每环任取扇心,
边+扇心成三角形以 `INVERT` 画进 stencil,奇数像素即内部(孔洞、环方向、嵌套深度由硬件
奇偶天然处理,`AmapGeometry` 的环归一化/嵌套/CDT/越界 clip/质心校验整条链对页路径不再
需要);再一个 cover pass 上色。线不三角化:段端点实例化、顶点 shader 挤出四边形。

**GLES 落地(2026-09-05,PHK110 真机,release 变体量)**:`PageGeometryProvider` 接口
(`providers/PageVectorGeometry.h`)→ 页存储 `kickPageFetches` 单源栈探测到即改要页几何、
不取影像,并附该页"屏幕px/纹素"(页中心距离 + SSE 助手);`drainGpuRasterPages` 每帧 ≤4 页:
2× scratch FBO(stencil)上 **非零环绕**(`StencilPhase::WindingAccumulate` 两面 INCR/DECR_WRAP,
builder 把外环 / 孔归一化反向,一组一 draw)+ `ClassifyColor` cover;线条带(折线一条连续
条带、斜接封顶 2×、只在端点加圆帽)直画;再 `setFramebufferColorLayer` 把页层当色附件 blit
降采样(`PageRasterShaderGLSL.h`)。worker:取要素 → 投影到页局部(含 1/1024 页宽去重、
小于超采像素的环剔除)→ 扇三角 / 条带,构建丢共享池(不占解码线程)。**账**:
GPU 侧 1–3 ms/页(填充页 0.6 ms;带线 z12–14 页 13k 要素 / 24 万顶点 建缓冲 4–13 ms + pass 1–7 ms);
worker 构建 release 11–64 ms/页(debug -O0 3–4×)。同机位 1200 m 面填充:CPU 路 65 页
`composeCpu 2982 ms` → GPU 路 110 页渲染线程合计 50 ms;像素对拍 >32 灰阶差 0.16%。
**已判死(本次)**:逐要素奇偶(每要素 2 draw,z15 页几千 draw,~100 ms/页);每段四边形 +
每顶点 8 扇圆盘的条带(22 顶点/段,一页 50 万顶点,构建 200–440 ms)。非 GLES 后端回落
worker 光栅(仍是"将删",等 Metal 入口);资源建不出 → 关 GPU 路、作废该页。A/B 门
`debug.ee.gpupage 0`。host:`TerrainPageStoreGpuRaster.*`、`VectorSurfaceFill.PageGeometry*`;
shader 进 glsl-guard。**未做**:多源栈(alphaOver 次序)、页级 GPU 投影、Metal、
每页即建即毁 VBO/IBO(环形缓冲可再省建缓冲那 4–13 ms)。**未解**:release 冷启 15 km 偶发
`pages=0` 且帧门控空转(3 次冷启 1 次),日志被 OEM 配额丢弃未抓到判定行,记 T-P12。

**后代自己当页瓦(2026-09-05 落地,commit 见 git log;你拍板)**。此前真机 300 m 视角页纹素
0.5 m 明显阶梯,原因是页存储的"瓦"是**渲染瓦**:近景 z14–18 选中瓦全靠 z12 祖先 DEM remap,
页存储看到的是 z12,页 zoom 上限 = 12 + kMaxDetDepthLevels(6) = 18;6 不是随手常数——每瓦
一张 64×64 间接纹理,驻留编码每帧逐 cell 跑,再放大 CPU 撑不住。「纹素≈像素」细化规则
试过(per-cell 算得 z19,被 pZoom=18 钳住),不是瓶颈,已回退。**落地形态**:
`collectPageStoreTiles` 在 render tile 之外另收 remap 后代(祖先回退 + clip 窗 + 祖先有
retainedHeightmap,与命令构建的 `supportsTerrainHeightRemap` 同一判定);determination 对它们
免 RealTerrain 闸、以自己的 key 建 cell 网格 / 间接纹理(cell OBB 高度沿父链取最近有高度范围的
祖先),页 zoom 上限 = 后代 z + 6;`applyToTerrainCommand` 对 remap 命令(`surfaceClipEnabled==2`)
优先绑后代的间接纹理,几何仿射与 clipUv 的逆在 CPU 复合(片元 psUv 是祖先 UV)→ **GLSL / MSL /
batcher 零改动**。祖先照旧进 determination(remap 任一资源未就绪回落 plainClip 时仍要它)。
仅 GLES(remap 是 GLES 专属);A/B 门 `debug.ee.descpages 0`。host:`TerrainPageStoreDescendant.*`
三条(收集 / 放行且越过祖先上限 / 命令复合仿射与回落)。**证据(模拟器 arm64,PHK110 未连,
手机 VOG-AL00 卡在华为安装授权框未装)**:300 m/−6°:`descTiles=44 visibleCappedTiles=111
uniquePages=186 zMax=18 fullyResident=99/111 budgetDemote=0`,稳态 `pageStore=0.12 ms`;
15 km:`descTiles=35 uniquePages=155 zMax=14`;零 FATAL。像素归你(T-V14)。
**代价 / 债**:祖先与后代双份 determination(祖先 z12 瓦仍走 64×64 cell walk;后代新增 walk
在 gridN ≤ 64 内),z18 以下页两边共用同 key 不重复,z18 以上是净增页 → 记 T-P16;
真机 CPU / 页数账未量(模拟器帧 ~1 s,不能当性能证据)。

## F. 已判死 / 勿再提

要推翻需要新证据,不是新想法。**每条带上下文;上下文变了要重判**(元规则 2)。

| 方案 | 死因 | 上下文 |
|---|---|---|
| 逐瓦片高度量化 | 破坏无缝所需的逐位相等;正解是全局固定格点 | 与表示无关,长期有效 |
| 隐式瓦片(implicit tiling) | 零引用,但你已裁决保留 | 裁决 |
| 地形 draping 走影像路径(矢量 E4-4) | 根因已重定位 | ⚠️ 上下文已变:drape 路径本身已物理删除(`706fb156`),本条转为历史 |
| V1818T GPU 帧率再优化(ALU/pass/texop 层) | ALU/pass 四杠杆全死、MSAA 免费、texop 层到底 131 ms | `a744801c2` 时点。**2026-09-05 复审:死因仍成立,但"不再降"的裁决被「60 fps 含弱机」目标推翻**——三层杠杆到底 ≠ 目标可以放弃,剩下的是分辨率类杠杆(未判死)与 T-E1 后重算 texop 账。本条只判死"继续在 ALU/pass/texop 层找" |
| 把径向 / 几何密度当 T-V1 的解 | 2026-09-05:多档密度(`f69c39c5`)+ 跨档 morph(`dc0800fa`)已 revert;它们只解几何一半,在 65² 法线场下看不出收益 | 可在 T-E1 落地后**重开**(几何靠档位、细节靠纹理,两者配合才有意义) |
| 06261d40 式"高度 clip UV 用 geographic 投影" | 高度纹理行序 = 地形 scheme 投影(烘焙按源像素线性);geographic 在 WebMercator 祖先上差 ~6 纹素行成楔形 | T-P11,长期有效 |
| 字面"物理删除 GPU:off 路径"(连 baked VBO 一起删) | baked VBO 是四种结构性回退的生产路径(fade≈0 粗瓦 / 无自有 DEM 上采样瓦 / swap 失败 / 非 GLES);删 = Metal 地形变平 + 低 z limb faceting + 上采样瓦塌平 | 2026-09-05 裁决:只删开关机器 |

---

## G. 待你拍板

| 项 | 影响 | 我的建议 |
|---|---|---|
| **fill 代理是退役还是暂关**(T-V6) | T-V6 / T-P3 / M-graze 观感 | 我押重开:掠视远景平椭球+天际线是可见退化,成本已量化为可忽略(T-P3) |
| **T-E1 的目标纹理分辨率**(257² / 513²) | C2 显存、弱机 texop | 押 257²:z12 源 19 m/px,257² 在 z12 瓦上 ≈ 38 m/纹素,与几何 dense 档同分辨率;513² 显存 ×4 |
| T-V1′ / T-V12 / T-V13 观感目标 | 体验轴 | 先看 T-E1 落地后的 M-near / M-mid 截图再定 |

---

## H. 冻结档案(旧版整段保留,不再更新)

### H.1 T-V10 交接(2026-08-16 收集)

**现象**:瓦片边界出现**发白**细线,竖横都有、构成网格。GPU TERR 置 OFF 即消失
(⚠️ 该开关已于 2026-09-05 删除,此对照不可再复现,见 T-P10)。

**已钉死的事实**:

| 事实 | 依据 |
|---|---|
| 线是**发白**,不是背景色 | 放大图为奶油/灰白窄带,两侧地形连续 |
| **逐像素对比度仅 1~7 亮度单位** | 阈值 12 的逐像素检测器对肉眼清晰的线返回空 |
| 宽度量级 = **一个网格单元**(z7 约 4.9km≈7px) | 亚像素解释被排除 |
| **不随高度单调** | 1.69Mm 有(6.1σ)、198km 有(9.8σ),而 435km/839km 无 |

**根因①:源重叠环整列 no-data + 烘焙拿海平面 0m 去求斜率。** 生产 NASA 514 源普遍缺西/北
重叠环(抽样 19 片 14 片整列 `code=0`);烘焙把 no-data 当 0 m 送进边界差分,边界法线被打到
近水平(测试场 82.28°,修后 <3°);GPU 烘焙 `sampleH` 还漏移植 no-data 角剔除重归一化,
西边界高度砍半(908→454 m)。修 = no-data 邻居丢臂、中心取平;GPU 侧补重归一化;守卫
`TerrainEdgeNormalSeamTest.NoDataOverlapRingDoesNotWreckEdgeNormals`。**验收:真机同机位
A/B 竖线消失(你判)。**

未结三条:①「横线」从无证据(检测器 `--horizontal` 返回空,横向样本灵敏度低);②修前法线错
82° 屏上只 6 个亮度单位的量级落差未解释(疑大气压对比,未证);③GLSL 那份无执行级守卫(T-P6)。
工具 `tools/seam_line_detect.py`,阳性对照 `docs/assets/tv10/vline_repro_1p69Mm.png` x=363 6.1σ。
复现:camH≈1.7Mm 近正俯视或 ≈200km,`geoZ` 跨 0-7/0-10。

### H.2 T-V9 根因(2026-08-22)

共享位移模板按 `{schemeId,z,row,gridSize}` 缓存,由第一个来要的瓦片 bounds 定型且永久不自愈;
跨度不同的瓦片落到同一键 → 后来者拿到半宽模板,几何只铺一半、四周露背景。真机 `spanMis>0`
的 43 帧 dark 均值 0.0735,`spanMis=0` 的 8 帧 0.0003(差 245 倍);经度倍率恒 2.000 =
`rootTilesX` 差一倍的两套切片方案。修 = 地理跨度并进缓存键。T-P5 追元凶:`SchemeId::intern`
按字符串判等、两处 `acquire()` key/bounds 同源、单 scheme 下不可达;看门日志 9480 帧 0 次。

### H.3 2026-09-05 收口记账(原 E′ 节)

**地形几何回退到 730bee98**(用户裁决:730bee98 之后 GPU:on 出同级瓦接缝):revert `f69c39c5`
多档密度、`dc0800fa` 跨档 morph;`06261d40` 地形部分曾整体回退,**同日分离实验证明它就是
接缝根因,随后改为修好而非回退**(T-P11)。保留 `721af2dd`(shared_ptr HeightSource,
worker 重钳前置)与 `706fb156` 的 `flushEdgeLutUploads` bool。

**删除 GPU 位移 A/B 开关**:`Engine::terrainGpuDisplacementEnabled_` 及 setter/getter、
`Renderer::terrainBakedVboSkipEnabled_`、GLESView 两个 JNI、`GLESView.java` 声明、MainActivity
按钮、`EnvSnapshot.terrainGpuDisplacement`、死代码 `Tileset::reloadGhostReleasedTerrainContent`。
池由 `Engine::render` 无条件创建;`terrainSharedTemplateActive()` 只看池是否存在。baked VBO
路径保留(四种结构性回退),去留收归 `TerrainTemplateEligibility` 一处判据。新债 T-P9 / T-P10。

**T-P11 remap 修复**:分离实验(MVT 关、tier 已回退、只切 remap 判据)两组对照钉死根因;
三处混用(overlay 投影 / `rasterOverlayRectangles[0]` / 3% 封缝外扩)+ 两处同类分叉(实例化路径
无 heightClipUv、两套片元法线场用影像 UV)。修 = `TileSurfaceClip::forHeightRemap`(地形 scheme
投影 via `TerrainRasterOverlayProjectionResolver::forTileKey`、祖先 bounds、无外扩)、实例流
128→144 B、`v_heightUv` varying。host 守卫 `HeightRemapClipUsesTerrainSchemeProjectionWithoutSeamMargin`
(Mercator 0.5 vs 纬度线性 0.579)+ `…IsLatLinearForGeographicScheme`;模拟器隔离与生产条件
同机位 `remap=4 plainClip=0`,楔形与右缘明暗带消失。**缺口**:实例化路径未在模拟器命中
(首屏 `batch=0`),GLSL 只过离线编译(T-P6 类)。代价:+16 B/实例、1 个 vec2 varying、每 remap
命令一次矩形投影。

**纯地形五机位扫描(swiftshader,`noamap=1`)**:M-near 软包(T-V1);M-mid 稳态台阶缝(T-V5)
+ 斜条纹(T-V13);M-high 20 s 矩形补丁(T-V12)、45 s 收敛;M-graze 远景平椭球(T-V6 已关);
M-coarse 早期 `drop=0/12`。五机位全 `spanMis=0`、零 crash。

---

## 更新协议

- 专项收官改状态 + 四列(度量 / 当前 / 目标 / 代价),附 commit / 真机数据 / 截图。
- 你提出新体验要求 → 立刻加判据(哪怕 ❌),同时给度量方法;给不出度量方法先标 🔒。
- 新增开销 → 同一次提交更新代价列与 C 节;测不了进 B/C 节记债。
- 方案被否决 → F 节附死因 + 上下文。
- **删除 / 替换渲染表示 → 本文逐条改判(元规则 2),与 AI_INDEX 同级。**
- 横切根因 → 相关模块互引(元规则 3)。
