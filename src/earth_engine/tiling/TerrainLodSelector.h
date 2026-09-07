#pragma once

#include <vector>

#include "../core/geodesy/Ellipsoid.h"
#include "../core/math/Rectangle.h"
#include "../core/math/Vec3.h"
#include "TileKey.h"
#include "WebMercatorTileScheme.h"

namespace earth_engine {

/// LOD 瓦片选择的配置（地形瓦片树前身的 host 可测形态）。
struct TerrainLodConfig {
    TerrainLodConfig()
        : viewportHeightPx(1080.0),
          fovRadians(1.0471975511965976), // 60°
          maxScreenSpaceErrorPx(16.0),
          geometricErrorScale(0.001),
          maxLevel(14) {}

    double viewportHeightPx;
    double fovRadians;
    /// SSE 阈值：某瓦几何误差在该相机距离下的屏幕像素 > 阈值 → 细化。
    double maxScreenSpaceErrorPx;
    /// 几何误差代理系数：error(z) = geometricErrorScale × 瓦片 mercator 尺寸。
    /// 瓦内地形相对椭球的位移通常远小于瓦宽（如 1000 m 起伏 vs 数千 km 瓦）；
    /// 真实值应来自源元数据/高度图 min-max（量化网格 geometricError），
    /// 该系数只是选择机制骨架的可调代理。
    double geometricErrorScale;
    /// 允许的最深层级（防近地 runaway）。
    int maxLevel;
};

/// 选择结果：瓦片键集合（无重复、无父子同时出现 = 四叉树的一个划分）。
struct TerrainLodResult {
    std::vector<TileKey> tiles;
};

/// SSE 驱动的四叉树 LOD 选择器。
///
/// 规则（与 gis-md/cesium 语义对齐的骨架）：
/// - 从根瓦 (0/0/0) 出发，宽度优先遍历；
/// - 与兴趣矩形不相交的子树整支剪掉；
/// - 停在叶子条件 = 已达 maxLevel 或 该瓦 sse ≤ 阈值（sse = 几何误差代理
///   (geometricErrorScale × 瓦 mercator 尺寸) 在相机到瓦中心 ECEF 距离下的屏幕像素）；
/// - 未停则细化四个子瓦。
/// 说明：几何误差代理（scale × 瓦尺寸）后续会被真实 DEM 源元数据误差表替换
/// （量化网格的 geometricError）；本类先钉住"选择机制"本身。
class TerrainLodSelector {
public:
    TerrainLodResult selectTiles(const WebMercatorTileScheme& scheme,
                                 const Vec3& cameraPositionEcef,
                                 const Rectangle& interestRadians,
                                 const TerrainLodConfig& config = TerrainLodConfig()) const;

    /// 两个（弧度，无跨缝）矩形是否相交。
    static bool rectanglesIntersect(const Rectangle& a, const Rectangle& b);
};

} // namespace earth_engine
