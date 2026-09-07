#include "earth_engine/core/geodesy/Projection.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

namespace {

/// Web Mercator 纬度钳制上限（度）：atan(sinh(π)) 的解 → ±85.05112878°。
constexpr double kWebMercatorMaxLatitudeDegrees = 85.05112877980659;

double mercatorMaxLatitudeRadians() {
    return degreesToRadians(kWebMercatorMaxLatitudeDegrees);
}

} // namespace

// ---------------------------------------------------------------------------
// GeographicProjection
// ---------------------------------------------------------------------------

GeographicProjection::GeographicProjection(const Ellipsoid& ellipsoid) : ellipsoid_(ellipsoid) {}

Vec2 GeographicProjection::project(const Cartographic& cartographic) const {
    const double a = ellipsoid_.maximumRadius();
    return Vec2(cartographic.longitude() * a, cartographic.latitude() * a);
}

Cartographic GeographicProjection::unproject(const Vec2& positionMeters) const {
    const double a = ellipsoid_.maximumRadius();
    return Cartographic(positionMeters.x() / a, positionMeters.y() / a, 0.0);
}

double GeographicProjection::northSouthMetersPerRadian(double) const {
    return ellipsoid_.maximumRadius(); // y = a·φ → dy/dφ = a（与纬度无关）
}

Rectangle GeographicProjection::projectableRectangle() const {
    return Rectangle(-kPi, -kPiOverTwo, kPi, kPiOverTwo);
}

// ---------------------------------------------------------------------------
// WebMercatorProjection
// ---------------------------------------------------------------------------

WebMercatorProjection::WebMercatorProjection(const Ellipsoid& ellipsoid) : ellipsoid_(ellipsoid) {}

double WebMercatorProjection::maximumLatitudeRadians() {
    static const double kMaxLat = mercatorMaxLatitudeRadians();
    return kMaxLat;
}

Vec2 WebMercatorProjection::project(const Cartographic& cartographic) const {
    const double a = ellipsoid_.maximumRadius();
    const double latitude = clamp(cartographic.latitude(), -maximumLatitudeRadians(),
                                  maximumLatitudeRadians());
    const double x = cartographic.longitude() * a;
    // y = a·ln(tan(π/4 + φ/2))；φ=0 → y=0，φ→±maxLat → y→±πa。
    const double y = a * std::log(std::tan(kPi / 4.0 + latitude * 0.5));
    return Vec2(x, y);
}

Cartographic WebMercatorProjection::unproject(const Vec2& positionMeters) const {
    const double a = ellipsoid_.maximumRadius();
    const double longitude = positionMeters.x() / a;
    const double latitude = 2.0 * std::atan(std::exp(positionMeters.y() / a)) - kPiOverTwo;
    return Cartographic(longitude, latitude, 0.0);
}

double WebMercatorProjection::northSouthMetersPerRadian(double latitudeRadians) const {
    const double lat = clamp(latitudeRadians, -maximumLatitudeRadians(), maximumLatitudeRadians());
    return ellipsoid_.maximumRadius() / std::cos(lat); // y' = a·secφ
}

Rectangle WebMercatorProjection::projectableRectangle() const {
    return Rectangle(-kPi, -maximumLatitudeRadians(), kPi, maximumLatitudeRadians());
}

} // namespace earth_engine
