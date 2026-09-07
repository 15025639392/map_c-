#include "earth_engine/camera/CameraMotion.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

CameraMotion::CameraMotion(CameraMotionParams params) : params_(params) {}

namespace {
// 带阻尼的速率衰减：指数近似 rate *= max(0, 1 − k·dt)，k = damping/秒。
double decay(double rate, double dampingPerSecond, double dt) {
    const double factor = std::max(0.0, 1.0 - dampingPerSecond * dt);
    return rate * factor;
}
double clampAbs(double v, double bound) { return std::clamp(v, -bound, bound); }
} // namespace

void CameraMotion::stepInertia(CameraMotionState& state, double dt) const {
    const double h = std::clamp(dt, 0.0, params_.maxDtSeconds);
    state.yawRateRadPerSec =
        clampAbs(decay(state.yawRateRadPerSec, params_.dampingPerSecond, h),
                 params_.maxYawRateRadPerSec);
    state.pitchRateRadPerSec =
        clampAbs(decay(state.pitchRateRadPerSec, params_.dampingPerSecond, h),
                 params_.maxPitchRateRadPerSec);
    state.distRateMetersPerSec =
        clampAbs(decay(state.distRateMetersPerSec, params_.dampingPerSecond, h),
                 params_.maxDistRateMetersPerSec);

    state.yawRad += state.yawRateRadPerSec * h;
    state.pitchRad += state.pitchRateRadPerSec * h;
    state.pitchRad = std::clamp(state.pitchRad, params_.minPitchRad, params_.maxPitchRad);
    state.distanceMeters = std::max(state.distanceMeters + state.distRateMetersPerSec * h,
                                    params_.minDistanceMeters);

    state.settled = isSettled(state);
}

void CameraMotion::stepFlyTo(CameraMotionState& state, double targetYawRad,
                             double targetPitchRad, double targetDistanceMeters,
                             double t) const {
    if (t >= 1.0) {
        // 精确钉死到目标：防浮点残留（yaw 归一化到 ±π 由上层处理，这里原样赋值）。
        state.yawRad = targetYawRad;
        state.pitchRad = std::clamp(targetPitchRad, params_.minPitchRad, params_.maxPitchRad);
        state.distanceMeters = std::max(targetDistanceMeters, params_.minDistanceMeters);
        state.yawRateRadPerSec = 0.0;
        state.pitchRateRadPerSec = 0.0;
        state.distRateMetersPerSec = 0.0;
        state.settled = true;
        return;
    }
    const double s = std::clamp(t, 0.0, 1.0);
    state.yawRad += (targetYawRad - state.yawRad) * s;
    state.pitchRad += (targetPitchRad - state.pitchRad) * s;
    state.pitchRad = std::clamp(state.pitchRad, params_.minPitchRad, params_.maxPitchRad);
    state.distanceMeters += (targetDistanceMeters - state.distanceMeters) * s;
    state.settled = false;
}

bool CameraMotion::isSettled(const CameraMotionState& state) const {
    const double th = params_.settleThresholdPerSec;
    return std::abs(state.yawRateRadPerSec) < th &&
           std::abs(state.pitchRateRadPerSec) < th &&
           std::abs(state.distRateMetersPerSec) < th;
}

} // namespace earth_engine
