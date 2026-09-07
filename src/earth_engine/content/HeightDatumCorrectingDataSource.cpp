#include "earth_engine/content/HeightDatumCorrectingDataSource.h"

#include <vector>

#include "earth_engine/content/HeightmapTile.h"

namespace earth_engine {

HeightDatumCorrectingDataSource::HeightDatumCorrectingDataSource(
    const ITerrainDataSource& inner, const IHeightDatumCorrector& corrector)
    : inner_(inner), corrector_(corrector) {}

std::optional<TerrainGrid> HeightDatumCorrectingDataSource::requestHeights(
    const WebMercatorTileScheme& scheme, const TileKey& key, int gridSize) const {
    std::optional<TerrainGrid> grid = inner_.requestHeights(scheme, key, gridSize);
    if (!grid || grid->empty() || grid->width != gridSize || grid->height != gridSize) {
        return grid;
    }
    // 恒等改正器：零拷贝快路径（默认口径：不启用改正，逐位不变）。
    if (dynamic_cast<const IdentityHeightDatumCorrector*>(&corrector_) != nullptr) {
        return grid;
    }
    // 每像素取经纬 → 叠加 undulation。no-data 哨兵不改（保持"无数据"信号）。
    std::vector<double> zeros(static_cast<size_t>(gridSize) * gridSize, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), gridSize, gridSize);
    const bool hasSentinels = !grid->noDataValues.empty();
    for (int row = 0; row < gridSize; ++row) {
        for (int col = 0; col < gridSize; ++col) {
            const size_t idx = static_cast<size_t>(row) * gridSize + col;
            if (hasSentinels) {
                const double h = grid->heights[idx];
                bool noData = false;
                for (const double sentinel : grid->noDataValues) {
                    if (h == sentinel) {
                        noData = true;
                        break;
                    }
                }
                if (noData) {
                    continue;
                }
            }
            const Cartographic c = probe.pixelToCartographic(col, row);
            grid->heights[idx] += corrector_.undulationMeters(c);
        }
    }
    return grid;
}

} // namespace earth_engine
