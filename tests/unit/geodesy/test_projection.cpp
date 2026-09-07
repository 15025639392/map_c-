#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "earth_engine/core/geodesy/Projection.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

constexpr double kMeterEps = 1.0e-6;

double kA() { return Ellipsoid::WGS84().maximumRadius(); }

} // namespace

// ---------------------------------------------------------------------------
// GeographicProjection
// ---------------------------------------------------------------------------

TEST(GeographicProjection, KnownPoints) {
    const GeographicProjection proj;
    const Vec2 origin = proj.project(Cartographic(0.0, 0.0, 0.0));
    EXPECT_NEAR(origin.x(), 0.0, kMeterEps);
    EXPECT_NEAR(origin.y(), 0.0, kMeterEps);

    // 90°E 赤道 → x = π/2 · a。
    const Vec2 east90 = proj.project(Cartographic(kPiOverTwo, 0.0, 0.0));
    EXPECT_NEAR(east90.x(), kPiOverTwo * kA(), kMeterEps);
    // 45°N → y = π/4 · a。
    const Vec2 north45 = proj.project(Cartographic(0.0, kPi / 4.0, 0.0));
    EXPECT_NEAR(north45.y(), kPi / 4.0 * kA(), kMeterEps);
}

TEST(GeographicProjection, RoundTrip) {
    const GeographicProjection proj;
    const std::vector<Cartographic> cases = {
        Cartographic::fromDegrees(0.0, 0.0),
        Cartographic::fromDegrees(106.5, 29.7),
        Cartographic::fromDegrees(-179.9, -89.9),
        Cartographic::fromDegrees(180.0, 0.0),
    };
    for (const auto& c : cases) {
        const Vec2 p = proj.project(c);
        const Cartographic back = proj.unproject(p);
        EXPECT_NEAR(back.longitude(), c.longitude(), 1.0e-12) << c.longitude();
        EXPECT_NEAR(back.latitude(), c.latitude(), 1.0e-12) << c.latitude();
    }
}

TEST(GeographicProjection, NorthSouthScale) {
    const GeographicProjection proj;
    // dy/dφ = a，与纬度无关。
    EXPECT_NEAR(proj.northSouthMetersPerRadian(0.0), kA(), 1.0e-9);
    EXPECT_NEAR(proj.northSouthMetersPerRadian(degreesToRadians(60.0)), kA(), 1.0e-9);
    // 有限差分互验。
    const double lat = degreesToRadians(30.0);
    const double delta = 1.0e-6;
    const Cartographic hi(0.0, lat + delta, 0.0);
    const Cartographic lo(0.0, lat - delta, 0.0);
    const double diff = (proj.project(hi).y() - proj.project(lo).y()) / (2.0 * delta);
    EXPECT_NEAR(diff, kA(), 1.0e-3);
}

// ---------------------------------------------------------------------------
// WebMercatorProjection
// ---------------------------------------------------------------------------

TEST(WebMercatorProjection, KnownPoints) {
    const WebMercatorProjection proj;
    const Vec2 origin = proj.project(Cartographic(0.0, 0.0, 0.0));
    EXPECT_NEAR(origin.x(), 0.0, kMeterEps);
    EXPECT_NEAR(origin.y(), 0.0, kMeterEps);

    // 180°E → x = π·a（正方形世界的半宽）。
    const Vec2 lon180 = proj.project(Cartographic(kPi, 0.0, 0.0));
    EXPECT_NEAR(lon180.x(), kPi * kA(), 1.0e-3);

    // 纬度钳制上限 → y ≈ π·a（正方形世界半高）。
    const Vec2 top = proj.project(Cartographic(0.0, WebMercatorProjection::maximumLatitudeRadians(), 0.0));
    EXPECT_NEAR(top.y(), kPi * kA(), 1.0e-3);
    // 超过上限的纬度被钳制，不产生 inf/NaN。
    const Vec2 over = proj.project(Cartographic::fromDegrees(0.0, 89.9));
    EXPECT_TRUE(std::isfinite(over.y()));
    EXPECT_NEAR(over.y(), top.y(), 1.0e-3);
}

TEST(WebMercatorProjection, RoundTrip) {
    const WebMercatorProjection proj;
    const std::vector<Cartographic> cases = {
        Cartographic::fromDegrees(0.0, 0.0),
        Cartographic::fromDegrees(106.5, 29.7),
        Cartographic::fromDegrees(151.2, -33.9), // 悉尼
        Cartographic::fromDegrees(-73.98, 40.75), // 纽约
        Cartographic::fromDegrees(180.0, 0.0),
        Cartographic::fromDegrees(0.0, 85.0),
    };
    for (const auto& c : cases) {
        const Vec2 p = proj.project(c);
        const Cartographic back = proj.unproject(p);
        EXPECT_NEAR(back.longitude(), c.longitude(), 1.0e-12) << c.longitude();
        EXPECT_NEAR(back.latitude(), c.latitude(), 1.0e-12) << c.latitude();
    }
}

TEST(WebMercatorProjection, MonotonicInLatitude) {
    const WebMercatorProjection proj;
    double prev = -std::numeric_limits<double>::infinity();
    for (int deg = -85; deg <= 85; deg += 5) {
        const double y = proj.project(Cartographic::fromDegrees(0.0, deg)).y();
        EXPECT_GT(y, prev);
        prev = y;
    }
}

TEST(WebMercatorProjection, NorthSouthScale) {
    const WebMercatorProjection proj;
    // dy/dφ = a·secφ。
    EXPECT_NEAR(proj.northSouthMetersPerRadian(0.0), kA(), 1.0e-9);
    EXPECT_NEAR(proj.northSouthMetersPerRadian(degreesToRadians(60.0)), 2.0 * kA(), 1.0e-3);
    // 有限差分互验。
    const double lat = degreesToRadians(45.0);
    const double delta = 1.0e-6;
    const Cartographic hi(0.0, lat + delta, 0.0);
    const Cartographic lo(0.0, lat - delta, 0.0);
    const double diff = (proj.project(hi).y() - proj.project(lo).y()) / (2.0 * delta);
    EXPECT_NEAR(diff, proj.northSouthMetersPerRadian(lat), 1.0e-2);
}

TEST(WebMercatorProjection, ProjectableRectangle) {
    const WebMercatorProjection proj;
    const Rectangle r = proj.projectableRectangle();
    EXPECT_NEAR(r.west(), -kPi, 1.0e-12);
    EXPECT_NEAR(r.east(), kPi, 1.0e-12);
    EXPECT_NEAR(r.north(), WebMercatorProjection::maximumLatitudeRadians(), 1.0e-12);
    EXPECT_NEAR(r.south(), -WebMercatorProjection::maximumLatitudeRadians(), 1.0e-12);
}
