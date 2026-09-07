#include "earth_engine/content/SeamAudit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace earth_engine {

namespace {

// 同级邻接的键：东邻（x+1）与南邻（y+1，XYZ 顶行原点、y 向南）。
TileKey eastNeighbor(const TileKey& key) { return TileKey(key.z(), key.x() + 1, key.y()); }
TileKey southNeighbor(const TileKey& key) { return TileKey(key.z(), key.x(), key.y() + 1); }

// 粗瓦 P(z,x,y) 东邻 Q=(z,x+1,y) 的两个子瓦（沿 P 东边自北向南堆叠）。
std::array<TileKey, 2> eastChildren(const TileKey& parent) {
    return {TileKey(parent.z() + 1, (parent.x() + 1) * 2, parent.y() * 2),
            TileKey(parent.z() + 1, (parent.x() + 1) * 2, parent.y() * 2 + 1)};
}
// 南邻 Q=(z,x,y+1) 的两个子瓦（沿 P 南边）。
std::array<TileKey, 2> southChildren(const TileKey& parent) {
    return {TileKey(parent.z() + 1, parent.x() * 2, (parent.y() + 1) * 2),
            TileKey(parent.z() + 1, parent.x() * 2 + 1, (parent.y() + 1) * 2)};
}

// 比较一条同级共享边：aMesh 的外沿边（axis 0 = 东列 / 1 = 南行）节点 vs
// bMesh 的相对边节点（axis 0 = 西列 / 1 = 北行）。
void compareEdge(const TerrainMeshData& a, const TerrainMeshData& b, int edgeAxis,
                 SeamAuditResult& result) {
    if (a.nodesPerEdge <= 0 || a.nodesPerEdge != b.nodesPerEdge) {
        return; // 双侧网格规格须一致（同帧同 nodesPerEdge 的正常前提）
    }
    const int n = a.nodesPerEdge;
    const int stride = n + 1;
    result.comparedEdges += 1;
    for (int i = 0; i < stride; ++i) {
        const Vec3& pa = edgeAxis == 0
                             ? a.positionsEcef[static_cast<size_t>(i * stride + n)]  // a 东列
                             : a.positionsEcef[static_cast<size_t>(n * stride + i)]; // a 南行
        const Vec3& pb = edgeAxis == 0
                             ? b.positionsEcef[static_cast<size_t>(i * stride + 0)]  // b 西列
                             : b.positionsEcef[static_cast<size_t>(0 * stride + i)]; // b 北行
        const double diff = (pa - pb).magnitude();
        result.meanMeters += diff;
        result.maxMeters = std::max(result.maxMeters, diff);
        result.comparedNodePairs += 1;
        if (diff > 1.0) {
            result.nodePairsOverOneMeter += 1;
        }
    }
}

// 点到线段最近距离（ECEF）。
double pointSegmentDistance(const Vec3& p, const Vec3& a, const Vec3& b) {
    const Vec3 ab = b - a;
    const double lenSq = ab.magnitudeSquared();
    if (lenSq <= 0.0) {
        return (p - a).magnitude();
    }
    const double t = std::clamp((p - a).dot(ab) / lenSq, 0.0, 1.0);
    return (p - (a + ab * t)).magnitude();
}

// 子瓦边界节点到粗瓦边弦折线的最小距离（粗瓦外沿顶点序列）。
void accumulateChildToCoarse(const TerrainMeshData& child, int childEdgeAxis,
                             const TerrainMeshData& coarse, int coarseEdgeAxis,
                             SeamAuditResult& result) {
    if (child.nodesPerEdge < 2 || coarse.nodesPerEdge < 2) {
        return; // 网格构建要求 nodesPerEdge ≥ 2（无顶点 = 不可比）
    }
    const int strideChild = child.nodesPerEdge + 1;
    const int strideCoarse = coarse.nodesPerEdge + 1;
    if (child.positionsEcef.size() < static_cast<size_t>(strideChild) ||
        coarse.positionsEcef.size() < static_cast<size_t>(strideCoarse * strideCoarse)) {
        return;
    }
    // 粗瓦边弦折线顶点序列（axis 0 = 东列自上而下；1 = 南行自西向东）。
    std::vector<Vec3> polyline;
    polyline.reserve(static_cast<size_t>(strideCoarse));
    for (int i = 0; i < strideCoarse; ++i) {
        polyline.push_back(coarseEdgeAxis == 0
                               ? coarse.positionsEcef[static_cast<size_t>(i * strideCoarse +
                                                                          strideCoarse - 1)]
                               : coarse.positionsEcef[static_cast<size_t>((strideCoarse - 1) *
                                                                              strideCoarse +
                                                                          i)]);
    }
    result.comparedEdges += 1;
    for (int j = 0; j < strideChild; ++j) {
        const Vec3& w =
            childEdgeAxis == 0
                ? child.positionsEcef[static_cast<size_t>(j * strideChild)] // 子瓦西列
                : child.positionsEcef[static_cast<size_t>(0 * strideChild + j)]; // 子瓦北行
        double best = std::numeric_limits<double>::max();
        for (size_t s = 0; s + 1 < polyline.size(); ++s) {
            best = std::min(best, pointSegmentDistance(w, polyline[s], polyline[s + 1]));
        }
        result.meanMeters += best;
        result.maxMeters = std::max(result.maxMeters, best);
        result.comparedNodePairs += 1;
        if (best > 1.0) {
            result.nodePairsOverOneMeter += 1;
        }
    }
}

} // namespace

