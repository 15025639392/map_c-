#include "earth_engine/content/SeamAudit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace earth_engine {

namespace {

// 同级邻接的键：东邻（x+1）与南邻（y+1，XYZ 顶行原点、y 向南）。
TileKey eastNeighbor(const TileKey& key) { return TileKey(key.z(), key.x() + 1, key.y()); }
TileKey southNeighbor(const TileKey& key) { return TileKey(key.z(), key.x(), key.y() + 1); }

// 比较一条共享边：aMesh 的边 col（axis=0 东西）或 row（axis=1 南北）节点 vs
// bMesh 的相对边节点；沿共享边的方向 i = 0..n（n = nodesPerEdge）。
void compareEdge(const TerrainMeshData& a, const TerrainMeshData& b, int edgeAxis,
                 SeamAuditResult& result) {
    if (a.nodesPerEdge <= 0 || a.nodesPerEdge != b.nodesPerEdge) {
        return; // 双侧网格规格须一致（同帧同 nodesPerEdge 的正常前提）
    }
    const int n = a.nodesPerEdge;
    const int stride = n + 1;
    result.comparedEdges += 1;
    for (int i = 0; i < stride; ++i) {
        // a 在东/南侧边取自身外沿，b 在西/北侧取自身外沿——理想无缝时两者是
        // 同一物理点（同级共享边）。
        const Vec3& pa = edgeAxis == 0
                             ? a.positionsEcef[static_cast<size_t>(i * stride + n)]     // a 东列
                             : a.positionsEcef[static_cast<size_t>(n * stride + i)];    // a 南行
        const Vec3& pb = edgeAxis == 0
                             ? b.positionsEcef[static_cast<size_t>(i * stride + 0)]     // b 西列
                             : b.positionsEcef[static_cast<size_t>(0 * stride + i)];    // b 北行
        const double diff = (pa - pb).magnitude();
        result.meanMeters += diff;
        result.maxMeters = std::max(result.maxMeters, diff);
        result.comparedNodePairs += 1;
        if (diff > 1.0) {
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

} // namespace earth_engine
