#pragma once

#include "Cartographic.h"
#include "../math/Vec3.h"

namespace earth_engine {

/// WGS84 参考椭球体（旋转椭球：radii.x == radii.y）。
/// 提供 cartographic <-> ECEF（地心地固）坐标转换与几何工具。
/// 坐标语义：ECEF 原点在地心，单位米，+Z 指北极，+X 指 (0°N,0°E)。
class Ellipsoid {
public:
    Ellipsoid(double semiMajorAxis, double semiMinorAxis);
    Ellipsoid(double radiusX, double radiusY, double radiusZ);

    static const Ellipsoid& WGS84();

    double semiMajorAxis() const { return radii_.x(); }
    double semiMinorAxis() const { return radii_.z(); }
    /// 扁率 f = (a - b) / a。
    double flattening() const { return flattening_; }
    /// 第一偏心率平方 e² = f(2 - f) = (a² - b²)/a²。
    double eccentricitySquared() const { return eccentricitySquared_; }
    const Vec3& radii() const { return radii_; }
    double maximumRadius() const { return radii_.x(); }
    double minimumRadius() const { return radii_.z(); }

    bool operator==(const Ellipsoid& rhs) const { return radii_ == rhs.radii_; }
    bool operator!=(const Ellipsoid& rhs) const { return !(*this == rhs); }

    /// 大地坐标 (lon,lat) 处的椭球面外法线（单位向量）。
    Vec3 geodeticSurfaceNormal(const Cartographic& cartographic) const;
    /// ECEF 点处椭球面法线（单位向量）：grad(x²/a²+y²/a²+z²/b²) 归一。
    Vec3 geodeticSurfaceNormal(const Vec3& fixedFramePosition) const;

    /// cartographic（弧度 + 米高）→ ECEF 米坐标。
    Vec3 cartographicToCartesian(const Cartographic& cartographic) const;

    /// ECEF 米坐标 → cartographic（弧度 + 米高）。迭代解，收敛后误差 ~1e-9 m。
    Cartographic cartesianToCartographic(const Vec3& fixedFramePosition) const;

    /// 把 ECEF 点沿椭球面**大地法线**投影到椭球面上（返回面上点的 ECEF）。
    /// 定义：P 的大地坐标 (lon,lat,h) 的法线垂足 = ECEF(lon,lat,0)。
    /// 用于射线-椭球求交与地表投影。
    Vec3 scaleToGeodeticSurface(const Vec3& fixedFramePosition) const;

private:
    Vec3 radii_;            // (a, a, b)
    double flattening_;     // f
    double eccentricitySquared_;  // e²
};

} // namespace earth_engine
