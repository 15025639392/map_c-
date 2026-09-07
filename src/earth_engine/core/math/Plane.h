#pragma once

#include <cmath>

#include "Vec3.h"

namespace earth_engine {

/// 平面：n·x + d = 0（n 单位化）。distanceTo(p) > 0 = p 在 n 指向一侧。
class Plane {
public:
    Plane() : normal_(Vec3::unitZ()), d_(0.0) {}

    /// 由单位法线与偏移构造；normal 非单位时先归一化。
    Plane(const Vec3& normal, double d) {
        const double len = normal.magnitude();
        if (len > 0.0) {
            normal_ = normal / len;
            d_ = d / len;
        } else {
            normal_ = Vec3::unitZ();
            d_ = 0.0;
        }
    }

    /// 由面上一点与法线构造。
    static Plane fromPointAndNormal(const Vec3& point, const Vec3& normal) {
        const double len = normal.magnitude();
        const Vec3 n = len > 0.0 ? normal / len : Vec3::unitZ();
        return Plane(n, -n.dot(point));
    }

    const Vec3& normal() const { return normal_; }
    double d() const { return d_; }

    /// 有符号距离：>0 = 点在法线侧（外），<0 = 内。
    double distanceTo(const Vec3& point) const { return normal_.dot(point) + d_; }

    /// 点是否在平面负侧（内）。
    bool containsPoint(const Vec3& point) const { return distanceTo(point) <= 0.0; }

private:
    Vec3 normal_;
    double d_;
};

} // namespace earth_engine
