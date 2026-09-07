#pragma once

#include "Vec3.h"

namespace earth_engine {

/// 射线：origin + t * direction（t >= 0）。
/// direction 不强制单位化；需要单位方向时用 direction.normalized()。
class Ray {
public:
    Ray() = default;
    Ray(const Vec3& origin, const Vec3& direction) : origin_(origin), direction_(direction) {}

    const Vec3& origin() const { return origin_; }
    const Vec3& direction() const { return direction_; }

    Vec3 pointAt(double t) const { return origin_ + direction_ * t; }

    /// 射线上离给定点最近的点（t 不限制 >= 0 时为直线最近点）。
    /// direction 为零向量时返回 origin。
    Vec3 closestPointTo(const Vec3& point) const {
        const double denom = direction_.magnitudeSquared();
        if (denom <= 0.0) {
            return origin_;
        }
        const double t = (point - origin_).dot(direction_) / denom;
        return pointAt(t);
    }

    /// 射线（t >= 0）上离给定点最近的点；若最近点落在负半轴则返回 origin。
    Vec3 closestPointToRaySegment(const Vec3& point) const {
        const double denom = direction_.magnitudeSquared();
        if (denom <= 0.0) {
            return origin_;
        }
        const double t = std::max(0.0, (point - origin_).dot(direction_) / denom);
        return pointAt(t);
    }

    bool operator==(const Ray& rhs) const {
        return origin_ == rhs.origin_ && direction_ == rhs.direction_;
    }
    bool operator!=(const Ray& rhs) const { return !(*this == rhs); }

private:
    Vec3 origin_;
    Vec3 direction_;
};

} // namespace earth_engine
