#include <gtest/gtest.h>

#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/core/math/Vec3.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-12;

} // namespace

TEST(Vec3, BasicOps) {
    const Vec3 a(1.0, 2.0, 3.0);
    const Vec3 b(4.0, -1.0, 2.0);
    EXPECT_EQ(a + b, Vec3(5.0, 1.0, 5.0));
    EXPECT_EQ(a - b, Vec3(-3.0, 3.0, 1.0));
    EXPECT_EQ(a * 2.0, Vec3(2.0, 4.0, 6.0));
    EXPECT_EQ(2.0 * a, Vec3(2.0, 4.0, 6.0));
    EXPECT_EQ(-a, Vec3(-1.0, -2.0, -3.0));
    EXPECT_EQ(a / 2.0, Vec3(0.5, 1.0, 1.5));
    EXPECT_EQ(Vec3::zero(), Vec3(0.0, 0.0, 0.0));
    EXPECT_EQ(a.x(), 1.0);
    EXPECT_EQ(a[2], 3.0);
}

TEST(Vec3, DotCrossMagnitude) {
    const Vec3 a(1.0, 0.0, 0.0);
    const Vec3 b(0.0, 1.0, 0.0);
    EXPECT_EQ(a.dot(b), 0.0);
    EXPECT_EQ(a.cross(b), Vec3(0.0, 0.0, 1.0));
    EXPECT_EQ(b.cross(a), Vec3(0.0, 0.0, -1.0));
    EXPECT_EQ(a.magnitudeSquared(), 1.0);
    EXPECT_EQ(a.magnitude(), 1.0);
    EXPECT_EQ(Vec3(3.0, 4.0, 0.0).magnitude(), 5.0);

    // 叉积正交性与面积公式。
    const Vec3 u(2.0, 3.0, 5.0);
    const Vec3 v(-1.0, 7.0, 0.5);
    const Vec3 c = u.cross(v);
    EXPECT_NEAR(u.dot(c), 0.0, kEps);
    EXPECT_NEAR(v.dot(c), 0.0, kEps);
    EXPECT_NEAR(c.magnitude(), u.magnitude() * v.magnitude() * std::sin(u.angleBetween(v)), 1.0e-9);
}

TEST(Vec3, NormalizeAndEdges) {
    const Vec3 n = Vec3(2.0, 3.0, 6.0).normalized();
    EXPECT_NEAR(n.magnitude(), 1.0, kEps);
    EXPECT_NEAR(n.x(), 2.0 / 7.0, kEps);
    // 零向量归一化不产生 NaN。
    const Vec3 z = Vec3::zero().normalized();
    EXPECT_EQ(z, Vec3::zero());
    EXPECT_TRUE(std::isfinite(z.x()));
}

TEST(Vec3, DistanceAndAngle) {
    EXPECT_EQ(Vec3(1.0, 0.0, 0.0).distanceTo(Vec3(4.0, 0.0, 0.0)), 3.0);
    EXPECT_EQ(Vec3(1.0, 0.0, 0.0).distanceSquaredTo(Vec3(4.0, 0.0, 0.0)), 9.0);
    EXPECT_NEAR(Vec3::unitX().angleBetween(Vec3::unitY()), kPiOverTwo, kEps);
    EXPECT_NEAR(Vec3::unitX().angleBetween(Vec3::unitX()), 0.0, kEps);
    EXPECT_NEAR(Vec3::unitX().angleBetween(-Vec3::unitX()), kPi, kEps);
    // 余弦越界钳制（数值噪声下 acos 不 NaN）。
    const Vec3 a(1.0 + 1.0e-16, 0.0, 0.0);
    const Vec3 b(1.0, 0.0, 0.0);
    EXPECT_TRUE(std::isfinite(a.angleBetween(b)));
}

TEST(Vec3, LerpMidpointMinMax) {
    EXPECT_EQ(Vec3::lerp(Vec3(0.0, 0.0, 0.0), Vec3(2.0, 4.0, 6.0), 0.5),
              Vec3(1.0, 2.0, 3.0));
    EXPECT_EQ(Vec3::lerp(Vec3(0.0, 0.0, 0.0), Vec3(2.0, 4.0, 6.0), 1.0),
              Vec3(2.0, 4.0, 6.0));
    EXPECT_EQ(Vec3::midpoint(Vec3(1.0, 2.0, 3.0), Vec3(3.0, 4.0, 5.0)),
              Vec3(2.0, 3.0, 4.0));
    EXPECT_EQ(Vec3::componentwiseMin(Vec3(1.0, 5.0, 2.0), Vec3(3.0, 2.0, 4.0)),
              Vec3(1.0, 2.0, 2.0));
    EXPECT_EQ(Vec3::componentwiseMax(Vec3(1.0, 5.0, 2.0), Vec3(3.0, 2.0, 4.0)),
              Vec3(3.0, 5.0, 4.0));
}

TEST(Vec3, Equality) {
    EXPECT_EQ(Vec3(1.0, 2.0, 3.0), Vec3(1.0, 2.0, 3.0));
    EXPECT_NE(Vec3(1.0, 2.0, 3.0), Vec3(1.0, 2.0, 4.0));
    EXPECT_TRUE(Vec3(1.0, 2.0, 3.0).equalsEpsilon(Vec3(1.0 + 1.0e-10, 2.0, 3.0), 1.0e-6));
    EXPECT_FALSE(Vec3(1.0, 2.0, 3.0).equalsEpsilon(Vec3(1.0 + 1.0e-3, 2.0, 3.0), 1.0e-6));
}
