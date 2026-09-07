#pragma once

#include <optional>

#include "TileKey.h"
#include "../core/geodesy/Projection.h"
#include "../core/math/Rectangle.h"
#include "../core/math/Vec2.h"

namespace earth_engine {

/// Web Mercator 瓦片网格（XYZ/TMS 正方形世界）：
/// - 世界 = [-πa, πa]²（投影米）；
/// - z 层 2^z × 2^z 瓦片，每瓦在**投影空间**是等宽等高的方块
///   （经纬矩形随纬度变化，但瓦片边界在投影米下是直线）。
/// - 键序 = XYZ 顶层原点：y=0 是最北一行，y 向南递增（与 OSM/NASA
///   Terrain-RGB 的 URL 模板一致）；TMS 需要时在接入层做 y 翻转。
/// 高度图/影像主流 scheme（NASA Terrain-RGB、XYZ 影像）都基于它。
class WebMercatorTileScheme {
public:
    explicit WebMercatorTileScheme(const Ellipsoid& ellipsoid = Ellipsoid::WGS84());

    /// 世界半宽（米）= π·a。世界范围 [-half, half]²。
    double worldHalfExtentMeters() const { return worldHalfExtent_; }

    /// z 层每边瓦片数（= TileKey::tilesPerSide）。
    static int tilesPerSide(int z) { return TileKey::tilesPerSide(z); }

    /// 瓦片西南角（投影米；几何底角，与 y 方向约定无关）。
    Vec2 tileOriginMeters(const TileKey& key) const;
    /// 瓦片尺寸（投影米；正方形，同层恒定）。
    Vec2 tileSizeMeters(int z) const;

    /// 瓦片覆盖的经纬矩形（弧度）。
    Rectangle tileRectangleRadians(const TileKey& key) const;

    /// 投影米坐标 → 所在 z 层瓦片键。坐标在世界外（含恰在 +half 边界）返回 nullopt。
    std::optional<TileKey> tileKeyForMeters(const Vec2& positionMeters, int z) const;
    /// 经纬 → 所在 z 层瓦片键（高度被忽略）。
    std::optional<TileKey> tileKeyForCartographic(const Cartographic& cartographic, int z) const;

    /// 中心点（投影米）。
    Vec2 tileCenterMeters(const TileKey& key) const;

private:
    Ellipsoid ellipsoid_;
    WebMercatorProjection projection_;
    double worldHalfExtent_;
};

} // namespace earth_engine
