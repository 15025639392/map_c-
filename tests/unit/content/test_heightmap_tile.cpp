#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

// 合成 5×5 网格：h(col,row) = 100·col + row。row0 = 北。
std::vector<double> makeGrid5x5() {
    std::vector<double> grid(25);
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            grid[static_cast<size_t>(row * 5 + col)] = 100.0 * col + row;
        }
    }
    return grid;
}

// 取覆盖重庆附近的一瓦（z=9）。
TileKey chongqingTile(const WebMercatorTileScheme& scheme) {
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    EXPECT_TRUE(key.has_value());
    return key.value();
}

} // namespace

TEST(HeightmapTile, CoverageAndContains) {
    const WebMercatorTileScheme scheme;
    const std::vector<double> grid = makeGrid5x5();
    const TileKey key = chongqingTile(scheme);
    const HeightmapTile tile(scheme, key, grid.data(), 5, 5);

    const Rectangle cov = tile.coverageRadians();
    EXPECT_NEAR(cov.west(), scheme.tileRectangleRadians(key).west(), 1.0e-12);
    // 重庆点在瓦内。
    EXPECT_TRUE(tile.contains(Cartographic::fromDegrees(106.5, 29.7)));
    // 纬度越界点不在瓦内。
    EXPECT_FALSE(tile.contains(Cartographic::fromDegrees(106.5, 90.0)));
    // 明显在瓦西侧之外的点（经度差 > 1 度）。
    const Cartographic farWest = Cartographic::fromDegrees(
        radiansToDegrees(cov.west()) - 1.0, radiansToDegrees(cov.north()));
    EXPECT_FALSE(tile.contains(farWest));
}

TEST(HeightmapTile, CornerAndGridPointSampling) {
    const WebMercatorTileScheme scheme;
    const std::vector<double> grid = makeGrid5x5();
    const HeightmapTile tile(scheme, chongqingTile(scheme), grid.data(), 5, 5);

    // 四角 = 网格角值（网格点落在瓦片边界上）。
    const Cartographic nw = tile.pixelToCartographic(0.0, 0.0);      // 北-西
    const Cartographic ne = tile.pixelToCartographic(4.0, 0.0);      // 北-东
    const Cartographic sw = tile.pixelToCartographic(0.0, 4.0);      // 南-西
    const Cartographic se = tile.pixelToCartographic(4.0, 4.0);      // 南-东
    EXPECT_NEAR(tile.sampleHeightAt(nw).value(), 0.0, 1.0e-9);
    EXPECT_NEAR(tile.sampleHeightAt(ne).value(), 400.0, 1.0e-9);
    EXPECT_NEAR(tile.sampleHeightAt(sw).value(), 4.0, 1.0e-9);
    EXPECT_NEAR(tile.sampleHeightAt(se).value(), 404.0, 1.0e-9);

    // 中央格点 (col=2,row=2) → 202。
    const Cartographic center = tile.pixelToCartographic(2.0, 2.0);
    EXPECT_NEAR(tile.sampleHeightAt(center).value(), 202.0, 1.0e-9);

    // 像素↔地理往返一致（容差按 mercator 米 ~1e-6 瓦片宽）。
    const auto back = tile.cartographicToPixel(center);
    ASSERT_TRUE(back.has_value());
    EXPECT_NEAR(back->x(), 2.0, 1.0e-9);
    EXPECT_NEAR(back->y(), 2.0, 1.0e-9);
}

