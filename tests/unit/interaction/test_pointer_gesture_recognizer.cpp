// PointerGestureRecognizer：平台无关多点触摸 → 相机手势增量（S6 输入层 host 语义）。
// 覆盖：单指旋转增量/二指质心平移+捏合缩放/触点数变化防跳变/释放最后指/取消复位/
// 脏输入忽略/确定性。全部纯函数（无平台依赖）。
#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "earth_engine/interaction/PointerGestureRecognizer.h"

using namespace earth_engine::interaction;

namespace {

TouchEvent::Point pt(double x, double y) {
    TouchEvent::Point p;
    p.xPx = x;
    p.yPx = y;
    return p;
}

} // namespace

TEST(PointerGestureRecognizer, SingleFingerDragYieldsRotateDelta) {
    PointerGestureRecognizer rec;
    TouchEvent down;
    down.action = TouchEvent::Action::Down;
    down.points = {pt(100.0, 200.0)};
    const GestureDelta d0 = rec.onEvent(down);
    EXPECT_TRUE(d0.anyPointerDown);
    EXPECT_DOUBLE_EQ(d0.rotateDxPx, 0.0); // 落点只建基线

    TouchEvent move;
    move.action = TouchEvent::Action::Move;
    move.points = {pt(140.0, 260.0)};
    const GestureDelta d1 = rec.onEvent(move);
    EXPECT_DOUBLE_EQ(d1.rotateDxPx, 40.0);
    EXPECT_DOUBLE_EQ(d1.rotateDyPx, 60.0);
    EXPECT_DOUBLE_EQ(d1.panDxPx, 0.0);
    EXPECT_DOUBLE_EQ(d1.pinchScale, 1.0);
    EXPECT_TRUE(d1.anyPointerDown);

    // 再移一帧：增量相对上一位置。
    move.points = {pt(150.0, 260.0)};
    const GestureDelta d2 = rec.onEvent(move);
    EXPECT_DOUBLE_EQ(d2.rotateDxPx, 10.0);
    EXPECT_DOUBLE_EQ(d2.rotateDyPx, 0.0);
}

TEST(PointerGestureRecognizer, TwoFingerYieldsPanAndPinch) {
    PointerGestureRecognizer rec;
    TouchEvent down;
    down.action = TouchEvent::Action::Down;
    down.points = {pt(100.0, 200.0)};
    rec.onEvent(down);
    TouchEvent pd;
    pd.action = TouchEvent::Action::PointerDown;
    pd.points = {pt(300.0, 200.0)}; // 第二指（双指水平距离 200）
    const GestureDelta pd0 = rec.onEvent(pd);
    EXPECT_DOUBLE_EQ(pd0.pinchScale, 1.0); // 落双指只建基线

    // 两指整体右移 10px 且张开：A(100,200)→(120,200)、B(300,200)→(330,200)：dist 200→210
    TouchEvent move;
    move.action = TouchEvent::Action::Move;
    move.points = {pt(120.0, 200.0), pt(330.0, 200.0)};
    const GestureDelta m = rec.onEvent(move);
    EXPECT_NEAR(m.panDxPx, 25.0, 1e-9); // 平均位移 (20+30)/2
    EXPECT_DOUBLE_EQ(m.panDyPx, 0.0);
    EXPECT_NEAR(m.pinchScale, 210.0 / 200.0, 1e-9); // 张指 >1
    EXPECT_DOUBLE_EQ(m.rotateDxPx, 0.0);            // 多指不产旋转
    EXPECT_TRUE(m.anyPointerDown);
}

TEST(PointerGestureRecognizer, PointerUpBaselineNoJump) {
    PointerGestureRecognizer rec;
    // 双指 → 抬起第二指 → 剩余单指继续移动：增量连续（基线重建无跳变）。
    TouchEvent down;
    down.action = TouchEvent::Action::Down;
    down.points = {pt(100.0, 100.0)};
    rec.onEvent(down);
    TouchEvent pd;
    pd.action = TouchEvent::Action::PointerDown;
    pd.points = {pt(200.0, 100.0)};
    rec.onEvent(pd);

    TouchEvent up2;
    up2.action = TouchEvent::Action::PointerUp;
    up2.pointerIndex = 1;
    up2.points = {pt(200.0, 100.0)};
    const GestureDelta u = rec.onEvent(up2);
    EXPECT_FALSE(u.panDxPx != 0.0 || u.rotateDxPx != 0.0 || u.pinchScale != 1.0);
    EXPECT_EQ(rec.pointerCount(), 1);

    TouchEvent move;
    move.action = TouchEvent::Action::Move;
    move.points = {pt(120.0, 140.0)};
    const GestureDelta m = rec.onEvent(move);
    EXPECT_DOUBLE_EQ(m.rotateDxPx, 20.0); // 相对剩余单指基线，无历史残留
    EXPECT_DOUBLE_EQ(m.rotateDyPx, 40.0);
}

