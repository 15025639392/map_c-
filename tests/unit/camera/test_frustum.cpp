#include <gtest/gtest.h>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/Frustum.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

TEST(Frustum, SimpleCartesianGeometry) {
    // 相机在 (0,0,10) 看向 −Z，up=+Y，fov 90°、aspect 1（半角 45°）。
    const CameraView cam(Vec3(0.0, 0.0, 10.0), Vec3(0.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0),
                         degreesToRadians(90.0), 1.0);
    const Frustum f = Frustum::fromCamera(cam);
    // 正前方点：视锥内。
    EXPECT_TRUE(f.containsPoint(Vec3(0.0, 0.0, 5.0)));
    // 距离 5 处左右半宽 = 5：略小于半宽 → 内；略大于 → 外。
    EXPECT_TRUE(f.containsPoint(Vec3(4.0, 0.0, 5.0)));
    EXPECT_FALSE(f.containsPoint(Vec3(6.0, 0.0, 5.0)));
    // 上方/下方同样以半角判。
    EXPECT_TRUE(f.containsPoint(Vec3(0.0, 4.0, 5.0)));
    EXPECT_FALSE(f.containsPoint(Vec3(0.0, 6.0, 5.0)));
    // 相机背后（无近平面时侧平面不闭合）：远点可能"在内"，用近平面测背后。
    const Frustum fn = Frustum::fromCamera(cam, /*near=*/2.0);
    EXPECT_TRUE(fn.containsPoint(Vec3(0.0, 0.0, 7.0))); // 前 3m > near 2m → 内
    EXPECT_FALSE(fn.containsPoint(Vec3(0.0, 0.0, 8.5))); // 前 1.5m < near → 外
}

TEST(Frustum, SphereCulling) {
    const CameraView cam(Vec3(0.0, 0.0, 10.0), Vec3(0.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0),
                         degreesToRadians(90.0), 1.0);
    const Frustum f = Frustum::fromCamera(cam);
    // 视锥内球。
    EXPECT_TRUE(f.intersectsSphere(Vec3(0.0, 0.0, 5.0), 1.0));
    // 骑在边界上的球（半径足够覆盖边界）→ 相交。
    EXPECT_TRUE(f.intersectsSphere(Vec3(5.0, 0.0, 5.0), 1.0));
    // 完全在左侧外。
    EXPECT_FALSE(f.intersectsSphere(Vec3(-20.0, 0.0, 5.0), 1.0));
    EXPECT_FALSE(f.intersectsSphere(Vec3(20.0, 0.0, 5.0), 1.0));
}

TEST(Frustum, EcefCameraGroundCulling) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), 100000.0));
    const Vec3 target = e.cartographicToCartesian(
        Cartographic::fromDegrees(106.55, 29.65, 0.0));
    const Vec3 up = e.geodeticSurfaceNormal(center);
    const CameraView cam(pos, target, up, degreesToRadians(60.0), 16.0 / 9.0);
    const Frustum f = Frustum::fromCamera(cam);

    // 视线前方地表附近（目标周围）→ 视锥内。
    const Vec3 inFront = e.cartographicToCartesian(
        Cartographic::fromDegrees(106.54, 29.66, 0.0));
    EXPECT_TRUE(f.containsPoint(inFront));
    EXPECT_TRUE(f.intersectsSphere(inFront, 500.0));
    // 地球对面 → 完全在视锥外。
    const Vec3 farSide = e.cartographicToCartesian(
        Cartographic::fromDegrees(-73.0, 40.0, 0.0));
    EXPECT_FALSE(f.containsPoint(farSide));
    EXPECT_FALSE(f.intersectsSphere(farSide, 50000.0));
}
