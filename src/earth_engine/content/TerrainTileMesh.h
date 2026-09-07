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
///   节点 (i,j) 的像素坐标 = (i/(n)·(w-1), j/(n)·(h-1))，用双线性采样取高；
/// - 节点落位走 mercator 米（经 tile.pixelToCartographic），不做经纬线性。
/// 节点贴瓦片边界：第 0/n 行与第 0/n 列落在瓦边上 → 同级相邻瓦共享边顶点
/// 可由两侧各自网格精确重合（无缝契约 T-V5 的机制前提）。
class TerrainTileMeshBuilder {
public:
    /// nodesPerEdge >= 2（网格点 = nodesPerEdge+1 每边）。
    TerrainMeshData build(const HeightmapTile& tile, const Ellipsoid& ellipsoid,
                          int nodesPerEdge) const;
};

} // namespace earth_engine
