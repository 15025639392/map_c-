#include <gtest/gtest.h>

#include <vector>

#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

constexpr double kEps = 1.0e-9;
constexpr double kMeterEps = 1.0e-6; // 坐标（米）容差

// 已知 WGS84 常数（IUGG / 标准值）。
constexpr double kA = 6378137.0;
constexpr double kF = 1.0 / 298.257223563;
constexpr double kB = 6356752.3142451793; // a*(1-f)

/// ECEF 点是否落在椭球面上（F = x²/a² + y²/a² + z²/b² ≈ 1）。
double surfaceResidual(const Ellipsoid& e, const Vec3& p) {
    const double rx2 = e.radii().x() * e.radii().x();
    const double rz2 = e.radii().z() * e.radii().z();
    return (p.x() * p.x()) / rx2 + (p.y() * p.y()) / rx2 + (p.z() * p.z()) / rz2 - 1.0;
}

} // namespace

TEST(Ellipsoid, Wgs84Constants) {
    const Ellipsoid& wgs84 = Ellipsoid::WGS84();
    EXPECT_EQ(wgs84.semiMajorAxis(), kA);
    EXPECT_NEAR(wgs84.semiMinorAxis(), kB, 1.0e-9);
    EXPECT_NEAR(wgs84.flattening(), kF, 1.0e-15);
    // e² = f(2-f)
    EXPECT_NEAR(wgs84.eccentricitySquared(), kF * (2.0 - kF), 1.0e-15);
    EXPECT_EQ(wgs84.maximumRadius(), kA);
    EXPECT_EQ(wgs84.minimumRadius(), kB);
    EXPECT_EQ(wgs84.radii().x(), wgs84.radii().y());
}

TEST(Ellipsoid, GeodeticSurfaceNormalFromCartographic) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    // 赤道本初子午线：(1,0,0)；赤道 90°E：(0,1,0)；北极：(0,0,1)。
    // （π/2 处 sin/cos 有 ~1e-17 尾差，用分量 NEAR 而非精确相等。）
    const Vec3 n0 = e.geodeticSurfaceNormal(Cartographic(0.0, 0.0, 0.0));
    EXPECT_NEAR(n0.x(), 1.0, kEps);
    EXPECT_NEAR(n0.y(), 0.0, kEps);
    EXPECT_NEAR(n0.z(), 0.0, kEps);
    const Vec3 n1 = e.geodeticSurfaceNormal(Cartographic(kPiOverTwo, 0.0, 0.0));
    EXPECT_NEAR(n1.x(), 0.0, kEps);
    EXPECT_NEAR(n1.y(), 1.0, kEps);
    EXPECT_NEAR(n1.z(), 0.0, kEps);
    const Vec3 n2 = e.geodeticSurfaceNormal(Cartographic(0.0, kPiOverTwo, 0.0));
    EXPECT_NEAR(n2.x(), 0.0, kEps);
    EXPECT_NEAR(n2.y(), 0.0, kEps);
    EXPECT_NEAR(n2.z(), 1.0, kEps);
    // 一般点：(cosφcosλ, cosφsinλ, sinφ)，单位长。
    const double lon = degreesToRadians(106.5);
    const double lat = degreesToRadians(29.5);
    const Vec3 n = e.geodeticSurfaceNormal(Cartographic(lon, lat, 1000.0));
    EXPECT_NEAR(n.x(), std::cos(lat) * std::cos(lon), kEps);
    EXPECT_NEAR(n.y(), std::cos(lat) * std::sin(lon), kEps);
    EXPECT_NEAR(n.z(), std::sin(lat), kEps);
    EXPECT_NEAR(n.magnitude(), 1.0, kEps);
    // 高度不影响法线方向。
    EXPECT_EQ(e.geodeticSurfaceNormal(Cartographic(lon, lat, 0.0)), n);
}

