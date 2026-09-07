#include <gtest/gtest.h>

#include <cmath>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;


TEST(CameraView, BasisIsOrthonormal) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic::fromDegrees(106.5, 29.7, 3000.0));
    // 斜视（避免"正下方看"的 roll 退化构型）。
    const Vec3 target = e.cartographicToCartesian(
        Cartographic::fromDegrees(106.55, 29.63, 0.0));
    const Vec3 up = e.geodeticSurfaceNormal(Cartographic::fromDegrees(106.5, 29.7, 0.0));
    const CameraView cam(pos, target, up, degreesToRadians(60.0), 16.0 / 9.0);

    // 右手正交基（容差 1e-9，容纳归一化舍入）。
    EXPECT_NEAR(cam.forward().magnitude(), 1.0, 1.0e-12);
    EXPECT_NEAR(cam.right().magnitude(), 1.0, 1.0e-12);
    EXPECT_NEAR(cam.cameraUp().magnitude(), 1.0, 1.0e-12);
    EXPECT_NEAR(cam.forward().dot(cam.right()), 0.0, 1.0e-9);
    EXPECT_NEAR(cam.forward().dot(cam.cameraUp()), 0.0, 1.0e-9);
    EXPECT_NEAR(cam.right().dot(cam.cameraUp()), 0.0, 1.0e-9);
    // 视线对准目标方向。
    EXPECT_GT(cam.forward().dot((target - pos).normalized()), 0.999999);
}

TEST(CameraView, ScreenRayThroughCenterIsForward) {
    const Vec3 pos = Ellipsoid::WGS84().cartographicToCartesian(
        Cartographic::fromDegrees(106.5, 29.7, 3000.0));
    const Vec3 target = Ellipsoid::WGS84().cartographicToCartesian(
        Cartographic::fromDegrees(106.51, 29.71, 0.0));
    const CameraView cam(pos, target, Vec3(0.0, 0.0, 1.0), degreesToRadians(60.0), 1.0);

    const Ray center = cam.rayThroughNdc(0.0, 0.0);
    EXPECT_NEAR(center.direction().dot(cam.forward()), 1.0, 1.0e-12);

    // 角射线偏离视线：与 forward 夹角 ≤ fov 半角（含对角）。
    const Ray corner = cam.rayThroughNdc(1.0, 1.0);
    const double angle = cam.forward().angleBetween(corner.direction());
    EXPECT_GT(angle, degreesToRadians(30.0) - 1.0e-6); // 至少>半 fov
    EXPECT_LT(angle, kPiOverTwo);
}

TEST(CameraView, GroundFootprintContainsSubPoint) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), 50000.0));
    // 向下看：target = 相机正下方地表。
    const Vec3 target = e.cartographicToCartesian(center);
    const Vec3 up = e.geodeticSurfaceNormal(center);
    const CameraView cam(pos, target, up, degreesToRadians(60.0), 4.0 / 3.0);

    const auto footprint = cam.groundFootprintRadians(e);
    ASSERT_TRUE(footprint.has_value());
    // 脚印必须含正下方点（近似到脚印中心附近）。
    EXPECT_GE(footprint->east(), center.longitude());
    EXPECT_LE(footprint->west(), center.longitude());
    EXPECT_GE(footprint->north(), center.latitude());
    EXPECT_LE(footprint->south(), center.latitude());
    // 半宽应量级合理：h=50km, fovX ≈ 2·atan(tan30°·4/3) ≈ 75.7° → 半宽 ≈ h·tan(37.85°) ≈ 38.8km ≈ 0.35°。
    const double halfWidthDeg = radiansToDegrees(footprint->east() - footprint->west()) * 0.5;
    EXPECT_GT(halfWidthDeg, 0.2);
    EXPECT_LT(halfWidthDeg, 0.6);
}

TEST(CameraView, GroundFootprintNullWhenLookingAtSpace) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 pos = e.cartographicToCartesian(Cartographic::fromDegrees(0.0, 0.0, 1000000.0));
    // 朝天看：所有角射线都打不到地球。
    const CameraView cam(pos, pos + Vec3(0.0, 0.0, 1.0), Vec3(0.0, 0.0, 1.0),
                         degreesToRadians(60.0), 1.0);
    EXPECT_FALSE(cam.groundFootprintRadians(e).has_value());
}
