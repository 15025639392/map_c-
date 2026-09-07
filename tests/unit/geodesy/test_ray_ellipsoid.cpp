#include <gtest/gtest.h>

#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/geodesy/RayEllipsoid.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

constexpr double kMeterEps = 1.0e-6;
constexpr double kRelEps = 1.0e-9;

} // namespace

TEST(RayEllipsoid, StraightDownAboveEquator) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double a = e.semiMajorAxis();
    // 赤道外 1000 m，朝地心打。
    const Ray ray(Vec3(a + 1000.0, 0.0, 0.0), Vec3(-1.0, 0.0, 0.0));
    const RayEllipsoidHit hit = intersectRayEllipsoid(ray, e);
    ASSERT_TRUE(hit.hasHit);
    EXPECT_NEAR(hit.enterT, 1000.0, kMeterEps);
    // 出射点在对侧赤道：t = (a+1000) + a。
    EXPECT_NEAR(hit.exitT, 2.0 * a + 1000.0, 1.0e-3);
    EXPECT_EQ(hit.firstPositiveT().value(), hit.enterT);
}

TEST(RayEllipsoid, StraightDownAbovePole) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double b = e.semiMinorAxis();
    // 北极上空 500 m 垂直向下。
    const Ray ray(Vec3(0.0, 0.0, b + 500.0), Vec3(0.0, 0.0, -1.0));
    const RayEllipsoidHit hit = intersectRayEllipsoid(ray, e);
    ASSERT_TRUE(hit.hasHit);
    EXPECT_NEAR(hit.enterT, 500.0, kMeterEps);
    EXPECT_NEAR(hit.firstPositiveT().value(), 500.0, kMeterEps);
}

TEST(RayEllipsoid, MissesCompletely) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double a = e.semiMajorAxis();
    // 从赤道外侧 1000 m 沿 +X 平飞：全程在外。
    const Ray ray(Vec3(0.0, a + 1000.0, 0.0), Vec3(1.0, 0.0, 0.0));
    const RayEllipsoidHit hit = intersectRayEllipsoid(ray, e);
    EXPECT_FALSE(hit.hasHit);
    EXPECT_TRUE(hit.isMiss);
    EXPECT_FALSE(hit.firstPositiveT().has_value());
}

TEST(RayEllipsoid, GrazesTangentAtSurface) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double a = e.semiMajorAxis();
    // 从赤道表面出发沿 +Z（切向）：相切 → 判别式 ≈ 0，双根 ≈ 0。
    const Ray ray(Vec3(a, 0.0, 0.0), Vec3(0.0, 0.0, 1.0));
    const RayEllipsoidHit hit = intersectRayEllipsoid(ray, e);
    EXPECT_TRUE(hit.hasHit);
    EXPECT_NEAR(hit.enterT, 0.0, 1.0e-9);
    EXPECT_NEAR(hit.exitT, 0.0, 1.0e-9);
    EXPECT_EQ(hit.firstPositiveT().value(), 0.0);
}

TEST(RayEllipsoid, NearMissJustOutside) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double a = e.semiMajorAxis();
    // 距赤道表面 1 m 平飞：缩放空间中最近距离 (a+1)/a > 1 → 完全错过。
    const Ray ray(Vec3(0.0, a + 1.0, 0.0), Vec3(1.0, 0.0, 0.0));
    EXPECT_FALSE(intersectRayEllipsoid(ray, e).hasHit);
}

TEST(RayEllipsoid, OriginInsideEarth) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double b = e.semiMinorAxis();
    // 地心向 +Z：只有正根（出射 b），负根在身后。
    const Ray ray(Vec3::zero(), Vec3(0.0, 0.0, 1.0));
    const RayEllipsoidHit hit = intersectRayEllipsoid(ray, e);
    ASSERT_TRUE(hit.hasHit);
    EXPECT_NEAR(hit.enterT, -b, 1.0e-6);
    EXPECT_NEAR(hit.exitT, b, 1.0e-6);
    EXPECT_NEAR(hit.firstPositiveT().value(), b, 1.0e-6);
}

TEST(RayEllipsoid, CameraAimingAtGround) {
    // 相机在目标正上方 300 km，指向地面点：首个交点应在目标处。
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic target = Cartographic::fromDegrees(106.44, 29.70, 0.0);
    const Cartographic cameraCarto = Cartographic::fromDegrees(106.44, 29.70, 300000.0);
    const Vec3 camera = e.cartographicToCartesian(cameraCarto);
    const Vec3 ground = e.cartographicToCartesian(target);
    const Vec3 dir = (ground - camera).normalized();
    const Ray ray(camera, dir);

    const std::optional<double> t = firstRayEllipsoidIntersection(ray, e);
    ASSERT_TRUE(t.has_value());
    const double distance = camera.distanceTo(ground);
    EXPECT_NEAR(t.value(), distance, 1.0e-3);
    // 命中点 = 地面目标。
    const Vec3 hitPoint = ray.pointAt(t.value());
    EXPECT_NEAR(hitPoint.distanceTo(ground), 0.0, 1.0e-3);
    // 相对误差级别验证（远距命中精度）。
    EXPECT_LE(std::fabs(t.value() - distance) / distance, kRelEps);
}

TEST(RayEllipsoid, DegenerateZeroDirection) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Ray ray(Vec3(0.0, 0.0, e.semiMinorAxis() + 10.0), Vec3::zero());
    EXPECT_FALSE(intersectRayEllipsoid(ray, e).hasHit);
}
