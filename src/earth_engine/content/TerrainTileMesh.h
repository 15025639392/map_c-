#pragma once

#include <cstdint>
#include <vector>

#include "HeightmapTile.h"
#include "../core/geodesy/Ellipsoid.h"
#include "../core/math/Vec3.h"

namespace earth_engine {

/// 一块瓦片的地形网格（ECEF 顶点 + 三角形索引 + 平滑法线）。
/// 由 TerrainTileMeshBuilder 从 HeightmapTile 生成；渲染/剔除/无缝对拍用。
struct TerrainMeshData {
    /// (nodesPerEdge+1)² 个顶点；行序 row0=北（与高度图一致），行内 i 向东。
    std::vector<Vec3> positionsEcef;
    /// 三角形索引（每 3 个 = 一个三角形，外向绕序）。
    std::vector<uint32_t> indices;
    /// 与 positionsEcef 对齐的平滑顶点法线（单位向量）。
    std::vector<Vec3> normals;
    int nodesPerEdge = 0;
};

/// 从一块已解码高度图构建该瓦片的地形网格。
///
/// 几何与内容解耦（地形判据 T-E1 的原则 host 侧体现）：
/// - 几何节点数 nodesPerEdge 与高度图分辨率（tile.width()/height()）相互独立；
///   节点 (i,j) 的网格分数 = (i/n, j/n)，落位走 mercator 米（贴瓦界）；
/// - 采样走**数据缓冲**的配准坐标：默认顶点栅格（borderInset=0）时与节点同位；
///   带 1px 重叠环源（borderInset=0.5，cell-registered）时边界节点读环内邻瓦回填
///   → 同级相邻瓦共享边取到同一批世界样本（无缝机制前提，T-V5；见 TerrainGrid::
///   borderInset 与 test_ring_source_seam）。
class TerrainTileMeshBuilder {
public:
    /// nodesPerEdge >= 2（网格点 = nodesPerEdge+1 每边）。
    TerrainMeshData build(const HeightmapTile& tile, const Ellipsoid& ellipsoid,
                          int nodesPerEdge) const;
};

} // namespace earth_engine
