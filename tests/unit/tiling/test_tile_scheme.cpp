#include <gtest/gtest.h>

#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-9;
constexpr double kMeterEps = 1.0e-6;

double worldHalf() {
    return kPi * Ellipsoid::WGS84().maximumRadius(); // ≈ 20037508.342789244
}

const double kMaxLat = WebMercatorProjection::maximumLatitudeRadians();

} // namespace

TEST(WebMercatorTileScheme, WorldExtent) {
    const WebMercatorTileScheme scheme;
    EXPECT_NEAR(scheme.worldHalfExtentMeters(), worldHalf(), kMeterEps);
    // 世界正方形：宽 = 2·half。
    EXPECT_NEAR(scheme.tileSizeMeters(0).x(), 2.0 * worldHalf(), kMeterEps);
    EXPECT_NEAR(scheme.tileSizeMeters(1).x(), worldHalf(), kMeterEps);
    EXPECT_NEAR(scheme.tileSizeMeters(2).x(), worldHalf() / 2.0, kMeterEps);
    EXPECT_EQ(scheme.tileSizeMeters(2).x(), scheme.tileSizeMeters(2).y());
    EXPECT_EQ(scheme.tileSizeMeters(-1), Vec2::zero());
}

TEST(WebMercatorTileScheme, RootTileRectangle) {
    const WebMercatorTileScheme scheme;
    const Rectangle world = scheme.tileRectangleRadians(TileKey(0, 0, 0));
    EXPECT_NEAR(world.west(), -kPi, kEps);
    EXPECT_NEAR(world.east(), kPi, kEps);
    EXPECT_NEAR(world.south(), -kMaxLat, 1.0e-12);
    EXPECT_NEAR(world.north(), kMaxLat, 1.0e-12);
}

TEST(WebMercatorTileScheme, LevelOneTileRectangles) {
    const WebMercatorTileScheme scheme;
    // (1,0,0)：西半球、北半行（y=0 顶行）→ lat ∈ [0, maxLat]。
    const Rectangle nw = scheme.tileRectangleRadians(TileKey(1, 0, 0));
    EXPECT_NEAR(nw.west(), -kPi, kEps);
    EXPECT_NEAR(nw.east(), 0.0, 1.0e-12);
    EXPECT_NEAR(nw.north(), kMaxLat, 1.0e-12);
    EXPECT_NEAR(nw.south(), 0.0, 1.0e-12);

    // (1,1,1)：东半球、南半行 → lat ∈ [-maxLat, 0]。
    const Rectangle se = scheme.tileRectangleRadians(TileKey(1, 1, 1));
    EXPECT_NEAR(se.west(), 0.0, 1.0e-12);
    EXPECT_NEAR(se.east(), kPi, kEps);
    EXPECT_NEAR(se.south(), -kMaxLat, 1.0e-12);
    EXPECT_NEAR(se.north(), 0.0, 1.0e-12);
}

TEST(WebMercatorTileScheme, ChildrenTileBoundariesAlign) {
    const WebMercatorTileScheme scheme;
    const auto children = TileKey(2, 1, 1).children(); // z3 的四块
    for (const TileKey& c : children) {
        EXPECT_EQ(c.parent().value(), TileKey(2, 1, 1));
    }
    // 东西邻居共边（经度相等）。
    EXPECT_NEAR(scheme.tileRectangleRadians(TileKey(3, 2, 2)).east(),
                scheme.tileRectangleRadians(TileKey(3, 3, 2)).west(), 1.0e-12);
    // 南北邻居共边（纬度相等；南边的 north == 北边的 south）。
    EXPECT_NEAR(scheme.tileRectangleRadians(TileKey(3, 2, 3)).north(),
                scheme.tileRectangleRadians(TileKey(3, 2, 2)).south(), 1.0e-12);
    // 四子合并 = 父矩形（逐条边对比）。
    const Rectangle parent = scheme.tileRectangleRadians(TileKey(2, 1, 1));
    EXPECT_NEAR(scheme.tileRectangleRadians(TileKey(3, 2, 2)).west(), parent.west(), 1.0e-12);
    EXPECT_NEAR(scheme.tileRectangleRadians(TileKey(3, 3, 2)).east(), parent.east(), 1.0e-12);
    EXPECT_NEAR(scheme.tileRectangleRadians(TileKey(3, 2, 2)).north(), parent.north(), 1.0e-12);
    EXPECT_NEAR(scheme.tileRectangleRadians(TileKey(3, 2, 3)).south(), parent.south(), 1.0e-12);
}

