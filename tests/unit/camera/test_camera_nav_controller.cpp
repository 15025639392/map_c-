// CameraNavController：惯性步进 × 贴地防护联动的可测控制器（S6 host 切片）。
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>

#include "earth_engine/camera/CameraNavController.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

CameraNavController::GroundHeightFn flatGround(double h) {
    return [h](const Cartographic&) -> std::optional<double> { return h; };
}

} // namespace

TEST(CameraNavController, SkimmingCameraIsPushedAboveGroundClearance) {
    const double groundH = 500.0;
    CameraNavController ctrl;
    ctrl.setTarget(Cartographic(degreesToRadians(106.5), degreesToRadians(29.7), 0.0),
                   /*targetElev=*/groundH);
    ctrl.setGroundFn(flatGround(groundH));
    CameraMotionState& s = ctrl.state();
    s.pitchRad = 0.6;      // 明显向下
    s.distanceMeters = 1.0; // 相机几乎贴地（高度 ≈ 500.56 < 505）
    s.yawRateRadPerSec = 0.0;

    ctrl.step(0.016);
    const auto p = ctrl.pose();
    // 不穿地：相机高度 ≥ ground + clearance(5)。
    EXPECT_GE(p.heightMeters, groundH + 5.0 - 1e-9);
    EXPECT_TRUE(std::isfinite(p.heightMeters));
    // distance 被抬升到满足 clearance。
    EXPECT_GE(s.distanceMeters, (groundH + 5.0 - groundH) / std::sin(0.6) - 1e-9);
}

TEST(CameraNavController, SafeCameraIsNotTouched) {
    CameraNavController ctrl;
    ctrl.setTarget(Cartographic(degreesToRadians(106.5), degreesToRadians(29.7), 0.0), 500.0);
    ctrl.setGroundFn(flatGround(500.0));
    CameraMotionState& s = ctrl.state();
    s.pitchRad = 0.6;
    s.distanceMeters = 3000.0; // 高度 ≈ 1693 >> 505
    const double d0 = s.distanceMeters;
    ctrl.step(0.016);
    EXPECT_DOUBLE_EQ(s.distanceMeters, d0); // 未被 clamp
    EXPECT_GE(ctrl.pose().heightMeters, 505.0);
}

TEST(CameraNavController, InertiaStillConvergesWhileGuardActive) {
    CameraNavController ctrl;
    ctrl.setTarget(Cartographic(0.0, 0.0, 0.0), 0.0);
    ctrl.setGroundFn(flatGround(0.0));
    CameraMotionState& s = ctrl.state();
    s.pitchRad = 0.8;
    s.distanceMeters = 10.0; // 高度≈7.17 但 ground 0 → floor 5 → distance 需 ≥6.25 → ok 已满足
    s.yawRateRadPerSec = 1.2;
    int steps = 0;
    for (; steps < 20000 && !ctrl.isSettled(); ++steps) {
        ctrl.step(0.016);
    }
    EXPECT_LT(steps, 20000);
    EXPECT_TRUE(ctrl.isSettled());
    EXPECT_TRUE(std::isfinite(ctrl.pose().heightMeters));
    // 整段不穿地。
    EXPECT_GE(ctrl.pose().heightMeters, 5.0 - 1e-9);
}

TEST(CameraNavController, DeterministicSameInputsSamePose) {
    CameraNavController a;
    a.setTarget(Cartographic(degreesToRadians(106.5), degreesToRadians(29.7), 0.0), 0.0);
    a.setGroundFn(flatGround(0.0));
    CameraNavController b = a;
    CameraMotionState& sa = a.state();
    sa.yawRateRadPerSec = 0.9;
    sa.pitchRad = 0.4;
    sa.distanceMeters = 800.0;
    CameraMotionState& sb = b.state();
    sb.yawRateRadPerSec = 0.9;
    sb.pitchRad = 0.4;
    sb.distanceMeters = 800.0;
    const double dt[] = {0.016, 0.033, 0.016, 0.05};
    for (double d : dt) {
        a.step(d);
        b.step(d);
    }
    const auto pa = a.pose();
    const auto pb = b.pose();
    EXPECT_DOUBLE_EQ(pa.lonRad, pb.lonRad);
    EXPECT_DOUBLE_EQ(pa.latRad, pb.latRad);
    EXPECT_DOUBLE_EQ(pa.heightMeters, pb.heightMeters);
}

TEST(CameraNavController, NoGroundFnMeansPureMotion) {
    CameraNavController ctrl; // 未 setGroundFn
    ctrl.setTarget(Cartographic(0.0, 0.0, 0.0), 0.0);
    CameraMotionState& s = ctrl.state();
    s.pitchRad = 0.6;
    s.distanceMeters = 100.0; // > 默认 minDistance(30)：贴地 clamp 不介入
    const double d0 = s.distanceMeters;
    ctrl.step(0.016);
    EXPECT_DOUBLE_EQ(s.distanceMeters, d0); // 无地表约束：不 clamp
}
