#pragma once

#include <optional>
#include <vector>

#include "../tiling/TileKey.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// 一块瓦的解码高度栅格（row-major，row0 = 北；与 HeightmapCodec/Tile 约定一致）。
struct TerrainGrid {
    std::vector<double> heights;
    int width = 0;
    int height = 0;

    bool empty() const { return heights.empty() || width <= 0 || height <= 0; }
    int count() const { return width * height; }
};

/// 地形高度数据源抽象：按瓦返回解码后的高度栅格。
///
/// 本仓的 host 端地形帧只依赖这个接口——将来阶段 6 并入 gis-md 现成地形服务
/// 时，把其 HeightmapTerrainContentProvider / DEM 解码链适配成一个实现即可，
/// 装配与几何代码不动。
class ITerrainDataSource {
public:
    virtual ~ITerrainDataSource() = default;

    /// 请求某瓦的解码高度栅格（gridSize × gridSize）。
    /// 返回 nullopt = 该瓦数据不可用（装配层跳过该瓦；祖先回退/换代属调度阶段，
    /// 不在本接口语义内）。
    virtual std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                                      const TileKey& key,
                                                      int gridSize) const = 0;
};

} // namespace earth_engine
