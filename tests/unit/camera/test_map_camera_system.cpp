// MapCameraSystem：引擎层地图相机系统（俯视相机 + 惯性 + 贴地防护 + flyTo）。
// 覆盖：位姿往返/边界、拖动→惯性收敛、按住制动、贴地钳制、无地表约束回落、
// flyTo 钉死、手势打断 fly、确定性、脏输入免疫。全部 host 纯函数（无平台依赖）。
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>

#include "earth_engine/camera/MapCameraSystem.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

MapCameraSystem::GroundHeightFn flatGround(double h) {
    return [h](const Cartographic&) -> std::optional<double> { return h; };
}

MapCameraSystem::Pose makePose(double lonDeg, double latDeg, double alt, double pitchDeg,
                               double headingDeg = 20.0) {
    MapCameraSystem::Pose p;
    p.lonRad = degreesToRadians(lonDeg);
    p.latRad = degreesToRadians(latDeg);
    p.altitudeMeters = alt;
    p.pitchRad = degreesToRadians(pitchDeg);
    p.headingRad = degreesToRadians(headingDeg);
    return p;
}

} // namespace

TEST(MapCameraSystem, SetPoseRoundTripAndBounds) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 20.0));
    const MapCameraSystem::Pose p = cam.pose();
    EXPECT_NEAR(p.lonRad, degreesToRadians(106.44), 1e-12);
    EXPECT_NEAR(p.latRad, degreesToRadians(29.70), 1e-12);
    EXPECT_DOUBLE_EQ(p.altitudeMeters, 15000.0);
    EXPECT_NEAR(p.pitchRad, degreesToRadians(45.0), 1e-12);
    EXPECT_NEAR(p.headingRad, degreesToRadians(20.0), 1e-12);
    EXPECT_TRUE(cam.isSettled());

    // 边界钳制：pitch 越界 & 高度越上限 → 收敛到模型界内。
    MapCameraSystem::Pose bad = makePose(0.0, 0.0, 5e8, 120.0);
    cam.setPose(bad);
    const MapCameraSystem::Pose clamped = cam.pose();
    EXPECT_LE(clamped.altitudeMeters, cam.params().maxAltitudeMeters + 1e-9);
    EXPECT_LE(clamped.pitchRad, cam.params().motion.maxPitchRad + 1e-12);
    EXPECT_GE(clamped.pitchRad, cam.params().motion.minPitchRad - 1e-12);
    EXPECT_TRUE(std::isfinite(clamped.headingRad));
}

TEST(MapCameraSystem, DragInertiaDecaysAndSettles) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 20.0));
    cam.setGroundFn(flatGround(0.0));
    // 向右下快速拖动一帧 → 抬手后惯性滑行，随后阻尼收敛、不跑飞。
    cam.setGesture(80.0, 60.0, 1.0, 1080.0);
    cam.step(0.016);
    EXPECT_FALSE(cam.isSettled()); // 有速率 → 未收敛
    int steps = 0;
    for (; steps < 40000 && !cam.isSettled(); ++steps) {
        cam.step(0.016); // 无新输入 → 纯惯性衰减
        const MapCameraSystem::Pose p = cam.pose();
        EXPECT_TRUE(std::isfinite(p.altitudeMeters) && std::isfinite(p.headingRad) &&
                    std::isfinite(p.pitchRad));
    }
    EXPECT_LT(steps, 40000);
    EXPECT_TRUE(cam.isSettled());
    // 收敛后姿态数值有限且高度有界（滑行全程不跑飞）。
    EXPECT_GE(cam.pose().altitudeMeters, 0.0);
    EXPECT_LE(cam.pose().altitudeMeters, cam.params().maxAltitudeMeters);
}

TEST(MapCameraSystem, HoldStillBrakesInertia) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 20.0));
    cam.setGroundFn(flatGround(0.0));
    cam.setGesture(80.0, 0.0, 1.0, 1080.0);
    for (int i = 0; i < 3; ++i) {
        cam.step(0.016);
    }
    // 按住（无增量）：速率清零 → 位置冻结。
    cam.setGesture(0.0, 0.0, 1.0, 1080.0);
    cam.step(0.016);
    cam.step(0.016);
    EXPECT_TRUE(cam.isSettled());
    const double alt = cam.pose().altitudeMeters;
    cam.step(0.016);
    cam.step(0.016);
    EXPECT_DOUBLE_EQ(cam.pose().altitudeMeters, alt);
}

