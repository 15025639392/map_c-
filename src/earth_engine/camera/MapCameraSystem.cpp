#include "earth_engine/camera/MapCameraSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace earth_engine {

namespace {
constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

double wrapRadians(double r) {
    r = std::fmod(r, kTwoPi);
    return r < 0.0 ? r + kTwoPi : r;
}
/// 把 yaw 差 wrap 到 [-π, π)（flyTo 走短弧，不绕整圈）。
double wrapDeltaRad(double d) {
    d = std::fmod(d + 3.14159265358979323846, kTwoPi);
    if (d < 0.0) {
        d += kTwoPi;
    }
    return d - 3.14159265358979323846;
}
double smoothStep(double t) { return t * t * (3.0 - 2.0 * t); }

bool isFinite(double v) { return std::isfinite(v); }
} // namespace

MapCameraSystem::MapCameraSystem(Params params)
    : params_(params),
      motion_(params.motion),
      gestureMapper_(params.gesture),
      guard_(params.minClearanceMeters) {
    // turret 语义的 pitch 界：默认不允许抬头翻越地平线（0 = 地平线），
    // 由 params_.motion 的 min/maxPitchRad 承载（构造前可改）。
    state_.pitchRad = std::clamp(state_.pitchRad, params_.motion.minPitchRad,
                                 params_.motion.maxPitchRad);
    state_.distanceMeters =
        std::clamp(state_.distanceMeters, params_.motion.minDistanceMeters,
                   params_.maxAltitudeMeters);
}

void MapCameraSystem::setPose(const Pose& pose) {
    if (!isFinite(pose.lonRad) || !isFinite(pose.latRad) || !isFinite(pose.altitudeMeters) ||
        !isFinite(pose.headingRad) || !isFinite(pose.pitchRad)) {
        return; // 脏输入不吞不产
    }
    centerLonRad_ = pose.lonRad;
    centerLatRad_ = pose.latRad;
    state_.yawRad = pose.headingRad; // 允许任意（含多圈）累计；输出时 wrap
    state_.pitchRad = std::clamp(pose.pitchRad, params_.motion.minPitchRad,
                                 params_.motion.maxPitchRad);
    state_.distanceMeters =
        std::clamp(pose.altitudeMeters, params_.motion.minDistanceMeters,
                   params_.maxAltitudeMeters);
    // 边界钳制后仍可能低于真实地表：由下一步贴地防护抬升。
    state_.yawRateRadPerSec = 0.0;
    state_.pitchRateRadPerSec = 0.0;
    state_.distRateMetersPerSec = 0.0;
    settled_ = true;
    cancelFlyTo();
}

MapCameraSystem::Pose MapCameraSystem::pose() const {
    Pose p;
    p.lonRad = centerLonRad_;
    p.latRad = centerLatRad_;
    p.altitudeMeters = state_.distanceMeters;
    p.headingRad = wrapRadians(state_.yawRad);
    p.pitchRad = state_.pitchRad;
    return p;
}

void MapCameraSystem::setGesture(double dragDxPx, double dragDyPx, double pinchScale,
                                 double screenHeightPx) {
    if (!isFinite(dragDxPx) || !isFinite(dragDyPx) || !isFinite(pinchScale) ||
        !isFinite(screenHeightPx) || pinchScale <= 0.0 || screenHeightPx <= 0.0) {
        return; // 脏输入忽略本帧手势
    }
    gDxPx_ = dragDxPx;
    gDyPx_ = dragDyPx;
    gScale_ = pinchScale;
    gScreenHeightPx_ = screenHeightPx;
    gHasInput_ = true;
}

