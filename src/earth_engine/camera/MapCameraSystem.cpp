#include "earth_engine/camera/MapCameraSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace earth_engine {

namespace {
constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
constexpr double kEarthRadiusMeters = 6378137.0; // 平移用球面半径（WGS84 赤道）

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
double clampAbs(double v, double bound) { return std::clamp(v, -bound, bound); }
/// LOD 感知灵敏度包络：视距(distance) ≤ near → min；≥ far → 1；之间线性。
double lodFactor(double distanceMeters, double nearMeters, double farMeters, double minFactor) {
    if (distanceMeters <= nearMeters) {
        return minFactor;
    }
    if (distanceMeters >= farMeters) {
        return 1.0;
    }
    const double t = (distanceMeters - nearMeters) / (farMeters - nearMeters);
    return minFactor + (1.0 - minFactor) * t;
}

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
    panEastRateMps_ = 0.0;
    panNorthRateMps_ = 0.0;
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

void MapCameraSystem::setPanGesture(double panDxPx, double panDyPx, double screenHeightPx) {
    if (!isFinite(panDxPx) || !isFinite(panDyPx) || !isFinite(screenHeightPx) ||
        screenHeightPx <= 0.0) {
        return; // 脏输入忽略本帧平移
    }
    gPanDxPx_ = panDxPx;
    gPanDyPx_ = panDyPx;
    gScreenHeightPx_ = screenHeightPx;
    gHasPan_ = true;
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
    const bool anyInput = gHasInput_ || gHasPan_;
    // 有手势/平移 → 打断 flyTo（用户接管）。
    if (anyInput) {
        cancelFlyTo();
    }
    if (!flying_ && !anyInput && settled_) {
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
                const double f = lodFactor(state_.distanceMeters, params_.lodSpeed.nearMeters,
                                           params_.lodSpeed.farMeters,
                                           params_.lodSpeed.minFactor);
                state_.yawRateRadPerSec = rates.yawRateRadPerSec * f;
                state_.pitchRateRadPerSec = rates.pitchRateRadPerSec * f;
                state_.distRateMetersPerSec = rates.distRateMetersPerSec * f;
            } else {
                // 按住但无增量：制动（不残留惯性速率）。
                state_.yawRateRadPerSec = 0.0;
                state_.pitchRateRadPerSec = 0.0;
                state_.distRateMetersPerSec = 0.0;
            }
        }
        motion_.stepInertia(state_, dtSeconds);

        // —— 中心平移轴（引擎层；与旋转/缩放轴独立、可同帧组合）——
        if (gHasPan_) {
            if (gPanDxPx_ != 0.0 || gPanDyPx_ != 0.0) {
                // 像素 → 地面米：视距处每像素世界尺寸 × 屏幕高归一；
                // 内容跟随手指 → 中心反向移动（相机右向/屏幕上向的地面投影）。
                const double mpp = 2.0 * state_.distanceMeters *
                                   std::tan(0.5 * params_.fovRadians) / gScreenHeightPx_;
                const double yaw = state_.yawRad;
                const double sp = std::max(std::sin(state_.pitchRad), 0.017); // 防掠视除零
                const double sy = std::sin(yaw);
                const double cy = std::cos(yaw);
                // 右向 ENU：(cy,-sy)；屏幕上向地面水平投影方向 (sy,cy)，米/px=mpp·sp。
                const double eastRaw = gPanDxPx_ * cy + gPanDyPx_ * sp * sy;
                const double northRaw = -gPanDxPx_ * sy + gPanDyPx_ * sp * cy;
                const double f = lodFactor(state_.distanceMeters, params_.lodSpeed.nearMeters,
                                           params_.lodSpeed.farMeters,
                                           params_.lodSpeed.minFactor);
                panEastRateMps_ = -eastRaw * mpp * f / dtSeconds;
                panNorthRateMps_ = -northRaw * mpp * f / dtSeconds;
            } else {
                // 按住但无增量：平移轴制动。
                panEastRateMps_ = 0.0;
                panNorthRateMps_ = 0.0;
            }
        }
        // pan 惯性/阻尼（与旋转同阻尼系数与 dt 上限；速率上限钳制）。
        const double hPan = std::clamp(dtSeconds, 0.0, params_.motion.maxDtSeconds);
        const double dampFactor =
            std::max(0.0, 1.0 - params_.motion.dampingPerSecond * hPan);
        panEastRateMps_ =
            clampAbs(panEastRateMps_ * dampFactor, params_.maxPanRateMetersPerSec);
        panNorthRateMps_ =
            clampAbs(panNorthRateMps_ * dampFactor, params_.maxPanRateMetersPerSec);
        // 中心经纬球面小步积分（北/东，米）。
        centerLatRad_ += panNorthRateMps_ * hPan / kEarthRadiusMeters;
        const double cosLat = std::max(std::cos(centerLatRad_), 0.05);
        centerLonRad_ += panEastRateMps_ * hPan / (kEarthRadiusMeters * cosLat);
    }

    // 高度上限（防无限拉远出带）。
    state_.distanceMeters = std::min(state_.distanceMeters, params_.maxAltitudeMeters);

    // 贴地防护：正下方（当前中心）地表高度 + 净空（clamped → 抬到 floor）；
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
        const double th = params_.motion.settleThresholdPerSec;
        settled_ = motion_.isSettled(state_) && std::fabs(panEastRateMps_) < th &&
                   std::fabs(panNorthRateMps_) < th;
    }
    gHasInput_ = false;
    gHasPan_ = false;
}

} // namespace earth_engine
