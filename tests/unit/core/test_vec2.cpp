#include <gtest/gtest.h>

#include <cmath>

#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/core/math/Vec2.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-12;

} // namespace

TEST(Vec2, BasicOps) {
    const Vec2 a(1.0, 2.0);
    const Vec2 b(3.0, -4.0);
    EXPECT_EQ(a + b, Vec2(4.0, -2.0));
    EXPECT_EQ(a - b, Vec2(-2.0, 6.0));
    EXPECT_EQ(a * 2.0, Vec2(2.0, 4.0));
    EXPECT_EQ(b / 2.0, Vec2(1.5, -2.0));
    EXPECT_EQ(-a, Vec2(-1.0, -2.0));
}

TEST(Vec2, DotAndMagnitude) {
    EXPECT_EQ(Vec2(1.0, 0.0).dot(Vec2(0.0, 1.0)), 0.0);
    EXPECT_EQ(Vec2(3.0, 4.0).magnitudeSquared(), 25.0);
    EXPECT_EQ(Vec2(3.0, 4.0).magnitude(), 5.0);
    EXPECT_EQ(Vec2(0.0, 0.0).distanceTo(Vec2(1.0, 0.0)), 1.0);
}

TEST(Vec2, NormalizeEdges) {
    const Vec2 n = Vec2(3.0, 4.0).normalized();
    EXPECT_NEAR(n.magnitude(), 1.0, kEps);
    EXPECT_NEAR(n.x(), 0.6, kEps);
    // 零向量归一化不产生 NaN。
    const Vec2 z = Vec2::zero().normalized();
    EXPECT_EQ(z, Vec2::zero());
    EXPECT_TRUE(std::isfinite(z.x()));
}

TEST(Vec2, EqualityAndEpsilon) {
    EXPECT_EQ(Vec2(1.0, 2.0), Vec2(1.0, 2.0));
    EXPECT_NE(Vec2(1.0, 2.0), Vec2(1.0, 3.0));
    EXPECT_TRUE(Vec2(1.0, 2.0).equalsEpsilon(Vec2(1.0 + 1.0e-10, 2.0), 1.0e-6));
    EXPECT_FALSE(Vec2(1.0, 2.0).equalsEpsilon(Vec2(1.0 + 1.0e-3, 2.0), 1.0e-6));
}