void MapCameraSystem::flyTo(const Pose& target) {
    if (!isFinite(target.altitudeMeters) || !isFinite(target.headingRad) ||
        !isFinite(target.pitchRad)) {
        return;
    }
    flyYawRad_ = target.headingRad;
    flyPitchRad_ = std::clamp(target.pitchRad, params_.motion.minPitchRad,
                              params_.motion.maxPitchRad);
    flyAltitudeMeters_ = std::clamp(target.altitudeMeters, params_.motion.minDistanceMeters,
                                    params_.maxAltitudeMeters);
    // flyTo 目标高度先做贴地抬升（不穿地）：目标低于正下方地表+净空 → 提到 floor，
    // 避免飞行中与落地后反复被 clamp 拉锯（引擎语义：飞行也绝不穿地）。
    if (ground_ != nullptr) {
        const std::optional<GroundClearanceResult> cleared =
            guard_.enforceClearance(centerLonRad_, centerLatRad_, flyAltitudeMeters_, ground_);
        if (cleared.has_value() && cleared->clamped) {
            flyAltitudeMeters_ = cleared->heightMeters;
        }
    }
    flyAltitudeMeters_ = std::max(flyAltitudeMeters_, params_.minAltitudeMeters);
    flyClockSeconds_ = 0.0;
    flying_ = true;
    settled_ = false;
}

void MapCameraSystem::cancelFlyTo() {
    flying_ = false;
    flyClockSeconds_ = 0.0;
}

void MapCameraSystem::step(double dtSeconds) {
    if (!(dtSeconds > 0.0) || !isFinite(dtSeconds)) {
        return;
    }
    // 有手势 → 打断 flyTo（用户接管）。
    if (gHasInput_) {
        cancelFlyTo();
    }
    if (!flying_ && !gHasInput_ && settled_) {
        return; // 收敛静止且无输入：零开销早退
    }

    if (flying_) {
        flyClockSeconds_ += dtSeconds;
        const double t =
            smoothStep(std::min(1.0, flyClockSeconds_ / std::max(1e-6, params_.flyToSeconds)));
        // 短弧目标：相对当前 yaw 的 wrap 差（t=1 时精确钉死由 stepFlyTo 保证）。
        const double yawTarget = state_.yawRad + wrapDeltaRad(flyYawRad_ - state_.yawRad);
        motion_.stepFlyTo(state_, yawTarget, flyPitchRad_, flyAltitudeMeters_, t);
        if (flyClockSeconds_ >= params_.flyToSeconds) {
            cancelFlyTo();
            settled_ = true;
        }
    } else {
        if (gHasInput_) {
            if (gDxPx_ != 0.0 || gDyPx_ != 0.0 || gScale_ != 1.0) {
                const GestureRates rates =
                    gestureMapper_.compute(gDxPx_, gDyPx_, gScale_, gScreenHeightPx_, dtSeconds);
                state_.yawRateRadPerSec = rates.yawRateRadPerSec;
                state_.pitchRateRadPerSec = rates.pitchRateRadPerSec;
                state_.distRateMetersPerSec = rates.distRateMetersPerSec;
            } else {
                // 按住但无增量：制动（不残留惯性速率）。
                state_.yawRateRadPerSec = 0.0;
                state_.pitchRateRadPerSec = 0.0;
                state_.distRateMetersPerSec = 0.0;
            }
        }
        motion_.stepInertia(state_, dtSeconds);
    }

    // 高度上限（防无限拉远出带）。
    state_.distanceMeters = std::min(state_.distanceMeters, params_.maxAltitudeMeters);

    // 贴地防护：正下方地表高度 + 净空（clamped → 抬到 floor）；
    // 无地表数据（enforceClearance 返回 nullopt 或未 clamp）不抬，
    // 统一再由 minAltitude 兜底（防极端贴地）。
    if (ground_ != nullptr) {
        const std::optional<GroundClearanceResult> cleared = guard_.enforceClearance(
            centerLonRad_, centerLatRad_, state_.distanceMeters, ground_);
        if (cleared.has_value() && cleared->clamped) {
            state_.distanceMeters = cleared->heightMeters;
        }
    }
    state_.distanceMeters = std::max(state_.distanceMeters, params_.minAltitudeMeters);

    if (!flying_) {
        settled_ = motion_.isSettled(state_);
    }
    gHasInput_ = false;
}

} // namespace earth_engine