TEST(HeightmapTile, InterpolationAlongRow) {
    const WebMercatorTileScheme scheme;
    const std::vector<double> grid = makeGrid5x5();
    const HeightmapTile tile(scheme, chongqingTile(scheme), grid.data(), 5, 5);

    // 北行 row0 上 col 1.5（两个格点之间）→ 高度线性 = 100·1.5 = 150。
    // pixelToCartographic 行/列在 mercator 米线性，双线性重建线性场 → 精确。
    const Cartographic mid = tile.pixelToCartographic(1.5, 0.0);
    EXPECT_NEAR(tile.sampleHeightAt(mid).value(), 150.0, 1.0e-6);

    // 跨两行中点 (0.5, 2.5)：col 0.5→50；row 2.5 → 2.5；h=50+2.5=52.5。
    const Cartographic mid2 = tile.pixelToCartographic(0.5, 2.5);
    EXPECT_NEAR(tile.sampleHeightAt(mid2).value(), 52.5, 1.0e-6);
}

TEST(HeightmapTile, OutsideReturnsNullopt) {
    const WebMercatorTileScheme scheme;
    const std::vector<double> grid = makeGrid5x5();
    const HeightmapTile tile(scheme, chongqingTile(scheme), grid.data(), 5, 5);

    const Rectangle cov = tile.coverageRadians();
    // 瓦片北边再往北一点（仍在 Web Mercator 范围内）。
    const Cartographic above = Cartographic(0.0, cov.north() + 1.0e-6, 0.0);
    EXPECT_FALSE(tile.sampleHeightAt(above).has_value());
    // 东边外。
    const Cartographic east = Cartographic(cov.east() + 1.0e-6, 0.0, 0.0);
    EXPECT_FALSE(tile.sampleHeightAt(east).has_value());
}

TEST(HeightmapTile, MinMaxHeight) {
    const WebMercatorTileScheme scheme;
    const std::vector<double> grid = makeGrid5x5();
    const HeightmapTile tile(scheme, chongqingTile(scheme), grid.data(), 5, 5);
    const auto [minH, maxH] = tile.minMaxHeight();
    EXPECT_NEAR(minH, 0.0, 1.0e-12);
    EXPECT_NEAR(maxH, 404.0, 1.0e-12);
}

TEST(HeightmapTile, DegenerateSingleColumn) {
    const WebMercatorTileScheme scheme;
    // 1×5 网格（w=1）：col 恒 0。
    const double narrow[5] = {10.0, 20.0, 30.0, 40.0, 50.0};
    const HeightmapTile tile(scheme, chongqingTile(scheme), narrow, 1, 5);
    const Cartographic north = tile.pixelToCartographic(0.0, 0.0);
    const Cartographic south = tile.pixelToCartographic(0.0, 4.0);
    EXPECT_NEAR(tile.sampleHeightAt(north).value(), 10.0, 1.0e-9);
    EXPECT_NEAR(tile.sampleHeightAt(south).value(), 50.0, 1.0e-9);
    // 中间行（row 2.0）→ 30。
    const Cartographic mid = tile.pixelToCartographic(0.0, 2.0);
    EXPECT_NEAR(tile.sampleHeightAt(mid).value(), 30.0, 1.0e-9);
}

TEST(HeightmapTile, NorthRowIsTileNorth) {
    // 语义钉死：row 0 贴瓦片北边（与 XYZ/高度图缓冲一致）。
    const WebMercatorTileScheme scheme;
    const std::vector<double> grid = makeGrid5x5();
    const HeightmapTile tile(scheme, chongqingTile(scheme), grid.data(), 5, 5);

    const Rectangle cov = tile.coverageRadians();
    // row0 全行的地理纬度 ≈ 瓦片北边。
    const Cartographic row0East = tile.pixelToCartographic(4.0, 0.0);
    EXPECT_NEAR(row0East.latitude(), cov.north(), 1.0e-9);
    // row4 全行纬度 ≈ 瓦片南边。
    const Cartographic row4West = tile.pixelToCartographic(0.0, 4.0);
    EXPECT_NEAR(row4West.latitude(), cov.south(), 1.0e-9);
    // col0 ≈ 西边、col4 ≈ 东边。
    EXPECT_NEAR(row4West.longitude(), cov.west(), 1.0e-9);
    EXPECT_NEAR(row0East.longitude(), cov.east(), 1.0e-9);
}
