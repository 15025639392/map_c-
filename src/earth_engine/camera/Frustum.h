#pragma once

#include <array>
#include <optional>

#include "CameraView.h"
#include "../core/math/Plane.h"

namespace earth_engine {

/// 相机视锥（无穷远，近平面可选）：由 CameraView 的位姿基与 fov/aspect 构造
/// 左右上下四个侧平面（法线向外）。供剔除/调度（球内测）复用。
class Frustum {
public:
    /// 由相机构造。nearDistanceMeters > 0 时附加近平面（法线 = 视线向）。
    static Frustum fromCamera(const CameraView& camera, double nearDistanceMeters = 0.0);

    /// 球是否与视锥相交（或在其内）：任一平面"外距 > 半径"即分离。
    bool intersectsSphere(const Vec3& center, double radius) const;

    /// 点是否在视锥内（可选近平面后为"被截锥"）。
    bool containsPoint(const Vec3& point) const;

    const std::array<Plane, 4>& sidePlanes() const { return side_; }
    std::optional<Plane> nearPlane() const { return near_; }

private:
    std::array<Plane, 4> side_; // left/right/top/bottom（法线向外）
    std::optional<Plane> near_;
};

} // namespace earth_engine
