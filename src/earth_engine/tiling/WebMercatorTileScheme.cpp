#include "earth_engine/tiling/WebMercatorTileScheme.h"

#include <cmath>

namespace earth_engine {

WebMercatorTileScheme::WebMercatorTileScheme(const Ellipsoid& ellipsoid)
    : ellipsoid_(ellipsoid),
      projection_(ellipsoid),
      worldHalfExtent_(kPi * ellipsoid.maximumRadius()) {}

Vec2 WebMercatorTileScheme::tileOriginMeters(const TileKey& key) const {
    const Vec2 size = tileSizeMeters(key.z());
    // XYZ 顶层原点：y=0 行贴世界北边（mercator y = +half）。
    const double northY = worldHalfExtent_ - (key.y() + 1) * size.y();
    return Vec2(-worldHalfExtent_ + key.x() * size.x(), northY);
}

Vec2 WebMercatorTileScheme::tileSizeMeters(int z) const {
    const int n = tilesPerSide(z);
    if (n <= 0) {
        return Vec2::zero();
    }
    const double cell = (2.0 * worldHalfExtent_) / static_cast<double>(n);
    return Vec2(cell, cell);
}

Rectangle WebMercatorTileScheme::tileRectangleRadians(const TileKey& key) const {
    const Vec2 sw = tileOriginMeters(key);
    const Vec2 size = tileSizeMeters(key.z());
    const Cartographic swCarto = projection_.unproject(sw);
    const Cartographic neCarto = projection_.unproject(sw + size);
    return Rectangle(swCarto.longitude(), swCarto.latitude(),
                     neCarto.longitude(), neCarto.latitude());
}

std::optional<TileKey> WebMercatorTileScheme::tileKeyForMeters(const Vec2& positionMeters,
                                                               int z) const {
    const int n = tilesPerSide(z);
    if (n <= 0) {
        return std::nullopt;
    }
    // 世界 [−half, half)²（西/南边界闭、东/北边界开在索引意义下）：
    // x：px ∈ [−half, half)；y（顶行原点）：py ∈ (−half, half]。
    const double span = 2.0 * worldHalfExtent_;
    const double px = positionMeters.x() + worldHalfExtent_;
    const double topDistance = worldHalfExtent_ - positionMeters.y();
    if (!(px >= 0.0 && px < span)) {
        return std::nullopt;
    }
    if (!(topDistance >= 0.0 && topDistance < span)) {
        return std::nullopt;
    }
    const double cell = span / static_cast<double>(n);
    const int x = static_cast<int>(px / cell);
    const int y = static_cast<int>(topDistance / cell);
    if (x < 0 || x >= n || y < 0 || y >= n) {
        return std::nullopt; // 浮点边界防御
    }
    return TileKey(z, x, y);
}

std::optional<TileKey> WebMercatorTileScheme::tileKeyForCartographic(
    const Cartographic& cartographic, int z) const {
    // Web Mercator 世界只覆盖 ±85.05112878°：纬度越界 = 世界外（不静默钳制）。
    const double maxLat = WebMercatorProjection::maximumLatitudeRadians();
    if (!(cartographic.latitude() >= -maxLat && cartographic.latitude() <= maxLat)) {
        return std::nullopt;
    }
    return tileKeyForMeters(projection_.project(cartographic), z);
}

Vec2 WebMercatorTileScheme::tileCenterMeters(const TileKey& key) const {
    const Vec2 origin = tileOriginMeters(key);
    const Vec2 size = tileSizeMeters(key.z());
    return origin + size * 0.5;
}

Vec2 WebMercatorTileScheme::projectToMeters(const Cartographic& cartographic) const {
    return projection_.project(cartographic);
}

Cartographic WebMercatorTileScheme::unprojectMeters(const Vec2& positionMeters) const {
    return projection_.unproject(positionMeters);
}

} // namespace earth_engine