TEST(PointerGestureRecognizer, LastFingerUpReleasesAndResets) {
    PointerGestureRecognizer rec;
    TouchEvent down;
    down.action = TouchEvent::Action::Down;
    down.points = {pt(100.0, 100.0)};
    rec.onEvent(down);
    TouchEvent up;
    up.action = TouchEvent::Action::Up;
    up.pointerIndex = 0;
    up.points = {pt(100.0, 100.0)};
    const GestureDelta u = rec.onEvent(up);
    EXPECT_FALSE(u.anyPointerDown);
    EXPECT_EQ(rec.pointerCount(), 0);
    EXPECT_FALSE(rec.hasPointer());

    // 释放后新的 Down 从零基线开始。
    TouchEvent down2;
    down2.action = TouchEvent::Action::Down;
    down2.points = {pt(10.0, 10.0)};
    rec.onEvent(down2);
    TouchEvent move;
    move.action = TouchEvent::Action::Move;
    move.points = {pt(15.0, 10.0)};
    const GestureDelta m = rec.onEvent(move);
    EXPECT_DOUBLE_EQ(m.rotateDxPx, 5.0);
}

TEST(PointerGestureRecognizer, CancelResetsState) {
    PointerGestureRecognizer rec;
    TouchEvent down;
    down.action = TouchEvent::Action::Down;
    down.points = {pt(100.0, 100.0)};
    rec.onEvent(down);
    TouchEvent cancel;
    cancel.action = TouchEvent::Action::Cancel;
    const GestureDelta c = rec.onEvent(cancel);
    EXPECT_EQ(rec.pointerCount(), 0);
    EXPECT_DOUBLE_EQ(c.rotateDxPx, 0.0);
    // 复位后可正常重新识别。
    TouchEvent down2;
    down2.action = TouchEvent::Action::Down;
    down2.points = {pt(1.0, 1.0)};
    rec.onEvent(down2);
    EXPECT_EQ(rec.pointerCount(), 1);
}

TEST(PointerGestureRecognizer, NaNPointIgnoredNotFatal) {
    PointerGestureRecognizer rec;
    TouchEvent down;
    down.action = TouchEvent::Action::Down;
    down.points = {pt(100.0, 100.0)};
    rec.onEvent(down);
    TouchEvent bad;
    bad.action = TouchEvent::Action::Move;
    bad.points = {pt(std::numeric_limits<double>::quiet_NaN(), 0.0)};
    const GestureDelta b = rec.onEvent(bad);
    EXPECT_DOUBLE_EQ(b.rotateDxPx, 0.0); // 脏输入忽略本帧
    EXPECT_EQ(rec.pointerCount(), 1);
    // 后续正常输入仍可用。
    TouchEvent move;
    move.action = TouchEvent::Action::Move;
    move.points = {pt(110.0, 100.0)};
    const GestureDelta m = rec.onEvent(move);
    EXPECT_DOUBLE_EQ(m.rotateDxPx, 10.0);
}

TEST(PointerGestureRecognizer, DeterministicSameInputsSameOutputs) {
    const auto run = []() {
        PointerGestureRecognizer rec;
        TouchEvent down;
        down.action = TouchEvent::Action::Down;
        down.points = {pt(100.0, 100.0)};
        rec.onEvent(down);
        TouchEvent pd;
        pd.action = TouchEvent::Action::PointerDown;
        pd.points = {pt(300.0, 200.0)};
        rec.onEvent(pd);
        std::vector<double> out;
        for (int i = 0; i < 20; ++i) {
            TouchEvent move;
            move.action = TouchEvent::Action::Move;
            move.points = {pt(100.0 + i, 100.0 + 2 * i),
                           pt(300.0 - i, 200.0 + i)};
            const GestureDelta d = rec.onEvent(move);
            out.push_back(d.panDxPx);
            out.push_back(d.panDyPx);
            out.push_back(d.pinchScale);
        }
        return out;
    };
    const std::vector<double> a = run();
    const std::vector<double> b = run();
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_DOUBLE_EQ(a[i], b[i]);
    }
}
