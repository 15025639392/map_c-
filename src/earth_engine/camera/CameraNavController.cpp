#include "earth_engine/camera/CameraNavController.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

namespace {
constexpr double kEarthRadiusMeters = 6378137.0; // 小角近似用 WGS84 赤道半径
constexpr double kMinSinPitchForDistanceClamp = 0.02; // ~1.1°
} // namespace

CameraNavController::CameraNavController(const CameraMotionParams& motionParams,
                                         double minClearanceMeters)
    : motion_(motionParams), guard_(minClearanceMeters) {}

void CameraNavController::setTarget(const Cartographic& target, double targetElevMeters) {
    target_ = target;
    targetElevMeters_ = targetElevMeters;
}

void CameraNavController::setGroundFn(GroundHeightFn ground) {
    ground_ = std::move(ground);
}

void CameraNavController::step(double dtSeconds) {
    // 1) 惯性推进。
    motion_.stepInertia(state_, dtSeconds);
    if (ground_ == nullptr) {
        return; // 无地表约束（无 ground fn）：纯运动。
    }
    // 2) 贴地 clamp：相机当前位姿的高度不得低于该经纬地面+净空。
    const CameraPose p = pose();
    const std::optional<GroundClearanceResult> cleared =
        guard_.enforceClearance(p.lonRad, p.latRad, p.heightMeters, ground_);
    if (!cleared || !cleared->clamped) {
        return;
    }
    // 需要抬升：反解 distance（sin(pitch) 过小则不做距离强制，见头注释）。
    const double sinP = std::sin(state_.pitchRad);
    if (sinP <= kMinSinPitchForDistanceClamp) {
        return;
    }
    const double requiredDist = (cleared->heightMeters - targetElevMeters_) / sinP;
    state_.distanceMeters = std::max(requiredDist, motion_.params().minDistanceMeters);
}

CameraNavController::CameraPose CameraNavController::pose() const {
    const double horizontal = state_.distanceMeters * std::cos(state_.pitchRad);
    const double az = state_.yawRad;
    const double dAng = std::min(horizontal / kEarthRadiusMeters, 0.1); // 小角封顶
    const double lat0 = target_.latitude();
    const double lon0 = target_.longitude();
    // 球面小角平移（方位角 az：0=北，向东为正，与 ENU 约定一致）。
    const double sinLat1 =
        std::sin(lat0) * std::cos(dAng) + std::cos(lat0) * std::sin(dAng) * std::cos(az);
    const double lat1 = std::asin(std::clamp(sinLat1, -1.0, 1.0));
    const double lon1 =
        lon0 + std::atan2(std::sin(az) * std::sin(dAng) * std::cos(lat0),
                          std::cos(dAng) - std::sin(lat0) * std::sin(lat1));
    CameraPose p;
    p.lonRad = lon1;
    p.latRad = lat1;
    p.heightMeters = targetElevMeters_ + state_.distanceMeters * std::sin(state_.pitchRad);
    return p;
}

} // namespace earth_engine