TEST(Ellipsoid, GeodeticSurfaceNormalFromFixedFrame) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    // 轴上点：赤道 (a,0,0) → +X；北极 (0,0,b) → +Z。
    const Vec3 nx = e.geodeticSurfaceNormal(Vec3(kA, 0.0, 0.0));
    EXPECT_EQ(nx, Vec3::unitX());
    const Vec3 nz = e.geodeticSurfaceNormal(Vec3(0.0, 0.0, kB));
    EXPECT_EQ(nz, Vec3::unitZ());
    // 与 cartographic 版一致：面上任一点的法线。
    const double lon = degreesToRadians(-73.0);
    const double lat = degreesToRadians(40.0);
    const Vec3 surface = e.cartographicToCartesian(Cartographic(lon, lat, 0.0));
    const Vec3 n1 = e.geodeticSurfaceNormal(surface);
    const Vec3 n2 = e.geodeticSurfaceNormal(Cartographic(lon, lat, 0.0));
    EXPECT_NEAR(n1.x(), n2.x(), 1.0e-9);
    EXPECT_NEAR(n1.y(), n2.y(), 1.0e-9);
    EXPECT_NEAR(n1.z(), n2.z(), 1.0e-9);
}

TEST(Ellipsoid, CartographicToCartesianKnownPoints) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    // (0°,0°,0m) → (a,0,0)。
    const Vec3 p0 = e.cartographicToCartesian(Cartographic(0.0, 0.0, 0.0));
    EXPECT_NEAR(p0.x(), kA, kMeterEps);
    EXPECT_NEAR(p0.y(), 0.0, kMeterEps);
    EXPECT_NEAR(p0.z(), 0.0, kMeterEps);
    // (90°E,0°,0m) → (0,a,0)。
    const Vec3 p1 = e.cartographicToCartesian(Cartographic(kPiOverTwo, 0.0, 0.0));
    EXPECT_NEAR(p1.x(), 0.0, kMeterEps);
    EXPECT_NEAR(p1.y(), kA, kMeterEps);
    // (0°,90°N,0m) → (0,0,b)。
    const Vec3 p2 = e.cartographicToCartesian(Cartographic(0.0, kPiOverTwo, 0.0));
    EXPECT_NEAR(p2.z(), kB, kMeterEps);
    // (0°,90°S,0m) → (0,0,-b)。
    const Vec3 p3 = e.cartographicToCartesian(Cartographic(0.0, -kPiOverTwo, 0.0));
    EXPECT_NEAR(p3.z(), -kB, kMeterEps);
}

TEST(Ellipsoid, RoundTripConversions) {
    const Ellipsoid& e = Ellipsoid::WGS84();

    // 覆盖：赤道、中纬（北/南）、高纬、跨反经线、非零高（含负高=地下）。
    const std::vector<Cartographic> cases = {
        Cartographic::fromDegrees(0.0, 0.0, 0.0),
        Cartographic::fromDegrees(106.5, 29.7, 0.0),
        Cartographic::fromDegrees(-73.0, 40.7, 3500.0),
        Cartographic::fromDegrees(179.9, -45.0, 100000.0),
        Cartographic::fromDegrees(-179.9, 89.9, 500.0),
        Cartographic::fromDegrees(10.0, 60.0, -3000.0), // 地下
        Cartographic::fromDegrees(0.0, 89.9999999, 12345.0),
    };
    for (const auto& c : cases) {
        const Vec3 ecef = e.cartographicToCartesian(c);
        const Cartographic back = e.cartesianToCartographic(ecef);
        EXPECT_NEAR(back.longitude(), c.longitude(), 1.0e-12)
            << "lon mismatch for (" << c.longitude() << ", " << c.latitude() << ")";
        EXPECT_NEAR(back.latitude(), c.latitude(), 1.0e-12)
            << "lat mismatch for (" << c.longitude() << ", " << c.latitude() << ")";
        EXPECT_NEAR(back.height(), c.height(), 1.0e-6)
            << "height mismatch for (" << c.longitude() << ", " << c.latitude() << ")";
    }
}

TEST(Ellipsoid, HeightGrowsAlongNormal) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    // 抬高 h 米 = 沿大地法线平移 h（lat/lon 不变）。
    const double lon = degreesToRadians(106.44);
    const double lat = degreesToRadians(29.7);
    const Vec3 base = e.cartographicToCartesian(Cartographic(lon, lat, 0.0));
    const Vec3 lifted = e.cartographicToCartesian(Cartographic(lon, lat, 3000.0));
    const Vec3 normal = e.geodeticSurfaceNormal(Cartographic(lon, lat, 0.0));
    const Vec3 diff = lifted - base;
    EXPECT_NEAR(diff.magnitude(), 3000.0, 1.0e-6);
    EXPECT_NEAR(diff.x() / 3000.0, normal.x(), 1.0e-12);
    EXPECT_NEAR(diff.y() / 3000.0, normal.y(), 1.0e-12);
    EXPECT_NEAR(diff.z() / 3000.0, normal.z(), 1.0e-12);
}

