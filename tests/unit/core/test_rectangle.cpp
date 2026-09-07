#include <gtest/gtest.h>

#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/core/math/Rectangle.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-12;

} // namespace

TEST(Rectangle, FromDegreesAndUnits) {
    // 内部弧度单位：1 度跨度 → 弧度值。
    const Rectangle r = Rectangle::fromDegrees(0.0, 0.0, 1.0, 2.0);
    EXPECT_NEAR(r.west(), 0.0, kEps);
    EXPECT_NEAR(r.east(), degreesToRadians(1.0), kEps);
    EXPECT_NEAR(r.north(), degreesToRadians(2.0), kEps);
    EXPECT_NEAR(r.width(), degreesToRadians(1.0), kEps);
    EXPECT_NEAR(r.height(), degreesToRadians(2.0), kEps);
}

TEST(Rectangle, Getters) {
    const Rectangle r = Rectangle::fromDegrees(106.0, 29.0, 107.0, 30.0);
    EXPECT_EQ(r.south(), degreesToRadians(29.0));
    EXPECT_EQ(r.north(), degreesToRadians(30.0));
    EXPECT_EQ(r.west(), degreesToRadians(106.0));
    EXPECT_EQ(r.east(), degreesToRadians(107.0));
}

TEST(Rectangle, Contains) {
    const Rectangle r = Rectangle::fromDegrees(106.0, 29.0, 107.0, 30.0);
    // 内部 / 边界。
    EXPECT_TRUE(r.contains(degreesToRadians(106.5), degreesToRadians(29.5)));
    EXPECT_TRUE(r.contains(degreesToRadians(106.0), degreesToRadians(29.0)));
    EXPECT_TRUE(r.contains(degreesToRadians(107.0), degreesToRadians(30.0)));
    // 外部。
    EXPECT_FALSE(r.contains(degreesToRadians(105.0), degreesToRadians(29.5)));
    EXPECT_FALSE(r.contains(degreesToRadians(106.5), degreesToRadians(30.5)));
    EXPECT_FALSE(r.contains(degreesToRadians(106.5), degreesToRadians(28.0)));
}

TEST(Rectangle, EmptyAndMaxBounds) {
    const Rectangle empty = Rectangle(1.0, 1.0, 1.0, 2.0);
    EXPECT_TRUE(empty.isEmpty());
    const Rectangle full = Rectangle::maxBounds();
    EXPECT_FALSE(full.isEmpty());
    EXPECT_NEAR(full.width(), kTwoPi, kEps);
    EXPECT_NEAR(full.height(), kPi, kEps);
    // 全球覆盖。
    EXPECT_TRUE(full.contains(0.0, 0.0));
    EXPECT_TRUE(full.contains(kPi, kPiOverTwo));
    EXPECT_TRUE(full.contains(-kPi, -kPiOverTwo));
}

TEST(Rectangle, Equality) {
    EXPECT_EQ(Rectangle::fromDegrees(1.0, 2.0, 3.0, 4.0),
              Rectangle::fromDegrees(1.0, 2.0, 3.0, 4.0));
    EXPECT_NE(Rectangle::fromDegrees(1.0, 2.0, 3.0, 4.0),
              Rectangle::fromDegrees(1.0, 2.0, 3.0, 4.5));
}