TEST(WebMercatorTileScheme, KeyForCartographic) {
    const WebMercatorTileScheme scheme;
    // 重庆：106.5E 29.7N（北半球东半球）。
    const Cartographic c = Cartographic::fromDegrees(106.5, 29.7);
    const auto key = scheme.tileKeyForCartographic(c, 10);
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->z(), 10);
    // 返回键的矩形必须包含该点。
    const Rectangle r = scheme.tileRectangleRadians(key.value());
    EXPECT_TRUE(r.contains(c.longitude(), c.latitude()));

    // 纬度超出 Web Mercator 上限（±85.051…°）= 世界外 → 无键。
    EXPECT_FALSE(scheme.tileKeyForCartographic(Cartographic::fromDegrees(0.0, 90.0), 5).has_value());
    EXPECT_FALSE(scheme.tileKeyForCartographic(Cartographic::fromDegrees(0.0, -90.0), 5).has_value());
    // 经度恰在 180（东边界开）→ 无键。
    EXPECT_FALSE(scheme.tileKeyForCartographic(Cartographic::fromDegrees(180.0, 0.0), 5).has_value());
    // 179.9E 正常。
    EXPECT_TRUE(scheme.tileKeyForCartographic(Cartographic::fromDegrees(179.9, 0.0), 5).has_value());
}

TEST(WebMercatorTileScheme, KeyForMetersRoundTrip) {
    const WebMercatorTileScheme scheme;
    // 任意层瓦片中心 → 同层键应回到自身。
    for (const TileKey& k : {TileKey(3, 5, 2), TileKey(5, 17, 13), TileKey(8, 130, 90)}) {
        const Vec2 center = scheme.tileCenterMeters(k);
        const auto back = scheme.tileKeyForMeters(center, k.z());
        ASSERT_TRUE(back.has_value());
        EXPECT_EQ(back.value(), k) << k.toString();
    }
    // 世界外（y = -half - 1）→ nullopt。
    EXPECT_FALSE(scheme.tileKeyForMeters(Vec2(0.0, -worldHalf() - 1.0), 4).has_value());
    // 世界北边 y = +half → y=0 行（闭）。
    const auto northEdge = scheme.tileKeyForMeters(Vec2(0.0, worldHalf()), 4);
    ASSERT_TRUE(northEdge.has_value());
    EXPECT_EQ(northEdge->y(), 0);
}

TEST(WebMercatorTileScheme, TopLeftOriginConvention) {
    const WebMercatorTileScheme scheme;
    const double cell = scheme.tileSizeMeters(3).x(); // half/4
    // y=0 是最北行：行内北边 = 世界北边（= SW 原点 + cell）。
    const Vec2 row0Origin = scheme.tileOriginMeters(TileKey(3, 0, 0));
    EXPECT_NEAR(row0Origin.y() + cell, worldHalf(), kMeterEps);
    // 行 y=1 的北边 = 行 y=0 的南边（相邻行无缝）。
    const Vec2 row1Origin = scheme.tileOriginMeters(TileKey(3, 0, 1));
    EXPECT_NEAR(row1Origin.y() + cell, row0Origin.y(), kMeterEps);
    // 最南行（y=7）南边 = 世界南边。
    const Vec2 lastRowOrigin = scheme.tileOriginMeters(TileKey(3, 0, 7));
    EXPECT_NEAR(lastRowOrigin.y(), -worldHalf(), kMeterEps);
    // 矩形边与行几何一致：y=0 行 rect.north = maxLat。
    const Rectangle r = scheme.tileRectangleRadians(TileKey(3, 0, 0));
    EXPECT_NEAR(r.north(), kMaxLat, 1.0e-12);
}
