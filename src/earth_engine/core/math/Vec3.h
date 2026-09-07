#pragma once

#include <algorithm>
#include <cmath>

namespace earth_engine {

/// 三维双精度向量。几何/大地测量主数据类型（ECEF、局部 ENU、法线等）。
/// 语义约定：坐标单位为米（ECEF 帧）或无量纲方向向量，由调用方上下文决定。
class Vec3 {
public:
    constexpr Vec3() : x_(0.0), y_(0.0), z_(0.0) {}
    constexpr Vec3(double x, double y, double z) : x_(x), y_(y), z_(z) {}

    double x() const { return x_; }
    double y() const { return y_; }
    double z() const { return z_; }

    // ---- 常用常量 ----
    static constexpr Vec3 zero() { return Vec3(0.0, 0.0, 0.0); }
    static constexpr Vec3 unitX() { return Vec3(1.0, 0.0, 0.0); }
    static constexpr Vec3 unitY() { return Vec3(0.0, 1.0, 0.0); }
    static constexpr Vec3 unitZ() { return Vec3(0.0, 0.0, 1.0); }

    // ---- 标量运算 ----
    Vec3 operator+(const Vec3& rhs) const { return Vec3(x_ + rhs.x_, y_ + rhs.y_, z_ + rhs.z_); }
    Vec3 operator-(const Vec3& rhs) const { return Vec3(x_ - rhs.x_, y_ - rhs.y_, z_ - rhs.z_); }
    Vec3 operator*(double s) const { return Vec3(x_ * s, y_ * s, z_ * s); }
    Vec3 operator/(double s) const { return Vec3(x_ / s, y_ / s, z_ / s); }
    Vec3 operator-() const { return Vec3(-x_, -y_, -z_); }
    Vec3& operator+=(const Vec3& rhs) { x_ += rhs.x_; y_ += rhs.y_; z_ += rhs.z_; return *this; }
    Vec3& operator-=(const Vec3& rhs) { x_ -= rhs.x_; y_ -= rhs.y_; z_ -= rhs.z_; return *this; }
    Vec3& operator*=(double s) { x_ *= s; y_ *= s; z_ *= s; return *this; }

    friend Vec3 operator*(double s, const Vec3& v) { return v * s; }

    bool operator==(const Vec3& rhs) const { return x_ == rhs.x_ && y_ == rhs.y_ && z_ == rhs.z_; }
    bool operator!=(const Vec3& rhs) const { return !(*this == rhs); }

    double operator[](int index) const { return (&x_)[index]; }

    // ---- 内积 / 长度 ----
    double dot(const Vec3& rhs) const { return x_ * rhs.x_ + y_ * rhs.y_ + z_ * rhs.z_; }
    Vec3 cross(const Vec3& rhs) const {
        return Vec3(y_ * rhs.z_ - z_ * rhs.y_,
                    z_ * rhs.x_ - x_ * rhs.z_,
                    x_ * rhs.y_ - y_ * rhs.x_);
    }
    double magnitudeSquared() const { return x_ * x_ + y_ * y_ + z_ * z_; }
    double magnitude() const { return std::sqrt(magnitudeSquared()); }

    /// 返回单位向量；零向量返回零向量（不产生 NaN，文档化行为）。
    Vec3 normalized() const {
        const double m = magnitude();
        if (m <= 0.0) {
            return Vec3::zero();
        }
        return *this / m;
    }

    double distanceTo(const Vec3& rhs) const { return (*this - rhs).magnitude(); }
    double distanceSquaredTo(const Vec3& rhs) const { return (*this - rhs).magnitudeSquared(); }

    /// 与另一向量的夹角（弧度，[0, pi]）；零向量返回 0。
    double angleBetween(const Vec3& rhs) const {
        const double a = magnitude();
        const double b = rhs.magnitude();
        if (a <= 0.0 || b <= 0.0) {
            return 0.0;
        }
        const double cosine = clampDot(dot(rhs) / (a * b));
        return std::acos(cosine);
    }

    // ---- 插值 / 组合 ----
    static Vec3 lerp(const Vec3& from, const Vec3& to, double t) {
        return from + (to - from) * t;
    }
    static Vec3 midpoint(const Vec3& a, const Vec3& b) { return (a + b) * 0.5; }
    static Vec3 componentwiseMin(const Vec3& a, const Vec3& b) {
        return Vec3(std::min(a.x_, b.x_), std::min(a.y_, b.y_), std::min(a.z_, b.z_));
    }
    static Vec3 componentwiseMax(const Vec3& a, const Vec3& b) {
        return Vec3(std::max(a.x_, b.x_), std::max(a.y_, b.y_), std::max(a.z_, b.z_));
    }

    // ---- 判等 ----
    bool equalsEpsilon(const Vec3& rhs, double relativeEpsilon, double absoluteEpsilon = 0.0) const {
        return equalsEpsilon(x_, rhs.x_, relativeEpsilon, absoluteEpsilon) &&
               equalsEpsilon(y_, rhs.y_, relativeEpsilon, absoluteEpsilon) &&
               equalsEpsilon(z_, rhs.z_, relativeEpsilon, absoluteEpsilon);
    }

private:
    static double clampDot(double v) { return v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v); }
    static bool equalsEpsilon(double a, double b, double relativeEpsilon, double absoluteEpsilon) {
        const double diff = std::fabs(a - b);
        return diff <= absoluteEpsilon || diff <= relativeEpsilon * std::max(std::fabs(a), std::fabs(b));
    }

    double x_;
    double y_;
    double z_;
};

} // namespace earth_engine
