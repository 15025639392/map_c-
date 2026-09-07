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

// ---------------------------------------------------------------------------
// L3 slice B：中心平移（pan）——ENU 地面投影 / 惯性收敛 / 移入高地贴地抬升 /
// pan×rotate×zoom 组合 / 确定性。
// ---------------------------------------------------------------------------
TEST(MapCameraSystem, PanRightwardDragMovesCenterWestAndSettles) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 0.0)); // 朝北望
    cam.setGroundFn(flatGround(0.0));
    const double lon0 = cam.pose().lonRad;
    const double lat0 = cam.pose().latRad;
    // 持续向右拖动（内容跟随手指 → 中心西移，lon 减小）。
    for (int i = 0; i < 40; ++i) {
        cam.setPanGesture(3.0, 0.0, 1080.0);
        cam.step(0.016);
        EXPECT_LE(cam.pose().lonRad + 1e-12, lon0); // 单调不东移
        EXPECT_TRUE(std::isfinite(cam.pose().latRad));
    }
    EXPECT_LT(cam.pose().lonRad, lon0 - 1e-4); // 明显西移
    EXPECT_NEAR(cam.pose().latRad, lat0, 1e-9); // 纯横向拖动不改纬度（yaw0）
    // 抬手后惯性收敛。
    int steps = 0;
    for (; steps < 40000 && !cam.isSettled(); ++steps) {
        cam.step(0.016);
    }
    EXPECT_LT(steps, 40000);
    EXPECT_TRUE(cam.isSettled());
    EXPECT_TRUE(std::isfinite(cam.pose().lonRad));
}

TEST(MapCameraSystem, PanDisplacementIsProportionalToPixels) {
    const auto run = [](double dxPx) {
        MapCameraSystem cam;
        cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 0.0));
        cam.setGroundFn(flatGround(0.0));
        const double lon0 = cam.pose().lonRad;
        for (int i = 0; i < 30; ++i) {
            cam.setPanGesture(dxPx, 0.0, 1080.0);
            cam.step(0.016);
        }
        for (int i = 0; i < 4000 && !cam.isSettled(); ++i) {
            cam.step(0.016);
        }
        return lon0 - cam.pose().lonRad; // 西移量（正）
    };
    const double d1 = run(1.0);
    const double d2 = run(2.0);
    EXPECT_GT(d1, 0.0);
    EXPECT_GT(d2, 0.0);
    // 速率线性于像素（同 dt/帧数；阻尼逐帧等比 → 位移比 ≈ 2）。
    EXPECT_NEAR(d2 / d1, 2.0, 0.4);
}

TEST(MapCameraSystem, PanIntoHigherGroundIsClampedToClearance) {
    MapCameraSystem::Params params; // 本用例只测平移×贴地语义：关 LOD 包络（恒 factor 1）
    params.lodSpeed.nearMeters = 0.0;
    params.lodSpeed.farMeters = 0.0;
    params.lodSpeed.minFactor = 1.0;
    MapCameraSystem cam(params);
    cam.setPose(makePose(106.40, 29.70, 600.0, 45.0, 0.0)); // 低地起手，高度 600
    // 地表：lon < 106.45 → 200m；lon ≥ 106.45 → 1500m（高地）。
    const double ridgeLonRad = degreesToRadians(106.45);
    cam.setGroundFn([ridgeLonRad](const Cartographic& c) -> std::optional<double> {
        return c.longitude() >= ridgeLonRad ? 1500.0 : 200.0;
    });
    // 向左拖动 → 中心东移进入高地（持续拖动直到越过界；逐帧送输入故不进 settled 早退）。
    for (int i = 0; i < 600; ++i) {
        cam.setPanGesture(-20.0, 0.0, 1080.0); // 向左拖 → 中心东移
        cam.step(0.016);
        if (cam.pose().lonRad > ridgeLonRad) {
            break;
        }
    }
    EXPECT_GT(cam.pose().lonRad, ridgeLonRad); // 已进入高地
    // 高度已被抬到 1500+5 净空之上（贴地防护随中心走）。
    EXPECT_GE(cam.pose().altitudeMeters, 1500.0 + cam.params().minClearanceMeters - 1e-9);
    EXPECT_TRUE(std::isfinite(cam.pose().altitudeMeters));
}