TEST(MapCameraSystem, PinchInBelowGroundIsClampedToClearance) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 20.0));
    const double groundH = 700.0;
    cam.setGroundFn(flatGround(groundH));
    // 持续拉近（scale>1 = 放大/拉近）直至被贴地防护拦住。
    for (int i = 0; i < 40000 && !cam.isSettled(); ++i) {
        cam.setGesture(0.0, 0.0, 1.5, 1080.0);
        cam.step(0.016);
        if (i > 200) {
            break;
        }
    }
    // 无论惯性速率多大，最终高度不得低于 ground + clearance。
    const MapCameraSystem::Pose p = cam.pose();
    EXPECT_GE(p.altitudeMeters, groundH + cam.params().minClearanceMeters - 1e-9);
    EXPECT_TRUE(std::isfinite(p.altitudeMeters));
}

TEST(MapCameraSystem, NoGroundDataFallsBackToMinAltitude) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 5000.0, 45.0, 20.0));
    // 无 ground fn（模拟无地表数据）：持续拉近只受 minAltitude 兜底。
    for (int i = 0; i < 20000 && !cam.isSettled(); ++i) {
        cam.setGesture(0.0, 0.0, 1.5, 1080.0);
        cam.step(0.016);
        if (i > 300) {
            break;
        }
    }
    EXPECT_GE(cam.pose().altitudeMeters, cam.params().minAltitudeMeters - 1e-9);
}

TEST(MapCameraSystem, FlyToPinsTargetAndSettles) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 20.0));
    cam.setGroundFn(flatGround(0.0));
    cam.flyTo(makePose(106.44, 29.70, 3000.0, 70.0, 200.0));
    EXPECT_TRUE(cam.flying());
    int steps = 0;
    for (; steps < 20000 && cam.flying(); ++steps) {
        cam.step(0.016);
    }
    EXPECT_LT(steps, 20000);
    const MapCameraSystem::Pose p = cam.pose();
    EXPECT_NEAR(p.altitudeMeters, 3000.0, 1e-6);
    EXPECT_NEAR(p.pitchRad, degreesToRadians(70.0), 1e-6);
    EXPECT_NEAR(p.headingRad, degreesToRadians(200.0), 1e-6);
    EXPECT_TRUE(cam.isSettled());
}

TEST(MapCameraSystem, GestureInterruptsFlyTo) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 20.0));
    cam.flyTo(makePose(106.44, 29.70, 3000.0, 70.0, 200.0));
    cam.step(0.016);
    cam.setGesture(10.0, 0.0, 1.0, 1080.0);
    cam.step(0.016);
    EXPECT_FALSE(cam.flying()); // 手势接管 → flyTo 取消
    // 之后由惯性路径继续（此处只验证已取消 + 数值健康）。
    EXPECT_TRUE(std::isfinite(cam.pose().altitudeMeters));
}

TEST(MapCameraSystem, DeterministicSameInputsSamePose) {
    MapCameraSystem a;
    MapCameraSystem b;
    a.setPose(makePose(106.44, 29.70, 8000.0, 40.0, 30.0));
    b.setPose(makePose(106.44, 29.70, 8000.0, 40.0, 30.0));
    a.setGroundFn(flatGround(100.0));
    b.setGroundFn(flatGround(100.0));
    const double dx[] = {30.0, -12.0, 0.0, 55.0};
    const double dy[] = {-18.0, 9.0, 0.0, -30.0};
    const double scale[] = {1.0, 1.0, 1.02, 1.0};
    for (int i = 0; i < 120; ++i) {
        const int k = i % 4;
        a.setGesture(dx[k], dy[k], scale[k], 1080.0);
        b.setGesture(dx[k], dy[k], scale[k], 1080.0);
        a.step(0.016);
        b.step(0.016);
        const MapCameraSystem::Pose pa = a.pose();
        const MapCameraSystem::Pose pb = b.pose();
        EXPECT_DOUBLE_EQ(pa.altitudeMeters, pb.altitudeMeters);
        EXPECT_DOUBLE_EQ(pa.headingRad, pb.headingRad);
        EXPECT_DOUBLE_EQ(pa.pitchRad, pb.pitchRad);
    }
}

TEST(MapCameraSystem, NaNInputsAreIgnoredNotFatal) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 8000.0, 40.0, 30.0));
    cam.setGesture(std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0, 1080.0);
    cam.step(0.016);
    EXPECT_TRUE(std::isfinite(cam.pose().altitudeMeters));
    EXPECT_TRUE(std::isfinite(cam.pose().headingRad));
    // 脏输入被忽略 → 无速率 → 仍静止。
    EXPECT_TRUE(cam.isSettled());
}
