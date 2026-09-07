#include "earth_engine/content/AncestorFallbackDataSource.h"

#include <utility>
#include <vector>

#include "earth_engine/content/HeightmapTile.h"

namespace earth_engine {

AncestorFallbackDataSource::AncestorFallbackDataSource(const ITerrainDataSource& inner,
                                                       int maxFallbackLevels)
    : inner_(inner), maxFallbackLevels_(maxFallbackLevels) {}

std::optional<TerrainGrid> AncestorFallbackDataSource::requestHeights(
    const WebMercatorTileScheme& scheme, const TileKey& key, int gridSize) const {
    if (gridSize <= 0) {
        return std::nullopt;
    }
    // 1) 沿父链找最近可用瓦（请求键本身算第 0 层）。
    TileKey current = key;
    std::optional<TerrainGrid> grid;
    for (int depth = 0; depth <= maxFallbackLevels_; ++depth) {
        grid = inner_.requestHeights(scheme, current, gridSize);
        if (grid) {
            break;
        }
        const auto parent = current.parent();
        if (!parent) {
            break;
        }
        current = *parent;
    }
    if (!grid || grid->empty() || grid->width != gridSize || grid->height != gridSize) {
        return std::nullopt; // 任意祖先层都无可用数据 / 畸形 → 不冒充数据
    }
    if (current == key) {
        return grid; // 直接命中：原样透传
    }

    // 2) 祖先命中：把祖先栅格重采样到请求瓦的栅格（父数据、子几何）。
    //    祖先瓦按自身键构建内容视图；请求瓦每个像素取地理坐标后在祖先瓦查高。
    TerrainGrid out;
    out.width = gridSize;
    out.height = gridSize;
    out.noDataValues = grid->noDataValues; // 哨兵语义随值传播
    out.heights.resize(static_cast<size_t>(gridSize) * gridSize);
    const HeightmapTile ancestorTile(scheme, current, grid->heights.data(), grid->width,
                                     grid->height, grid->noDataValues.data(),
                                     static_cast<int>(grid->noDataValues.size()),
                                     grid->borderInset);
    std::vector<double> zeros(static_cast<size_t>(gridSize) * gridSize, 0.0);
    const HeightmapTile childProbe(scheme, key, zeros.data(), gridSize, gridSize);
    for (int row = 0; row < gridSize; ++row) {
        for (int col = 0; col < gridSize; ++col) {
            const Cartographic c = childProbe.pixelToCartographic(col, row);
            const std::optional<double> h = ancestorTile.sampleHeightAt(c);
            if (!h) {
                return std::nullopt; // 正常四叉树祖先应完全覆盖子瓦；异常即放弃
            }
            out.heights[static_cast<size_t>(row) * gridSize + col] = *h;
        }
    }
    return out;
}

} // namespace earth_engine