SeamAuditResult auditSameLevelSharedEdges(
    const std::vector<TerrainFrameAssembler::Frame>& frames) {
    SeamAuditResult result;
    if (frames.empty()) {
        return result;
    }
    std::unordered_map<TileKey, size_t> index;
    for (size_t i = 0; i < frames.size(); ++i) {
        index.emplace(frames[i].key, i);
    }
    for (const auto& frame : frames) {
        const TileKey key = frame.key;
        // 东邻：一条共享边（a 东列 vs b 西列）。
        const auto east = index.find(eastNeighbor(key));
        if (east != index.end()) {
            result.edgePairsFound += 1;
            compareEdge(frame.mesh, frames[east->second].mesh, 0, result);
        }
        // 南邻：一条共享边（a 南行 vs b 北行）。
        const auto south = index.find(southNeighbor(key));
        if (south != index.end()) {
            result.edgePairsFound += 1;
            compareEdge(frame.mesh, frames[south->second].mesh, 1, result);
        }
    }
    if (result.comparedNodePairs > 0) {
        result.meanMeters /= static_cast<double>(result.comparedNodePairs);
    }
    return result;
}

SeamAuditResult auditCrossLevelTVertexGap(
    const std::vector<TerrainFrameAssembler::Frame>& frames) {
    SeamAuditResult result;
    if (frames.empty()) {
        return result;
    }
    std::unordered_map<TileKey, size_t> index;
    for (size_t i = 0; i < frames.size(); ++i) {
        index.emplace(frames[i].key, i);
    }
    for (const auto& frame : frames) {
        const TileKey key = frame.key;
        // 作为粗瓦：东邻的两个子瓦若在场 → 子瓦西列对粗瓦东边弦。
        for (const TileKey& child : eastChildren(key)) {
            const auto it = index.find(child);
            if (it != index.end()) {
                result.edgePairsFound += 1;
                accumulateChildToCoarse(frames[it->second].mesh, 0, frame.mesh, 0, result);
            }
        }
        // 南邻的两个子瓦若在场 → 子瓦北行对粗瓦南边弦。
        for (const TileKey& child : southChildren(key)) {
            const auto it = index.find(child);
            if (it != index.end()) {
                result.edgePairsFound += 1;
                accumulateChildToCoarse(frames[it->second].mesh, 1, frame.mesh, 1, result);
            }
        }
    }
    if (result.comparedNodePairs > 0) {
        result.meanMeters /= static_cast<double>(result.comparedNodePairs);
    }
    return result;
}

} // namespace earth_engine
