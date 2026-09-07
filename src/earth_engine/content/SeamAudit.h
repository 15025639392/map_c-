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

} // namespace earth_engine
