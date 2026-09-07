#include <gtest/gtest.h>

#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-12;

} // namespace

TEST(MathUtils, AngleConversions) {
    EXPECT_NEAR(degreesToRadians(180.0), kPi, kEps);
    EXPECT_NEAR(degreesToRadians(0.0), 0.0, kEps);
    EXPECT_NEAR(degreesToRadians(90.0), kPiOverTwo, kEps);
    EXPECT_NEAR(radiansToDegrees(kPi), 180.0, kEps);
    EXPECT_NEAR(radiansToDegrees(-kPiOverTwo), -90.0, kEps);
    // 角度单位误用防护：把 1 弧度当 1 度会差 ~57.3 倍，测试钉住换算比例。
    EXPECT_NEAR(radiansToDegrees(1.0), 180.0 / kPi, kEps);
}

TEST(MathUtils, ClampAndRange) {
    EXPECT_EQ(clamp(5.0, 0.0, 3.0), 3.0);
    EXPECT_EQ(clamp(-5.0, 0.0, 3.0), 0.0);
    EXPECT_EQ(clamp(1.5, 0.0, 3.0), 1.5);
    EXPECT_TRUE(withinRange(1.0, 0.0, 1.0));
    EXPECT_FALSE(withinRange(1.0 + 1.0e-12, 0.0, 1.0));
}

TEST(MathUtils, WrapLongitude) {
    // 规整到 [-pi, pi)。
    EXPECT_NEAR(wrapLongitude(0.0), 0.0, kEps);
    EXPECT_NEAR(wrapLongitude(kPi), -kPi, kEps);          // pi -> -pi（右开区间）
    EXPECT_NEAR(wrapLongitude(-kPi), -kPi, kEps);
    EXPECT_NEAR(wrapLongitude(kTwoPi), 0.0, kEps);
    EXPECT_NEAR(wrapLongitude(3.0 * kPi / 2.0), -kPiOverTwo, kEps); // 270° ≡ -90°
    EXPECT_NEAR(wrapLongitude(-3.0 * kPi / 2.0), kPiOverTwo, kEps);
    EXPECT_NEAR(wrapLongitude(1.9 * kPi), -0.1 * kPi, kEps);
    // 大数经度（多次环绕）也收敛。
    EXPECT_NEAR(wrapLongitude(100.0 * kTwoPi + 0.5), 0.5, kEps);
}

TEST(MathUtils, ClampLatitude) {
    EXPECT_NEAR(clampLatitude(kPiOverTwo + 0.1), kPiOverTwo, kEps);
    EXPECT_NEAR(clampLatitude(-kPiOverTwo - 0.1), -kPiOverTwo, kEps);
    EXPECT_NEAR(clampLatitude(0.3), 0.3, kEps);
}

TEST(MathUtils, SignNotZero) {
    EXPECT_EQ(signNotZero(0.0), 1.0);
    EXPECT_EQ(signNotZero(-3.0), -1.0);
    EXPECT_EQ(signNotZero(2.5), 1.0);
    // 极小正数不被当作零吞噬符号。
    EXPECT_EQ(signNotZero(1.0e-300), 1.0);
}

TEST(MathUtils, EqualsEpsilon) {
    EXPECT_TRUE(equalsEpsilon(1.0, 1.0 + 1.0e-9, 1.0e-6));
    EXPECT_FALSE(equalsEpsilon(1.0, 1.0 + 1.0e-3, 1.0e-6));
    // 绝对容差兜住近零值。
    EXPECT_TRUE(equalsEpsilon(0.0, 1.0e-12, 1.0e-6, 1.0e-9));
    EXPECT_FALSE(equalsEpsilon(0.0, 1.0e-8, 1.0e-6, 1.0e-9));
}
