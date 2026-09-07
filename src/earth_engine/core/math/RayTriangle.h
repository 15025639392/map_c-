#pragma once

#include <cmath>

#include "Vec3.h"

namespace earth_engine {

/// 射线与三角形求交（Möller–Trumbore，双精度）。
/// 不做背面剔除（双面命中）；t 以 dir 为单位的倍数，t >= 0 才算命中。
/// 命中时输出重心坐标 (outU, outV)（对应顶点 v1、v2；v0 权重 = 1-u-v）。
/// 退化三角形（零面积）返回 false。
inline bool rayTriangleIntersection(const Vec3& origin, const Vec3& direction, const Vec3& v0,
                                    const Vec3& v1, const Vec3& v2, double& outT, double& outU,
                                    double& outV, double epsilon = 1.0e-12) {
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 p = direction.cross(edge2);
    const double det = edge1.dot(p);
    if (std::fabs(det) < epsilon) {
        return false; // 平行或退化
    }
    const double invDet = 1.0 / det;
    const Vec3 tVec = origin - v0;
    const double u = tVec.dot(p) * invDet;
    if (u < -epsilon || u > 1.0 + epsilon) {
        return false;
    }
    const Vec3 q = tVec.cross(edge1);
    const double v = direction.dot(q) * invDet;
    if (v < -epsilon || u + v > 1.0 + epsilon) {
        return false;
    }
    const double t = edge2.dot(q) * invDet;
    if (t < 0.0) {
        return false;
    }
    outT = t;
    outU = u;
    outV = v;
    return true;
}

} // namespace earth_engine
