#include <gtest/gtest.h>

#include "earth_engine/core/math/Ray.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-12;

} // namespace

TEST(Ray, PointAt) {
    const Ray ray(Vec3(1.0, 2.0, 3.0), Vec3(1.0, 0.0, 0.0));
    EXPECT_EQ(ray.pointAt(0.0), Vec3(1.0, 2.0, 3.0));
    EXPECT_EQ(ray.pointAt(5.0), Vec3(6.0, 2.0, 3.0));
    EXPECT_EQ(ray.pointAt(-2.0), Vec3(-1.0, 2.0, 3.0));
}

TEST(Ray, ClosestPointOnLine) {
    const Ray ray(Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0));
    const Vec3 p(3.0, 4.0, 0.0);
    // 直线上最近点为 (3,0,0)。
    const Vec3 closest = ray.closestPointTo(p);
    EXPECT_EQ(closest, Vec3(3.0, 0.0, 0.0));
    // 连线垂直于方向。
    EXPECT_NEAR((p - closest).dot(ray.direction()), 0.0, kEps);
}

TEST(Ray, ClosestPointRespectsRayDirection) {
    const Ray ray(Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0));
    const Vec3 p(-5.0, 3.0, 0.0);
    // 最近直线点在负半轴：closestPointToRaySegment 应返回 origin。
    EXPECT_EQ(ray.closestPointToRaySegment(p), Vec3(0.0, 0.0, 0.0));
    // 正半轴则与直线解一致。
    const Vec3 p2(5.0, 3.0, 0.0);
    EXPECT_EQ(ray.closestPointToRaySegment(p2), Vec3(5.0, 0.0, 0.0));
}

TEST(Ray, NonUnitDirection) {
    // 不要求方向单位化；最近点公式对任意非零方向成立。
    const Ray ray(Vec3(0.0, 0.0, 0.0), Vec3(2.0, 0.0, 0.0));
    EXPECT_EQ(ray.closestPointTo(Vec3(6.0, 1.0, 0.0)), Vec3(6.0, 0.0, 0.0));
}

TEST(Ray, DegenerateDirection) {
    const Ray ray(Vec3(1.0, 1.0, 1.0), Vec3::zero());
    EXPECT_EQ(ray.closestPointTo(Vec3(9.0, 9.0, 9.0)), Vec3(1.0, 1.0, 1.0));
    EXPECT_EQ(ray.closestPointToRaySegment(Vec3(9.0, 9.0, 9.0)), Vec3(1.0, 1.0, 1.0));
    EXPECT_EQ(ray.pointAt(3.0), Vec3(1.0, 1.0, 1.0));
}

TEST(Ray, Equality) {
    const Ray a(Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0));
    const Ray b(Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0));
    const Ray c(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0));
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}
