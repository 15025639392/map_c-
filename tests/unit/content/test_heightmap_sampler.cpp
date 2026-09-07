#include <gtest/gtest.h>

#include <vector>

#include "earth_engine/content/HeightmapSampler.h"

using namespace earth_engine;

namespace {

// 2×2 网格：row0(北) = {10, 20}；row1 = {30, 40}。
const double kGrid2x2[4] = {10.0, 20.0, 30.0, 40.0};

} // namespace

TEST(HeightmapSampler, GridAccess) {
    const HeightmapSampler s(kGrid2x2, 2, 2);
    EXPECT_EQ(s.width(), 2);
    EXPECT_EQ(s.height(), 2);
    EXPECT_EQ(s.sampleNearest(0.0, 0.0), 10.0);
    EXPECT_EQ(s.sampleNearest(1.0, 1.0), 40.0);
    EXPECT_EQ(s.sampleNearest(0.6, 0.6), 40.0); // 四舍五入到 (1,1)
    EXPECT_EQ(s.sampleNearest(0.4, 0.4), 10.0); // 四舍五入到 (0,0)
}

TEST(HeightmapSampler, BilinearKnownValues) {
    const HeightmapSampler s(kGrid2x2, 2, 2);
    EXPECT_EQ(s.sampleBilinear(0.0, 0.0), 10.0);
    EXPECT_EQ(s.sampleBilinear(1.0, 1.0), 40.0);
    // 像素对中点。
    EXPECT_NEAR(s.sampleBilinear(0.5, 0.0), 15.0, 1.0e-12); // 北行东西中点
    EXPECT_NEAR(s.sampleBilinear(1.0, 0.5), 30.0, 1.0e-12); // 东列南北中点
    EXPECT_NEAR(s.sampleBilinear(0.0, 0.5), 20.0, 1.0e-12); // 西列南北中点
    EXPECT_NEAR(s.sampleBilinear(0.5, 1.0), 35.0, 1.0e-12); // 南行东西中点
    // 中心 = 四格平均。
    EXPECT_NEAR(s.sampleBilinear(0.5, 0.5), 25.0, 1.0e-12);
    // 四分之一处。
    EXPECT_NEAR(s.sampleBilinear(0.25, 0.25), 17.5, 1.0e-12);
}

TEST(HeightmapSampler, ClampToEdge) {
    const HeightmapSampler s(kGrid2x2, 2, 2);
    // 越界坐标贴边：col<0 → 西列插值。
    EXPECT_NEAR(s.sampleBilinear(-3.0, 0.5), 20.0, 1.0e-12);
    // 完全越界 → 角落值。
    EXPECT_NEAR(s.sampleBilinear(99.0, 99.0), 40.0, 1.0e-12);
    EXPECT_NEAR(s.sampleBilinear(-5.0, -5.0), 10.0, 1.0e-12);
    // 行越界但列在中间：南行插值（r=1 行内）。
    EXPECT_NEAR(s.sampleBilinear(0.2, 5.0), 32.0, 1.0e-12);
    // nearest 同样钳制（各轴独立贴边）。
    EXPECT_EQ(s.sampleNearest(-99.0, -99.0), 10.0); // (0,0)
    EXPECT_EQ(s.sampleNearest(99.0, -99.0), 20.0);  // (1,0)
    EXPECT_EQ(s.sampleNearest(-99.0, 99.0), 30.0);  // (0,1)
    EXPECT_EQ(s.sampleNearest(99.0, 99.0), 40.0);   // (1,1)
}

TEST(HeightmapSampler, ReproducesLinearField) {
    // 双线性插值精确重建线性场：8×8 网格 h = 100 + 3·col + 5·row。
    constexpr int kW = 8;
    constexpr int kH = 8;
    std::vector<double> grid(static_cast<size_t>(kW * kH));
    for (int row = 0; row < kH; ++row) {
        for (int col = 0; col < kW; ++col) {
            grid[static_cast<size_t>(row * kW + col)] = 100.0 + 3.0 * col + 5.0 * row;
        }
    }
    const HeightmapSampler s(grid.data(), kW, kH);
    for (const double col : {0.3, 1.0, 4.7, 7.0}) {
        for (const double row : {0.1, 3.5, 6.9}) {
            const double analytic = 100.0 + 3.0 * col + 5.0 * row;
            EXPECT_NEAR(s.sampleBilinear(col, row), analytic, 1.0e-9);
        }
    }
}

TEST(HeightmapSampler, DegenerateSingleCell) {
    const double single[1] = {42.0};
    const HeightmapSampler s(single, 1, 1);
    EXPECT_EQ(s.sampleBilinear(0.0, 0.0), 42.0);
    EXPECT_EQ(s.sampleBilinear(3.0, -2.0), 42.0); // 越界钳到唯一格点
    EXPECT_EQ(s.sampleNearest(99.0, 99.0), 42.0);
}
