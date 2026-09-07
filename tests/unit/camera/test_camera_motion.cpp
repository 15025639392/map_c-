// CameraMotion：相机运动模型（S6 host 地基）——惯性收敛不发散 / flyTo 钉死终点 /
// 确定性。纯数值，不接 GL/输入。
#include <gtest/gtest.h>

#include <cmath>

#include "earth_engine/camera/CameraMotion.h"

using namespace earth_engine;

TEST(CameraMotion, InertiaConvergesToSettledInFiniteSteps) {
    CameraMotion motion;
    CameraMotionState s;
    s.yawRateRadPerSec = 1.0;
    s.pitchRateRadPerSec = -0.8;
    s.distRateMetersPerSec = 1500.0;
    s.settled = false;
    int steps = 0;
    for (; steps < 10000 && !s.settled; ++steps) {
        motion.stepInertia(s, 0.016); // 60fps 帧步
    }
    EXPECT_LT(steps, 10000); // 有限步内收敛
    EXPECT_TRUE(s.settled);
    EXPECT_TRUE(std::isfinite(s.yawRad) && std::isfinite(s.distanceMeters));
}

TEST(CameraMotion, LargeDtStaysBoundedWithoutNaN) {
    // 大步长（远超 maxDt）被钳到上限，不产生 NaN/发散。
    CameraMotion motion;
    CameraMotionState s;
    s.yawRateRadPerSec = 1000.0; // 远超市面速率 → 被钳制
    for (int i = 0; i < 100; ++i) {
        motion.stepInertia(s, 1e9);
        EXPECT_TRUE(std::isfinite(s.yawRad));
        EXPECT_TRUE(std::isfinite(s.pitchRad));
        EXPECT_TRUE(std::isfinite(s.distanceMeters));
        EXPECT_LE(std::abs(s.yawRateRadPerSec), motion.params().maxYawRateRadPerSec + 1e-12);
    }
    // 俯仰被钳到合法视角范围。
    EXPECT_GE(s.pitchRad, motion.params().minPitchRad - 1e-12);
    EXPECT_LE(s.pitchRad, motion.params().maxPitchRad + 1e-12);
    EXPECT_GE(s.distanceMeters, motion.params().minDistanceMeters - 1e-9);
}

TEST(CameraMotion, FlyToReachesTargetExactlyAndSnaps) {
    CameraMotion motion;
    CameraMotionState s;
    s.yawRad = 0.0;
    s.pitchRad = -0.2;
    s.distanceMeters = 5000.0;
    const double targetYaw = 1.2;
    const double targetPitch = -0.6;
    const double targetDist = 800.0;
    const int steps = 100;
    for (int i = 0; i < steps; ++i) {
        motion.stepFlyTo(s, targetYaw, targetPitch, targetDist, static_cast<double>(i + 1) / steps);
    }
    EXPECT_DOUBLE_EQ(s.yawRad, targetYaw);
    EXPECT_DOUBLE_EQ(s.pitchRad, targetPitch);
    EXPECT_DOUBLE_EQ(s.distanceMeters, targetDist);
    EXPECT_TRUE(motion.isSettled(s));
}

TEST(CameraMotion, FlyToIsMonotonicPerAxis) {
    CameraMotion motion;
    CameraMotionState s;
    s.yawRad = 0.0;
    s.pitchRad = 0.0;
    s.distanceMeters = 1000.0;
    double prevDist = s.distanceMeters;
    for (int i = 1; i <= 50; ++i) {
        motion.stepFlyTo(s, 1.0, -0.5, 500.0, static_cast<double>(i) / 50.0);
        // 目标距离低于初值 → 单调递减（不倒退、不越界到目标以下）。
        EXPECT_LE(s.distanceMeters, prevDist + 1e-9);
        EXPECT_GE(s.distanceMeters, 500.0 - 1e-9);
        prevDist = s.distanceMeters;
    }
}

TEST(CameraMotion, ZeroInputStaysPutAndSettled) {
    CameraMotion motion;
    CameraMotionState s;
    s.settled = true;
    const double yaw0 = s.yawRad, dist0 = s.distanceMeters;
    motion.stepInertia(s, 0.016);
    EXPECT_TRUE(s.settled);
    EXPECT_DOUBLE_EQ(s.yawRad, yaw0);
    EXPECT_DOUBLE_EQ(s.distanceMeters, dist0);
}

TEST(CameraMotion, DeterministicSameInputsSameOutput) {
    CameraMotion motion;
    CameraMotionState a;
    a.yawRateRadPerSec = 0.7;
    CameraMotionState b = a;
    const double dt[] = {0.016, 0.033, 0.016, 0.05, 0.008};
    for (double d : dt) {
        motion.stepInertia(a, d);
        motion.stepInertia(b, d);
    }
    EXPECT_DOUBLE_EQ(a.yawRad, b.yawRad);
    EXPECT_DOUBLE_EQ(a.pitchRad, b.pitchRad);
    EXPECT_DOUBLE_EQ(a.distanceMeters, b.distanceMeters);
    EXPECT_EQ(a.settled, b.settled);
}