TEST(Ellipsoid, PolarAxisHandling) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    // 地轴上的点（x=y=0）不应产生 NaN / 除零。
    const Vec3 northPole(0.0, 0.0, kB + 1000.0);
    const Cartographic c = e.cartesianToCartographic(northPole);
    EXPECT_NEAR(c.latitude(), kPiOverTwo, 1.0e-12);
    EXPECT_NEAR(c.height(), 1000.0, 1.0e-6);
    const Vec3 southPole(0.0, 0.0, -(kB + 500.0));
    const Cartographic c2 = e.cartesianToCartographic(southPole);
    EXPECT_NEAR(c2.latitude(), -kPiOverTwo, 1.0e-12);
    EXPECT_NEAR(c2.height(), 500.0, 1.0e-6);
}

TEST(Ellipsoid, ScaleToGeodeticSurface) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const double lon = degreesToRadians(106.5);
    const double lat = degreesToRadians(29.5);

    for (const double h : {0.0, 100.0, 12345.0}) {
        const Vec3 lifted = e.cartographicToCartesian(Cartographic(lon, lat, h));
        const Vec3 onSurface = e.scaleToGeodeticSurface(lifted);
        // 落回面上：残差近零。
        EXPECT_NEAR(surfaceResidual(e, onSurface), 0.0, 1.0e-12);
        // 与 (lat,lon,h=0) 的直接构造一致。
        const Vec3 expected = e.cartographicToCartesian(Cartographic(lon, lat, 0.0));
        EXPECT_NEAR(onSurface.x(), expected.x(), 1.0e-6);
        EXPECT_NEAR(onSurface.y(), expected.y(), 1.0e-6);
        EXPECT_NEAR(onSurface.z(), expected.z(), 1.0e-6);
        // 垂足方向与法线一致：P - P' = h·n̂。
        const Vec3 diff = lifted - onSurface;
        const Vec3 normal = e.geodeticSurfaceNormal(Cartographic(lon, lat, 0.0));
        EXPECT_NEAR(diff.magnitude(), std::fabs(h), 1.0e-6);
        if (h != 0.0) {
            EXPECT_NEAR(diff.normalized().x(), (h > 0 ? normal.x() : -normal.x()), 1.0e-9);
        }
    }
    // 内部点（地下负高）也落回面上。
    const Vec3 below = e.cartographicToCartesian(Cartographic(lon, lat, -2000.0));
    const Vec3 s = e.scaleToGeodeticSurface(below);
    EXPECT_NEAR(surfaceResidual(e, s), 0.0, 1.0e-12);
}

TEST(Ellipsoid, CustomEllipsoid) {
    // 自建椭球（例如月球简化 / 测试椭球）行为正确。
    const Ellipsoid moon(100.0, 90.0);
    EXPECT_EQ(moon.semiMajorAxis(), 100.0);
    EXPECT_EQ(moon.semiMinorAxis(), 90.0);
    EXPECT_NEAR(moon.flattening(), 0.1, 1.0e-15);
    const Cartographic c = Cartographic::fromDegrees(30.0, 45.0, 7.0);
    const Vec3 ecef = moon.cartographicToCartesian(c);
    const Cartographic back = moon.cartesianToCartographic(ecef);
    EXPECT_NEAR(back.longitude(), c.longitude(), 1.0e-12);
    EXPECT_NEAR(back.latitude(), c.latitude(), 1.0e-12);
    EXPECT_NEAR(back.height(), c.height(), 1.0e-9);
    // 球体（a=b）时 e²=0、f=0。
    const Ellipsoid sphere(5.0, 5.0);
    EXPECT_NEAR(sphere.flattening(), 0.0, 1.0e-15);
    EXPECT_NEAR(sphere.eccentricitySquared(), 0.0, 1.0e-15);
}

TEST(Ellipsoid, EqualityOperators) {
    const Ellipsoid a = Ellipsoid::WGS84();
    const Ellipsoid b(6378137.0, kB);
    const Ellipsoid c(6000000.0, 6000000.0);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}
