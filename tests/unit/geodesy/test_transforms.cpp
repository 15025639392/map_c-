#include <gtest/gtest.h>

#include "earth_engine/core/geodesy/Transforms.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {


// 固定机位（与地形北极星固定验收机位同点：重庆缙云山一带）。
const Cartographic kOrigin = Cartographic::fromDegrees(106.44, 29.70, 300.0);

bool isOrthonormalBasis(const Mat4& m) {
    const Vec3 c0 = m.column(0);
    const Vec3 c1 = m.column(1);
    const Vec3 c2 = m.column(2);
    if (std::fabs(c0.magnitude() - 1.0) > 1.0e-12 ||
        std::fabs(c1.magnitude() - 1.0) > 1.0e-12 ||
        std::fabs(c2.magnitude() - 1.0) > 1.0e-12) {
        return false;
    }
    if (std::fabs(c0.dot(c1)) > 1.0e-12 || std::fabs(c0.dot(c2)) > 1.0e-12 ||
        std::fabs(c1.dot(c2)) > 1.0e-12) {
        return false;
    }
    return true;
}

} // namespace

TEST(Transforms, EnuToFixedFrameOrigin) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Mat4 m = Transforms::eastNorthUpToFixedFrame(kOrigin, e);

    // 原点映射：ENU(0,0,0) → 原点 ECEF。
    const Vec3 originEcef = e.cartographicToCartesian(kOrigin);
    EXPECT_EQ(m.transformPoint(Vec3::zero()), originEcef);
    EXPECT_EQ(m.translation(), originEcef);
}

TEST(Transforms, EnuAxesAreOrthonormal) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Mat4 m = Transforms::eastNorthUpToFixedFrame(kOrigin, e);
    EXPECT_TRUE(isOrthonormalBasis(m));

    // 第三列 = 大地法线（up 与高度无关）。
    const Vec3 up = e.geodeticSurfaceNormal(kOrigin);
    const Vec3 upCol = m.column(2);
    EXPECT_NEAR(upCol.x(), up.x(), 1.0e-12);
    EXPECT_NEAR(upCol.y(), up.y(), 1.0e-12);
    EXPECT_NEAR(upCol.z(), up.z(), 1.0e-12);

    // 第一列 = 东向 (-sinλ, cosλ, 0)。
    EXPECT_NEAR(m.column(0).x(), -std::sin(kOrigin.longitude()), 1.0e-12);
    EXPECT_NEAR(m.column(0).y(), std::cos(kOrigin.longitude()), 1.0e-12);
    EXPECT_NEAR(m.column(0).z(), 0.0, 1.0e-12);
}

TEST(Transforms, EastNorthMatchCoordinateDerivatives) {
    // 用有限差分验证：ECEF 沿经度增大方向 = 东轴，沿纬度增大方向 = 北轴。
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Mat4 m = Transforms::eastNorthUpToFixedFrame(kOrigin, e);

    const double delta = 1.0e-7; // 弧度
    const Cartographic lonP = Cartographic(kOrigin.longitude() + delta, kOrigin.latitude(), kOrigin.height());
    const Cartographic lonM = Cartographic(kOrigin.longitude() - delta, kOrigin.latitude(), kOrigin.height());
    Vec3 dLon = (e.cartographicToCartesian(lonP) - e.cartographicToCartesian(lonM)).normalized();
    // 有限差分方向收敛到东轴。
    const Vec3 eastAxis = m.column(0);
    EXPECT_NEAR(dLon.dot(eastAxis), 1.0, 1.0e-6);

    const Cartographic latP = Cartographic(kOrigin.longitude(), kOrigin.latitude() + delta, kOrigin.height());
    const Cartographic latM = Cartographic(kOrigin.longitude(), kOrigin.latitude() - delta, kOrigin.height());
    Vec3 dLat = (e.cartographicToCartesian(latP) - e.cartographicToCartesian(latM)).normalized();
    const Vec3 northAxis = m.column(1);
    EXPECT_NEAR(dLat.dot(northAxis), 1.0, 1.0e-6);

    // 东/北夹角 ~90°（有限差分近似下允许小误差）。
    EXPECT_NEAR(dLon.dot(dLat), 0.0, 1.0e-4);
}

TEST(Transforms, RoundTripEnuToEcef) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Mat4 toFixed = Transforms::eastNorthUpToFixedFrame(kOrigin, e);
    const Mat4 toLocal = Transforms::fixedFrameToEastNorthUp(kOrigin, e);

    // 一个 ENU 局部点（东 1000m，北 -500m，上 200m）。
    const Vec3 enu(1000.0, -500.0, 200.0);
    const Vec3 ecef = toFixed.transformPoint(enu);
    // 逆变换应精确回到局部点。
    const Vec3 enuBack = toLocal.transformPoint(ecef);
    EXPECT_NEAR(enuBack.x(), enu.x(), 1.0e-6);
    EXPECT_NEAR(enuBack.y(), enu.y(), 1.0e-6);
    EXPECT_NEAR(enuBack.z(), enu.z(), 1.0e-6);

    // 顺带验证往返距离守恒：局部两点在 ECEF 下距离不变。
    const Vec3 enu2(50.0, 60.0, 70.0);
    EXPECT_NEAR(toFixed.transformPoint(enu).distanceTo(toFixed.transformPoint(enu2)),
                enu.distanceTo(enu2), 1.0e-6);
}

TEST(Transforms, InverseMatchesGeneralInverse) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Mat4 toFixed = Transforms::eastNorthUpToFixedFrame(kOrigin, e);
    const Mat4 toLocal = Transforms::fixedFrameToEastNorthUp(kOrigin, e);

    bool ok = false;
    const Mat4 generalInverse = toFixed.inverse(&ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(toLocal.equalsEpsilon(generalInverse, 1.0e-9, 1.0e-9));
}

TEST(Transforms, DifferentOriginsAndHeights) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    // 原点带高度时，局部帧原点 = 该高度的 ECEF 点。
    const Cartographic high = Cartographic::fromDegrees(120.0, -30.0, 5000.0);
    const Mat4 m = Transforms::eastNorthUpToFixedFrame(high, e);
    EXPECT_EQ(m.transformPoint(Vec3::zero()), e.cartographicToCartesian(high));
    EXPECT_TRUE(isOrthonormalBasis(m));

    // 南半球：north 轴仍然指向地理北（列 1 的 z 分量为正）。
    const Mat4 south = Transforms::eastNorthUpToFixedFrame(
        Cartographic::fromDegrees(0.0, -60.0, 0.0), e);
    EXPECT_GT(south.column(1).z(), 0.0);
    // 赤道 up = +Z。
    const Mat4 equator = Transforms::eastNorthUpToFixedFrame(
        Cartographic::fromDegrees(0.0, 0.0, 0.0), e);
    EXPECT_NEAR(equator.column(2).x(), 1.0, 1.0e-12);
    EXPECT_NEAR(equator.column(2).z(), 0.0, 1.0e-12);
}
