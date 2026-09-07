#include <gtest/gtest.h>

#include "earth_engine/core/math/Ray.h"
#include "earth_engine/core/math/RayTriangle.h"

using namespace earth_engine;

TEST(RayTriangle, HitFromAbove) {
    // XY 平面 z=0 三角形。
    const Vec3 v0(0.0, 0.0, 0.0);
    const Vec3 v1(1.0, 0.0, 0.0);
    const Vec3 v2(0.0, 1.0, 0.0);
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    const Ray ray(Vec3(0.25, 0.25, 1.0), Vec3(0.0, 0.0, -1.0));
    ASSERT_TRUE(rayTriangleIntersection(ray.origin(), ray.direction(), v0, v1, v2, t, u, v));
    EXPECT_NEAR(t, 1.0, 1.0e-12);
    // 重心坐标：v0 权重 0.5，v1/u=0.25，v2/v=0.25。
    EXPECT_NEAR(u, 0.25, 1.0e-12);
    EXPECT_NEAR(v, 0.25, 1.0e-12);
    EXPECT_NEAR(ray.pointAt(t).x(), 0.25, 1.0e-12);
    EXPECT_NEAR(ray.pointAt(t).y(), 0.25, 1.0e-12);
}

TEST(RayTriangle, HitOnVertexAndEdge) {
    const Vec3 v0(0.0, 0.0, 0.0);
    const Vec3 v1(1.0, 0.0, 0.0);
    const Vec3 v2(0.0, 1.0, 0.0);
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    // 打到顶点 v0。
    const Ray r1(Vec3(0.0, 0.0, 5.0), Vec3(0.0, 0.0, -1.0));
    ASSERT_TRUE(rayTriangleIntersection(r1.origin(), r1.direction(), v0, v1, v2, t, u, v));
    EXPECT_NEAR(t, 5.0, 1.0e-12);
    EXPECT_NEAR(u, 0.0, 1.0e-12);
    EXPECT_NEAR(v, 0.0, 1.0e-12);
    // 打到边 v1-v2 中点。
    const Ray r2(Vec3(0.5, 0.5, 3.0), Vec3(0.0, 0.0, -1.0));
    ASSERT_TRUE(rayTriangleIntersection(r2.origin(), r2.direction(), v0, v1, v2, t, u, v));
    EXPECT_NEAR(t, 3.0, 1.0e-12);
}

TEST(RayTriangle, Misses) {
    const Vec3 v0(0.0, 0.0, 0.0);
    const Vec3 v1(1.0, 0.0, 0.0);
    const Vec3 v2(0.0, 1.0, 0.0);
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    // 三角形外。
    const Ray miss(Vec3(2.0, 2.0, 1.0), Vec3(0.0, 0.0, -1.0));
    EXPECT_FALSE(rayTriangleIntersection(miss.origin(), miss.direction(), v0, v1, v2, t, u, v));
    // 平行掠过。
    const Ray parallel(Vec3(0.25, 0.25, 1.0), Vec3(1.0, 0.0, 0.0));
    EXPECT_FALSE(rayTriangleIntersection(parallel.origin(), parallel.direction(), v0, v1, v2, t, u, v));
    // 三角形背后（t < 0）。
    const Ray behind(Vec3(0.25, 0.25, -1.0), Vec3(0.0, 0.0, -1.0));
    EXPECT_FALSE(rayTriangleIntersection(behind.origin(), behind.direction(), v0, v1, v2, t, u, v));
}

TEST(RayTriangle, DegenerateRejected) {
    const Vec3 a(0.0, 0.0, 0.0);
    const Vec3 b(1.0, 0.0, 0.0);
    const Vec3 c(2.0, 0.0, 0.0); // 共线 → 零面积
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    const Ray ray(Vec3(1.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));
    EXPECT_FALSE(rayTriangleIntersection(ray.origin(), ray.direction(), a, b, c, t, u, v));
}

TEST(RayTriangle, NoBackfaceCulling) {
    const Vec3 v0(0.0, 0.0, 0.0);
    const Vec3 v1(1.0, 0.0, 0.0);
    const Vec3 v2(0.0, 1.0, 0.0);
    double t = 0.0;
    double u = 0.0;
    double v = 0.0;
    // 从平面下方朝上看也命中（双面）。
    const Ray ray(Vec3(0.25, 0.25, -1.0), Vec3(0.0, 0.0, 1.0));
    ASSERT_TRUE(rayTriangleIntersection(ray.origin(), ray.direction(), v0, v1, v2, t, u, v));
    EXPECT_NEAR(t, 1.0, 1.0e-12);
}
