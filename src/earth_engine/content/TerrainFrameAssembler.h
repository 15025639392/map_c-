#pragma once

#include <vector>

#include "HeightmapTile.h"
#include "TerrainDataSource.h"
#include "TerrainTileMesh.h"
#include "../tiling/TerrainLodSelector.h"

namespace earth_engine {

/// 地形帧装配器：把"LOD 选择结果"喂给数据源，产出每瓦的 ECEF 网格——
/// host 可跑的"选中瓦 → 解码 → 查高 → 网格"一帧（阶段 6 的装配前身）。
class TerrainFrameAssembler {
public:
    struct Frame {
        TileKey key;
        TerrainMeshData mesh;
        /// 瓦内解码高度 min/max（包围体/剔除用）。
        double minHeight = 0.0;
        double maxHeight = 0.0;
    };

    /// selection 的每个键请求解码（gridSize²），再以 nodesPerEdge 网格建 ECEF 网格。
    /// 数据源返回 nullopt 的瓦被跳过（帧数可能少于选中数）。
    std::vector<Frame> assemble(const WebMercatorTileScheme& scheme,
                                const TerrainLodResult& selection,
                                const ITerrainDataSource& source,
                                const Ellipsoid& ellipsoid, int gridSize,
                                int nodesPerEdge) const;
};

} // namespace earth_engine