TEST(MapCameraSystem, PanRotateZoomCombinedFrame) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, 15000.0, 45.0, 0.0));
    cam.setGroundFn(flatGround(0.0));
    const MapCameraSystem::Pose p0 = cam.pose();
    for (int i = 0; i < 60; ++i) {
        cam.setGesture(1.0, 0.5, 1.002, 1080.0); // 旋转 + 轻微拉近
        cam.setPanGesture(1.5, -1.0, 1080.0);    // 同帧平移（右上）
        cam.step(0.016);
        const MapCameraSystem::Pose p = cam.pose();
        EXPECT_TRUE(std::isfinite(p.lonRad) && std::isfinite(p.latRad) &&
                    std::isfinite(p.altitudeMeters) && std::isfinite(p.headingRad) &&
                    std::isfinite(p.pitchRad));
    }
    const MapCameraSystem::Pose p1 = cam.pose();
    // 三轴确实都动了（中心/高度/朝向均变化）。
    EXPECT_TRUE(std::fabs(p1.lonRad - p0.lonRad) > 1e-8 ||
                std::fabs(p1.latRad - p0.latRad) > 1e-8);
    EXPECT_NE(p1.altitudeMeters, p0.altitudeMeters);
    EXPECT_NE(p1.headingRad, p0.headingRad);
    // 抬手后收敛。
    int steps = 0;
    for (; steps < 40000 && !cam.isSettled(); ++steps) {
        cam.step(0.016);
    }
    EXPECT_LT(steps, 40000);
    EXPECT_TRUE(cam.isSettled());
}

TEST(MapCameraSystem, PanDeterministicSameInputsSamePose) {
    const auto run = []() {
        MapCameraSystem cam;
        cam.setPose(makePose(106.44, 29.70, 10000.0, 50.0, 30.0));
        cam.setGroundFn(flatGround(120.0));
        const double dx[] = {0.0, 2.0, -3.0, 5.0};
        const double dy[] = {-1.0, 0.0, 2.0, -4.0};
        for (int i = 0; i < 90; ++i) {
            const int k = i % 4;
            cam.setPanGesture(dx[k], dy[k], 1080.0);
            cam.step(0.016);
        }
        for (int i = 0; i < 4000 && !cam.isSettled(); ++i) {
            cam.step(0.016);
        }
        return cam.pose();
    };
    const MapCameraSystem::Pose a = run();
    const MapCameraSystem::Pose b = run();
    EXPECT_DOUBLE_EQ(a.lonRad, b.lonRad);
    EXPECT_DOUBLE_EQ(a.latRad, b.latRad);
    EXPECT_DOUBLE_EQ(a.altitudeMeters, b.altitudeMeters);
    EXPECT_DOUBLE_EQ(a.headingRad, b.headingRad);
    EXPECT_DOUBLE_EQ(a.pitchRad, b.pitchRad);
}

// ---------------------------------------------------------------------------
// L3 线②：LOD 感知灵敏度包络（近地面操纵降速；远距离全速）。
// ---------------------------------------------------------------------------
namespace {
/// 相同旋转拖动序列的总航向变化（起于 heading 0；不经 settle，避免衰减尾部差异）。
double runRotateAtAltitude(double altMeters) {
    MapCameraSystem cam;
    cam.setPose(makePose(106.44, 29.70, altMeters, 45.0, 0.0));
    cam.setGroundFn(flatGround(0.0));
    for (int i = 0; i < 30; ++i) {
        cam.setGesture(3.0, 0.0, 1.0, 1080.0); // 等量拖动
        cam.step(0.016);
    }
    double h = cam.pose().headingRad;
    if (h > 3.141592653589793) {
        h -= 6.283185307179586;
    }
    return std::fabs(h);
}
} // namespace

TEST(MapCameraSystem, LodEnvelopeSlowsRotationNearGround) {
    const double nearAlt = 1200.0; // ≤ lodSpeed.near(3000) → minFactor 0.35
    const double farAlt = 50000.0; // ≥ lodSpeed.far(8000) → 1.0
    const double nearDelta = runRotateAtAltitude(nearAlt);
    const double farDelta = runRotateAtAltitude(farAlt);
    EXPECT_GT(farDelta, 0.0);
    EXPECT_GT(nearDelta, 0.0);
    // 同一拖动序列：近地面航向增量 ≈ minFactor × 远距离增量。
    EXPECT_NEAR(nearDelta / farDelta, 0.35, 0.05);
}

TEST(MapCameraSystem, LodEnvelopeConfigurable) {
    MapCameraSystem::Params params;
    params.lodSpeed.nearMeters = 5000.0;
    params.lodSpeed.farMeters = 10000.0;
    params.lodSpeed.minFactor = 0.5;
    auto run = [&params](double altMeters) {
        MapCameraSystem cam(params);
        cam.setPose(makePose(106.44, 29.70, altMeters, 45.0, 0.0));
        cam.setGroundFn(flatGround(0.0));
        for (int i = 0; i < 30; ++i) {
            cam.setGesture(2.0, 0.0, 1.0, 1080.0);
            cam.step(0.016);
        }
        double h = cam.pose().headingRad;
        if (h > 3.141592653589793) {
            h -= 6.283185307179586;
        }
        return std::fabs(h);
    };
    const double low = run(2000.0); // < near5000 → 0.5
    const double high = run(30000.0); // > far10000 → 1.0
    EXPECT_GT(high, 0.0);
    EXPECT_NEAR(low / high, 0.5, 0.05);
}
