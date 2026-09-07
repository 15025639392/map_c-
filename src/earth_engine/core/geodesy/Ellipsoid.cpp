#include "earth_engine/core/geodesy/Ellipsoid.h"

#include <cmath>
#include <limits>

namespace earth_engine {

namespace {

// WGS84 定义（标准值）：
//   a = 6378137.0 m
//   f = 1 / 298.257223563
constexpr double kWgs84SemiMajorAxis = 6378137.0;
constexpr double kWgs84Flattening = 1.0 / 298.257223563;

} // namespace

Ellipsoid::Ellipsoid(double semiMajorAxis, double semiMinorAxis)
    : Ellipsoid(semiMajorAxis, semiMajorAxis, semiMinorAxis) {}

Ellipsoid::Ellipsoid(double radiusX, double radiusY, double radiusZ)
    : radii_(radiusX, radiusY, radiusZ) {
    if (radiusX <= 0.0 || radiusY <= 0.0 || radiusZ <= 0.0) {
        // 非法半径：保留构造值但把扁率/偏心率置为 0，避免除零污染。
        flattening_ = 0.0;
        eccentricitySquared_ = 0.0;
        return;
    }
    const double a = (radiusX + radiusY) * 0.5; // 赤道半径（旋转椭球取平均）
    flattening_ = (a - radiusZ) / a;
    const double e2 = 1.0 - (radiusZ * radiusZ) / (a * a);
    eccentricitySquared_ = e2 < 0.0 ? 0.0 : e2;
}

const Ellipsoid& Ellipsoid::WGS84() {
    static const Ellipsoid wgs84(kWgs84SemiMajorAxis,
                                 kWgs84SemiMajorAxis * (1.0 - kWgs84Flattening));
    return wgs84;
}

Vec3 Ellipsoid::geodeticSurfaceNormal(const Cartographic& cartographic) const {
    const double lon = cartographic.longitude();
    const double lat = cartographic.latitude();
    const double cosLat = std::cos(lat);
    return Vec3(cosLat * std::cos(lon), cosLat * std::sin(lon), std::sin(lat));
}

Vec3 Ellipsoid::geodeticSurfaceNormal(const Vec3& fixedFramePosition) const {
    const double rx2 = radii_.x() * radii_.x();
    const double rz2 = radii_.z() * radii_.z();
    // F(p) = x²/a² + y²/a² + z²/b² = 1 的梯度方向即法线。
    return Vec3(fixedFramePosition.x() / rx2, fixedFramePosition.y() / rx2,
                fixedFramePosition.z() / rz2)
        .normalized();
}

Vec3 Ellipsoid::cartographicToCartesian(const Cartographic& cartographic) const {
    const double lon = cartographic.longitude();
    const double lat = cartographic.latitude();
    const double h = cartographic.height();

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);

    // 卯酉圈曲率半径 N = a / sqrt(1 - e² sin²φ)
    const double n = radii_.x() / std::sqrt(1.0 - eccentricitySquared_ * sinLat * sinLat);

    const double x = (n + h) * cosLat * std::cos(lon);
    const double y = (n + h) * cosLat * std::sin(lon);
    const double z = (n * (1.0 - eccentricitySquared_) + h) * sinLat;
    return Vec3(x, y, z);
}

Cartographic Ellipsoid::cartesianToCartographic(const Vec3& fixedFramePosition) const {
    const double x = fixedFramePosition.x();
    const double y = fixedFramePosition.y();
    const double z = fixedFramePosition.z();

    const double longitude = std::atan2(y, x);
    const double radial = std::hypot(x, y);
    const double a = radii_.x();
    const double b = radii_.z();
    const double e2 = eccentricitySquared_;

    // 极区（径向可忽略）解析处理，避免 cos(φ) 除零。
    // 地轴点：北 z = b + h，南 z = -(b + h)。
    if (radial < 1.0e-12 * a) {
        const double latitude = (z >= 0.0) ? kPiOverTwo : -kPiOverTwo;
        const double height = (z >= 0.0) ? (z - b) : (-z - b);
        return Cartographic(longitude, latitude, height);
    }

    // 标准迭代：从球面纬度起步，收敛到大地纬度。
    double latitude = std::atan2(z, radial * (1.0 - e2));
    double height = 0.0;
    for (int iter = 0; iter < 12; ++iter) {
        const double sinLat = std::sin(latitude);
        const double n = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        height = radial / std::cos(latitude) - n;
        const double newLatitude =
            std::atan2(z, radial * (1.0 - e2 * n / (n + height)));
        if (std::fabs(newLatitude - latitude) < 1.0e-14) {
            latitude = newLatitude;
            break;
        }
        latitude = newLatitude;
    }
    const double sinLat = std::sin(latitude);
    const double n = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    height = radial / std::cos(latitude) - n;
    return Cartographic(longitude, latitude, height);
}

Vec3 Ellipsoid::scaleToGeodeticSurface(const Vec3& fixedFramePosition) const {
    // 法线垂足 = 去掉大地高后的点：先解出 (lon, lat, h)，再以 h=0 重投影。
    // 这样与 cartesianToCartographic / cartographicToCartesian 互为精确逆，
    // 避免径向缩放与"沿法线投影"的语义差（两者在非赤道处不一致）。
    const Cartographic carto = cartesianToCartographic(fixedFramePosition);
    return cartographicToCartesian(Cartographic(carto.longitude(), carto.latitude(), 0.0));
}

} // namespace earth_engine
