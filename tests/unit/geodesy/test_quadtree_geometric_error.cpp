#include <gtest/gtest.h>

#include <cmath>

#include "earth_engine/core/geodesy/QuadtreeGeometricError.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;
using namespace QuadtreeGeometricError;

namespace {

constexpr double kRel = 1.0e-9;

} // namespace

TEST(QuadtreeGeometricError, KnownGeometry) {
    // fov=90°，视口 1000px：米/像素 = 2d·tan45°/1000 = d/500。
    // e=1000m, d=1000m → 500 px。
    const double sse = screenSpaceError(1000.0, 1000.0, 1000.0, kPiOverTwo);
    EXPECT_NEAR(sse, 500.0, 1.0e-6);
}

TEST(QuadtreeGeometricError, MonotonicScaling) {
    const double base = screenSpaceError(100.0, 1000.0, 800.0, degreesToRadians(60.0));
    // 距离翻倍 → sse 减半。
    EXPECT_NEAR(screenSpaceError(100.0, 2000.0, 800.0, degreesToRadians(60.0)), base * 0.5, kRel * base);
    // 误差翻倍 → sse 翻倍。
    EXPECT_NEAR(screenSpaceError(200.0, 1000.0, 800.0, degreesToRadians(60.0)), base * 2.0, kRel * base);
    // 视口翻倍 → sse 翻倍。
    EXPECT_NEAR(screenSpaceError(100.0, 1000.0, 1600.0, degreesToRadians(60.0)), base * 2.0, kRel * base);
    // fov 变大 → 米/像素变大 → sse 变小。
    EXPECT_LT(screenSpaceError(100.0, 1000.0, 800.0, degreesToRadians(90.0)), base);
}

TEST(QuadtreeGeometricError, DegenerateInputs) {
    // 零误差 = 零像素，无论距离。
    EXPECT_EQ(screenSpaceError(0.0, 10.0, 800.0, degreesToRadians(60.0)), 0.0);
    // 无效距离/视场 → +inf（保守方向）。
    EXPECT_TRUE(std::isinf(screenSpaceError(100.0, 0.0, 800.0, degreesToRadians(60.0))));
    EXPECT_TRUE(std::isinf(screenSpaceError(100.0, -1.0, 800.0, degreesToRadians(60.0))));
    EXPECT_TRUE(std::isinf(screenSpaceError(-1.0, 1000.0, 800.0, degreesToRadians(60.0))));
    EXPECT_TRUE(std::isinf(screenSpaceError(100.0, 1000.0, 800.0, 0.0)));
    // 无视口 → 无像素可消费 → 0。
    EXPECT_EQ(screenSpaceError(100.0, 1000.0, 0.0, degreesToRadians(60.0)), 0.0);
}

TEST(QuadtreeGeometricError, ShouldRefineBoundary) {
    // 构造一个刚好 ~50px 的场景。
    const double fov = kPiOverTwo;
    const double viewport = 1000.0;
    // e=1000,d=1000 → 500px（见 KnownGeometry）。
    EXPECT_TRUE(shouldRefine(1000.0, 1000.0, viewport, fov, 16.0));
    // 阈值高于 sse 则不细化。
    EXPECT_FALSE(shouldRefine(1000.0, 1000.0, viewport, fov, 1000.0));
    // 零误差永不细化。
    EXPECT_FALSE(shouldRefine(0.0, 1000.0, viewport, fov, 16.0));
    // 无效参数保守细化（误差无效除外：<0 无效但 <0 的误差没意义 → 依实现为 refine true？
    // 语义：几何误差为负 = 无效 → 无细化动机，返回 false）。
    EXPECT_FALSE(shouldRefine(-1.0, 1000.0, viewport, fov, 16.0));
    EXPECT_TRUE(shouldRefine(1000.0, 0.0, viewport, fov, 16.0));
}

TEST(QuadtreeGeometricError, RefinementAtTypicalTerrainNumbers) {
    // 地形常见量级冒烟：误差 30m，距离 10km，视口 1080，fov 60°
    // → 米/像素 = 2·10000·tan30°/1080 ≈ 10.7 → sse ≈ 2.8px < 16px 不细化。
    const double sse = screenSpaceError(30.0, 10000.0, 1080.0, degreesToRadians(60.0));
    EXPECT_GT(sse, 2.0);
    EXPECT_LT(sse, 4.0);
    EXPECT_FALSE(shouldRefine(30.0, 10000.0, 1080.0, degreesToRadians(60.0), 16.0));
    // 距离 3km 时 sse ≈ 9.3px 仍不细化；1km 时 ≈ 28px 细化。
    EXPECT_FALSE(shouldRefine(30.0, 3000.0, 1080.0, degreesToRadians(60.0), 16.0));
    EXPECT_TRUE(shouldRefine(30.0, 1000.0, 1080.0, degreesToRadians(60.0), 16.0));
}
