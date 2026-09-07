#include <gtest/gtest.h>

#include <vector>

#include "earth_engine/core/geodesy/HeightDatumCorrector.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

TEST(HeightDatumCorrector, IdentityReturnsZero) {
    const IdentityHeightDatumCorrector id;
    EXPECT_EQ(id.undulationMeters(Cartographic::fromDegrees(106.5, 29.7)), 0.0);
    EXPECT_EQ(id.undulationMeters(Cartographic::fromDegrees(-73.0, 40.0)), 0.0);
}

TEST(HeightDatumCorrector, GridCornersExact) {
    // 2×2 网格：west=105,south=28,cell=1 → 角值 10/20/30/40（row: lat, col: lon）。
    const double grid[4] = {10.0, 20.0, 30.0, 40.0};
    const GridHeightDatumCorrector g(105.0, 28.0, 1.0, 2, 2, grid);
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(105.0, 28.0)), 10.0, 1e-9);
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(106.0, 28.0)), 20.0, 1e-9);
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(105.0, 29.0)), 30.0, 1e-9);
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(106.0, 29.0)), 40.0, 1e-9);
}

TEST(HeightDatumCorrector, BilinearInterpolation) {
    const double grid[4] = {10.0, 20.0, 30.0, 40.0};
    const GridHeightDatumCorrector g(105.0, 28.0, 1.0, 2, 2, grid);
    // 中心 (105.5, 28.5) = 平均 25。
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(105.5, 28.5)), 25.0, 1e-9);
    // (105.5, 28.0) 南北下沿 → 行内中点 15。
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(105.5, 28.0)), 15.0, 1e-9);
}

TEST(HeightDatumCorrector, OutOfRangeClampsToEdge) {
    const double grid[4] = {10.0, 20.0, 30.0, 40.0};
    const GridHeightDatumCorrector g(105.0, 28.0, 1.0, 2, 2, grid);
    // 西南外 → 钳到 (105,28) = 10。
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(100.0, 20.0)), 10.0, 1e-9);
    // 东北外 → 钳到 (106,29) = 40。
    EXPECT_NEAR(g.undulationMeters(Cartographic::fromDegrees(120.0, 40.0)), 40.0, 1e-9);
}

TEST(HeightDatumCorrector, EmptyGridReturnsZero) {
    const GridHeightDatumCorrector g(0.0, 0.0, 1.0, 0, 0, nullptr);
    EXPECT_TRUE(g.empty());
    EXPECT_EQ(g.undulationMeters(Cartographic::fromDegrees(10.0, 10.0)), 0.0);
}
