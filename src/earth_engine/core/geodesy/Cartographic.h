#pragma once

#include "../math/MathUtils.h"

namespace earth_engine {

/// 大地坐标。**longitude/latitude 单位为弧度**，height 单位为米（相对参考椭球面，
/// 可为负——地面以下）。
/// 规整约定：不在此处自动 wrap 经度（调用方需要时用 wrapLongitude）。
class Cartographic {
public:
    Cartographic() : longitude_(0.0), latitude_(0.0), height_(0.0) {}

    /// (longitudeRadians, latitudeRadians, heightMeters)。
    Cartographic(double longitudeRadians, double latitudeRadians, double heightMeters)
        : longitude_(longitudeRadians), latitude_(latitudeRadians), height_(heightMeters) {}

    /// 便捷构造：角度用度输入。
    static Cartographic fromDegrees(double longitudeDegrees, double latitudeDegrees,
                                    double heightMeters = 0.0) {
        return Cartographic(degreesToRadians(longitudeDegrees),
                            degreesToRadians(latitudeDegrees), heightMeters);
    }

    double longitude() const { return longitude_; }
    double latitude() const { return latitude_; }
    double height() const { return height_; }

    void setLongitude(double v) { longitude_ = v; }
    void setLatitude(double v) { latitude_ = v; }
    void setHeight(double v) { height_ = v; }

    bool operator==(const Cartographic& rhs) const {
        return longitude_ == rhs.longitude_ && latitude_ == rhs.latitude_ &&
               height_ == rhs.height_;
    }
    bool operator!=(const Cartographic& rhs) const { return !(*this == rhs); }

    bool equalsEpsilon(const Cartographic& rhs, double relativeEpsilon,
                       double absoluteEpsilon = 0.0) const {
        return earth_engine::equalsEpsilon(longitude_, rhs.longitude_, relativeEpsilon, absoluteEpsilon) &&
               earth_engine::equalsEpsilon(latitude_, rhs.latitude_, relativeEpsilon, absoluteEpsilon) &&
               earth_engine::equalsEpsilon(height_, rhs.height_, relativeEpsilon, absoluteEpsilon);
    }

private:
    double longitude_;
    double latitude_;
    double height_;
};

} // namespace earth_engine
