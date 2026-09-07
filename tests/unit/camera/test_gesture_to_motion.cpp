// GestureToMotion：手势增量 → 相机运动速率（S6 输入侧：各轴独立、有界、确定性）。
#include <gtest/gtest.h>

#include <cmath>

#include "earth_engine/camera/GestureToMotion.h"

using namespace earth_engine;

TEST(GestureToMotion, ZeroInputZeroRates) {
    GestureToMotion g;
    const auto r = g.compute(0.0, 0.0, 1.0, 1080.0, 0.016);
    EXPECT_DOUBLE_EQ(r.yawRateRadPerSec, 0.0);
    EXPECT_DOUBLE_EQ(r.pitchRateRadPerSec, 0.0);
    EXPECT_DOUBLE_EQ(r.distRateMetersPerSec, 0.0);
}

TEST(GestureToMotion, DragAxesIndependent) {
    GestureToMotion g;
    // 纯横向拖动 → 只动 yaw（pitch/dist 不动）。
    const auto r = g.compute(100.0, 0.0, 1.0, 1080.0, 0.016);
    EXPECT_GT(r.yawRateRadPerSec, 0.0);
    EXPECT_DOUBLE_EQ(r.pitchRateRadPerSec, 0.0);
    EXPECT_DOUBLE_EQ(r.distRateMetersPerSec, 0.0);
    // 纯纵向拖动 → 只动 pitch；且下拖（dy>0）→ pitch 负（上抬）。
    const auto v = g.compute(0.0, 100.0, 1.0, 1080.0, 0.016);
    EXPECT_DOUBLE_EQ(v.yawRateRadPerSec, 0.0);
    EXPECT_LT(v.pitchRateRadPerSec, 0.0);
    EXPECT_DOUBLE_EQ(v.distRateMetersPerSec, 0.0);
}

TEST(GestureToMotion, PinchZoomOnlyAffectsDistance) {
    GestureToMotion g;
    // 放大（scale>1）→ 拉近（dist 负速率）。
    const auto in = g.compute(0.0, 0.0, 1.2, 1080.0, 0.016);
    EXPECT_LT(in.distRateMetersPerSec, 0.0);
    EXPECT_DOUBLE_EQ(in.yawRateRadPerSec, 0.0);
    EXPECT_DOUBLE_EQ(in.pitchRateRadPerSec, 0.0);
    const auto out_ = g.compute(0.0, 0.0, 0.8, 1080.0, 0.016); // 缩小 → 拉远
    EXPECT_GT(out_.distRateMetersPerSec, 0.0);
}

TEST(GestureToMotion, RatesBoundedAgainstRunaway) {
    GestureToMotion g;
    // 极端输入仍被各轴上界钳住（惯性模型同款上限，防跑飞）。
    const auto r = g.compute(1e6, 1e6, 1000.0, 1080.0, 0.000001);
    EXPECT_LE(std::abs(r.yawRateRadPerSec), g.params().maxYawRateRadPerSec + 1e-9);
    EXPECT_LE(std::abs(r.pitchRateRadPerSec), g.params().maxPitchRateRadPerSec + 1e-9);
    EXPECT_LE(std::abs(r.distRateMetersPerSec),
              g.params().maxDistRateMetersPerSec + 1e-9);
    EXPECT_TRUE(std::isfinite(r.yawRateRadPerSec));
}

TEST(GestureToMotion, ScreenHeightNormalization) {
    GestureToMotion g;
    const auto low = g.compute(2.0, 0.0, 1.0, 1080.0, 0.016);  // 参考高（低于 clamp）
    const auto high = g.compute(2.0, 0.0, 1.0, 2160.0, 0.016); // 2× 屏高
    EXPECT_GT(low.yawRateRadPerSec, high.yawRateRadPerSec); // 同像素在高分屏角速率更小
    EXPECT_NEAR(low.yawRateRadPerSec / high.yawRateRadPerSec, 2.0, 1e-9);
}

TEST(GestureToMotion, Deterministic) {
    GestureToMotion g;
    const auto a = g.compute(33.0, -12.0, 1.05, 1440.0, 0.016);
    const auto b = g.compute(33.0, -12.0, 1.05, 1440.0, 0.016);
    EXPECT_DOUBLE_EQ(a.yawRateRadPerSec, b.yawRateRadPerSec);
    EXPECT_DOUBLE_EQ(a.pitchRateRadPerSec, b.pitchRateRadPerSec);
    EXPECT_DOUBLE_EQ(a.distRateMetersPerSec, b.distRateMetersPerSec);
}
