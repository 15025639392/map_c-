#include <gtest/gtest.h>

#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-12;

} // namespace

TEST(Cartographic, DefaultAndFields) {
    const Cartographic c;
    EXPECT_EQ(c.longitude(), 0.0);
    EXPECT_EQ(c.latitude(), 0.0);
    EXPECT_EQ(c.height(), 0.0);
}

TEST(Cartographic, RadiansIsTheUnit) {
    // 内部单位为弧度：直接构造按弧度解释。
    const Cartographic c(kPiOverTwo, -kPi / 4.0, 12.5);
    EXPECT_NEAR(c.longitude(), kPiOverTwo, kEps);
    EXPECT_NEAR(c.latitude(), -kPi / 4.0, kEps);
    EXPECT_EQ(c.height(), 12.5);
}

TEST(Cartographic, FromDegrees) {
    const Cartographic c = Cartographic::fromDegrees(106.5, 29.5, 300.0);
    EXPECT_NEAR(c.longitude(), degreesToRadians(106.5), kEps);
    EXPECT_NEAR(c.latitude(), degreesToRadians(29.5), kEps);
    EXPECT_EQ(c.height(), 300.0);
    // 度/弧度误用防护：fromDegrees(106.5) 不能等于把 106.5 当弧度的值。
    EXPECT_NE(c.longitude(), 106.5);
}

TEST(Cartographic, Setters) {
    Cartographic c;
    c.setLongitude(0.5);
    c.setLatitude(-0.2);
    c.setHeight(77.0);
    EXPECT_EQ(c.longitude(), 0.5);
    EXPECT_EQ(c.latitude(), -0.2);
    EXPECT_EQ(c.height(), 77.0);
}

TEST(Cartographic, Equality) {
    EXPECT_EQ(Cartographic::fromDegrees(10.0, 20.0, 30.0),
              Cartographic::fromDegrees(10.0, 20.0, 30.0));
    EXPECT_NE(Cartographic::fromDegrees(10.0, 20.0, 30.0),
              Cartographic::fromDegrees(10.0, 20.0, 31.0));
    EXPECT_TRUE(Cartographic::fromDegrees(10.0, 20.0, 30.0)
                    .equalsEpsilon(Cartographic::fromDegrees(10.0 + 1e-9, 20.0, 30.0), 1.0e-6));
}
