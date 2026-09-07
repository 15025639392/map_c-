#pragma once

#include <algorithm>
#include <cmath>

namespace earth_engine {

// ---- 常量（C++17 无 std::numbers，自行定义）----
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPiOverTwo = 1.57079632679489661923;
constexpr double kDegreesPerRadian = 180.0 / kPi;
constexpr double kRadiansPerDegree = kPi / 180.0;

/// 计算中当作"零"的阈值（双精度浮点实用下限）。
constexpr double kEpsilonZeroTolerance = 1.0e-15;

// ---- 角度换算 ----
inline double degreesToRadians(double degrees) { return degrees * kRadiansPerDegree; }
inline double radiansToDegrees(double radians) { return radians * kDegreesPerRadian; }

// ---- 基础数值工具 ----
inline double clamp(double value, double minValue, double maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

/// 符号函数（不含零的特殊语义）：value < 0 → -1，否则（含 +0）→ +1。
inline double signNotZero(double value) {
    return value < 0.0 ? -1.0 : 1.0;
}

/// 近似相等（相对 + 绝对容差，仿 glm::epsilonEqual 的口径）。
inline bool equalsEpsilon(double a, double b, double relativeEpsilon, double absoluteEpsilon = 0.0) {
    const double diff = std::fabs(a - b);
    return diff <= absoluteEpsilon || diff <= relativeEpsilon * std::max(std::fabs(a), std::fabs(b));
}

/// 值是否在 [min, max]（含端点）内。
inline bool withinRange(double value, double minValue, double maxValue) {
    return value >= minValue && value <= maxValue;
}

// ---- 角度域工具 ----
/// 把经度规整到 [-pi, pi)。
inline double wrapLongitude(double longitudeRadians) {
    double wrapped = std::fmod(longitudeRadians + kPi, kTwoPi);
    if (wrapped < 0.0) {
        wrapped += kTwoPi;
    }
    return wrapped - kPi;
}

/// 把纬度钳制到 [-pi/2, pi/2]。
inline double clampLatitude(double latitudeRadians) {
    return clamp(latitudeRadians, -kPiOverTwo, kPiOverTwo);
}

} // namespace earth_engine
