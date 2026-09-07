#pragma once

#include "MathUtils.h"

namespace earth_engine {

/// 地理矩形（经纬范围），**内部单位为弧度**。
/// 约定：west <= east，且矩形宽度 <= 2*pi；跨反经线（宽 > pi）的矩形暂不支持
/// （遇到时先做经度拆分，阶段 4 瓦片体系再补工具）。
class Rectangle {
public:
    Rectangle() : west_(0.0), south_(0.0), east_(0.0), north_(0.0) {}

    /// (west, south, east, north)，单位弧度。
    Rectangle(double west, double south, double east, double north)
        : west_(west), south_(south), east_(east), north_(north) {}

    static Rectangle fromDegrees(double westDegrees, double southDegrees,
                                 double eastDegrees, double northDegrees) {
        return Rectangle(degreesToRadians(westDegrees), degreesToRadians(southDegrees),
                         degreesToRadians(eastDegrees), degreesToRadians(northDegrees));
    }

    static Rectangle maxBounds() {
        return Rectangle(-kPi, -kPiOverTwo, kPi, kPiOverTwo);
    }

    double west() const { return west_; }
    double south() const { return south_; }
    double east() const { return east_; }
    double north() const { return north_; }

    bool isEmpty() const { return west_ == east_ || south_ == north_; }

    /// 经度跨度（弧度，>= 0，<= 2*pi）。
    double width() const { return east_ - west_; }
    /// 纬度跨度（弧度，>= 0，<= pi）。
    double height() const { return north_ - south_; }

    /// 是否包含给定经纬（弧度）。经度不规整（超出 [-pi,pi) 也能比）：
    /// 该矩形被当作不跨缝的线性区间比较。
    bool contains(double longitudeRadians, double latitudeRadians) const {
        return longitudeRadians >= west_ && longitudeRadians <= east_ &&
               latitudeRadians >= south_ && latitudeRadians <= north_;
    }

    /// 判断两个矩形是否相等（严格）。
    bool operator==(const Rectangle& rhs) const {
        return west_ == rhs.west_ && south_ == rhs.south_ &&
               east_ == rhs.east_ && north_ == rhs.north_;
    }
    bool operator!=(const Rectangle& rhs) const { return !(*this == rhs); }

private:
    double west_;
    double south_;
    double east_;
    double north_;
};

} // namespace earth_engine
