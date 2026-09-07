#pragma once

#include <vector>

#include "TerrainFrameAssembler.h"

namespace earth_engine {

/// 同级共享边审计（T-V5「瓦界 <1m」的 host 取证仪器）。
///
/// 给定一帧已装配网格，统计所有**同级**（同 z）东西/南北邻接瓦对共享边上的
/// ECEF 逐节点差。用途：
/// - 顶点栅格源（带重复边界列/共享 post）→ 双侧网格共享边应逐点重合（≈0）；
/// - 无重叠环的连续栅格源（本仓 assets 实测形态：相邻瓦边界 posts 相距 1 个
///   像元、无共享列）→ 贴边 CLAMP 采样让双侧各取到边界外/内最近的 post，
///   差 ≈ |坡度|·像元间距（实测 z13 均值 ~1.8m、z12 ~3.9-4.7m）——
///   该差距是 T-V5 整链前的**机制边界**，需带重叠环源（B2）或边 LUT/吸附（B4）
///   关闭；此仪器用于量化取证与回归门禁。
struct SeamAuditResult {
    /// 帧中存在同级邻接（东西/南北）的共享边数。
    int edgePairsFound = 0;
    /// 双侧网格均有效并完成比较的边数（nodesPerEdge 须一致）。
    int comparedEdges = 0;
    /// 参与比较的共享边节点对数。
    int comparedNodePairs = 0;
    /// ECEF 差 > 1.0 m 的节点对数（T-V5 判据口径 1m）。
    int nodePairsOverOneMeter = 0;
    /// 全部节点的平均 ECEF 差（米）。
    double meanMeters = 0.0;
    /// 最差节点 ECEF 差（米）。
    double maxMeters = 0.0;
};

/// 审计一帧内所有同级邻接共享边（东邻与南邻各计一次）。
SeamAuditResult auditSameLevelSharedEdges(
    const std::vector<TerrainFrameAssembler::Frame>& frames);

/// 跨级 T-顶点裂缝审计（T-V5 跨级半边 / T-V12 换代族的 host 取证，B4 前身）。
///
/// 场景：粗瓦 P(z) 与东/南邻 **z+1 子瓦**（邻瓦四叉树细分）共享边界。子瓦边界
/// 上的节点以自身加密度采样真实表面；粗瓦边界是一条经过其 n+1 个顶点的**弦折线**
/// （弦线只在粗顶点处贴合表面）。子瓦落在粗弦段之间的 T-顶点（真实表面点）与
/// 粗弦的偏差 = 裂缝（含椭球曲率弦垂 与 地形曲率两项）。
///
/// 度量：每个子瓦边界节点到粗瓦边界弦折线的最近 ECEF 距离。注意
/// 本度量 **>0 是常态**（弦垂不可能为 0，除非共享顶点重合），与同级审计
/// （理想 ≈0）口径不同——用途是量化换代裂缝量级并做门禁：
/// 修复 = 子瓦边界顶点吸附到粗弦（B4 remap/边 LUT），吸附量 = 本度量。
SeamAuditResult auditCrossLevelTVertexGap(
    const std::vector<TerrainFrameAssembler::Frame>& frames);

} // namespace earth_engine
