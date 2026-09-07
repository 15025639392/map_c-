#pragma once

#include <cmath>

#include "MathUtils.h"

namespace earth_engine {

/// 二维双精度向量。用于投影平面坐标（米）、屏幕/纹理坐标等。
class Vec2 {
public:
    constexpr Vec2() : x_(0.0), y_(0.0) {}
    constexpr Vec2(double x, double y) : x_(x), y_(y) {}

    double x() const { return x_; }
    double y() const { return y_; }

    static constexpr Vec2 zero() { return Vec2(0.0, 0.0); }

    Vec2 operator+(const Vec2& rhs) const { return Vec2(x_ + rhs.x_, y_ + rhs.y_); }
    Vec2 operator-(const Vec2& rhs) const { return Vec2(x_ - rhs.x_, y_ - rhs.y_); }
    Vec2 operator*(double s) const { return Vec2(x_ * s, y_ * s); }
    Vec2 operator/(double s) const { return Vec2(x_ / s, y_ / s); }
    Vec2 operator-() const { return Vec2(-x_, -y_); }

    bool operator==(const Vec2& rhs) const { return x_ == rhs.x_ && y_ == rhs.y_; }
    bool operator!=(const Vec2& rhs) const { return !(*this == rhs); }

    double dot(const Vec2& rhs) const { return x_ * rhs.x_ + y_ * rhs.y_; }
    double magnitudeSquared() const { return x_ * x_ + y_ * y_; }
    double magnitude() const { return std::sqrt(magnitudeSquared()); }
    double distanceTo(const Vec2& rhs) const { return (*this - rhs).magnitude(); }

    /// 返回单位向量；零向量返回零向量（不产生 NaN）。
    Vec2 normalized() const {
        const double m = magnitude();
        if (m <= 0.0) {
            return Vec2::zero();
        }
        return *this / m;
    }

    bool equalsEpsilon(const Vec2& rhs, double relativeEpsilon, double absoluteEpsilon = 0.0) const {
        return earth_engine::equalsEpsilon(x_, rhs.x_, relativeEpsilon, absoluteEpsilon) &&
               earth_engine::equalsEpsilon(y_, rhs.y_, relativeEpsilon, absoluteEpsilon);
    }

private:
    double x_;
    double y_;
};

} // namespace earth_engine
