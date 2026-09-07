#include "earth_engine/core/geodesy/RayEllipsoid.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

namespace {

/// 缩放球二次方程 |O' + t·D'|² = 1：A t² + B t + C = 0。
struct ScaledQuadratic {
    double A;
    double B;
    double C;
};

ScaledQuadratic buildScaledQuadratic(const Ray& ray, const Ellipsoid& ellipsoid) {
    const double a = ellipsoid.radii().x();
    const double b = ellipsoid.radii().z();
    const Vec3 o = ray.origin();
    const Vec3 d = ray.direction();
    const Vec3 os(o.x() / a, o.y() / a, o.z() / b);
    const Vec3 ds(d.x() / a, d.y() / a, d.z() / b);
    ScaledQuadratic q;
    q.A = ds.magnitudeSquared();
    q.B = 2.0 * os.dot(ds);
    q.C = os.magnitudeSquared() - 1.0;
    return q;
}

} // namespace

RayEllipsoidHit intersectRayEllipsoid(const Ray& ray, const Ellipsoid& ellipsoid) {
    RayEllipsoidHit out;
    const ScaledQuadratic q = buildScaledQuadratic(ray, ellipsoid);

    if (q.A <= 0.0) {
        // 方向为零向量：直线退化为点。
        return out; // hasHit=false, isMiss=true
    }

    double disc = q.B * q.B - 4.0 * q.A * q.C;
    if (disc < 0.0) {
        // 数值噪声容忍：仅当负值相对项量级可忽略（真实相切的计算舍入）才当 0；
        // 真实的"微小错过"（如距表面 1 m 平飞）必须判 miss，不能用绝对阈值吞掉。
        const double termMagnitude =
            std::max(std::fabs(q.B * q.B), std::fabs(4.0 * q.A * q.C));
        const double noiseFloor = 1.0e-12 * termMagnitude;
        if (disc < -noiseFloor) {
            return out; // 完全错过
        }
        disc = 0.0; // 舍入级负判别式 → 按相切处理
    }

    const double sq = std::sqrt(disc);
    const double t0 = (-q.B - sq) / (2.0 * q.A);
    const double t1 = (-q.B + sq) / (2.0 * q.A);

    out.hasHit = true;
    out.isMiss = false;
    out.enterT = std::min(t0, t1);
    out.exitT = std::max(t0, t1);
    return out;
}

std::optional<double> firstRayEllipsoidIntersection(const Ray& ray, const Ellipsoid& ellipsoid) {
    return intersectRayEllipsoid(ray, ellipsoid).firstPositiveT();
}

} // namespace earth_engine
