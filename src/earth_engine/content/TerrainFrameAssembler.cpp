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
        if (!grid || grid->empty()) {
            continue; // 数据不可用：跳过（祖先回退属调度阶段/装饰器）
        }
        // 栅格尺寸合法：顶点栅格 = gridSize²；cell-registered 环源（borderInset>0）
        // = (gridSize+2)²（512 cell + 1px 环；网格采样按 tile 内缩映射，见 builder）。
        const bool ringGrid = grid->borderInset > 0.0 &&
                              grid->width == gridSize + 2 && grid->height == gridSize + 2;
        if (grid->width < 2 || grid->height < 2 ||
            !(grid->width == gridSize || ringGrid)) {
            continue; // 畸形尺寸：跳过
        }
        const HeightmapTile tile(scheme, key, grid->heights.data(), grid->width, grid->height,
                                 grid->noDataValues.data(),
                                 static_cast<int>(grid->noDataValues.size()),
                                 grid->borderInset);
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
