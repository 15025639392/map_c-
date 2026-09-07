#include "earth_engine/tiling/TerrainLodSelector.h"

#include <algorithm>
#include <deque>

#include "earth_engine/core/geodesy/QuadtreeGeometricError.h"

namespace earth_engine {

bool TerrainLodSelector::rectanglesIntersect(const Rectangle& a, const Rectangle& b) {
    // 线性区间相交（前提：无跨反经线矩形）。
    if (a.east() <= b.west() || b.east() <= a.west()) {
        return false;
    }
    if (a.north() <= b.south() || b.north() <= a.south()) {
        return false;
    }
    return true;
}

namespace {

/// 相机到瓦覆盖区域（经纬矩形）的最近地表点 ECEF 距离。
/// 大瓦可能覆盖相机正下方：取"瓦内离相机最近的地表点"而非瓦中心，
/// 否则根瓦/大瓦中心在地球另一侧会把距离算成半个行星（SSE 失真）。
double distanceToTileRegion(const Ellipsoid& ellipsoid, const Vec3& cameraEcef,
                            const Rectangle& tileRectRadians) {
    const Cartographic cam = ellipsoid.cartesianToCartographic(cameraEcef);
    const double lon = std::clamp(cam.longitude(), tileRectRadians.west(), tileRectRadians.east());
    const double lat = std::clamp(cam.latitude(), tileRectRadians.south(), tileRectRadians.north());
    const Vec3 nearest = ellipsoid.cartographicToCartesian(Cartographic(lon, lat, 0.0));
    return cameraEcef.distanceTo(nearest);
}

} // namespace

TerrainLodResult TerrainLodSelector::selectTiles(const WebMercatorTileScheme& scheme,
                                                 const Vec3& cameraPositionEcef,
                                                 const Rectangle& interestRadians,
                                                 const TerrainLodConfig& config) const {
    TerrainLodResult result;
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();

    // 根瓦矩形（世界）与兴趣矩形不相交 → 空结果。
    const Rectangle world = scheme.tileRectangleRadians(TileKey(0, 0, 0));
    if (!rectanglesIntersect(world, interestRadians)) {
        return result;
    }

    std::deque<TileKey> frontier{TileKey(0, 0, 0)};
    while (!frontier.empty()) {
        const TileKey key = frontier.front();
        frontier.pop_front();

        const Rectangle rect = scheme.tileRectangleRadians(key);
        if (!rectanglesIntersect(rect, interestRadians)) {
            continue; // 兴趣矩形外，剪枝
        }

        const int z = key.z();
        const bool atMaxLevel = z >= config.maxLevel;
        // 几何误差代理：瓦内地形相对椭球的位移 ≈ scale × 瓦尺寸。
        const double geometricError = scheme.tileSizeMeters(z).x() * config.geometricErrorScale;
        const double distance = distanceToTileRegion(ellipsoid, cameraPositionEcef, rect);
        const double sse = QuadtreeGeometricError::screenSpaceError(
            geometricError, distance, config.viewportHeightPx, config.fovRadians);

        const bool shouldStop = atMaxLevel || sse <= config.maxScreenSpaceErrorPx;
        if (shouldStop) {
            result.tiles.push_back(key);
            continue;
        }
        for (const TileKey& child : key.children()) {
            frontier.push_back(child);
        }
    }
    return result;
}

} // namespace earth_engine
