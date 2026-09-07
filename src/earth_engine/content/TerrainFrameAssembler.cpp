#include "earth_engine/content/TerrainFrameAssembler.h"

namespace earth_engine {

std::vector<TerrainFrameAssembler::Frame> TerrainFrameAssembler::assemble(
    const WebMercatorTileScheme& scheme, const TerrainLodResult& selection,
    const ITerrainDataSource& source, const Ellipsoid& ellipsoid, int gridSize,
    int nodesPerEdge) const {
    std::vector<Frame> frames;
    frames.reserve(selection.tiles.size());
    const TerrainTileMeshBuilder meshBuilder;
    for (const TileKey& key : selection.tiles) {
        const std::optional<TerrainGrid> grid = source.requestHeights(scheme, key, gridSize);
        if (!grid || grid->empty() || grid->width != gridSize || grid->height != gridSize) {
            continue; // 数据不可用/畸形：跳过（祖先回退属调度阶段）
        }
        const HeightmapTile tile(scheme, key, grid->heights.data(), grid->width, grid->height,
                                 grid->noDataValues.data(),
                                 static_cast<int>(grid->noDataValues.size()));
        Frame frame;
        frame.key = key;
        frame.mesh = meshBuilder.build(tile, ellipsoid, nodesPerEdge);
        const auto [minH, maxH] = tile.minMaxHeight();
        frame.minHeight = minH;
        frame.maxHeight = maxH;
        frames.push_back(std::move(frame));
    }
    return frames;
}

} // namespace earth_engine
