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

    /// 本瓦 no-data 哨兵值列表（米，精确匹配；空 = 无哨兵，行为与旧版一致）。
    /// 解码源按编码填写（镜像 gis-md 解码 worker 语义）：
    /// - Terrain-RGB 源**隐式注册** RGB(0,0,0) 的解码底值
    ///   `HeightmapCodec::kTerrainRgbNoDataFloorMeters`（-10000）；
    /// - Terrarium 源无隐式哨兵（Mapzen 未定义 nodata 像素，-32768 是合法底值）。
    /// 下游（HeightmapTile 的 min/max 与采样）遇到哨兵时排除，防止 -10000 混进
    /// min/max 或边缘双线性（gis-md 根因档案：假深沟/假悬崖法线）。
    std::vector<double> noDataValues;

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
