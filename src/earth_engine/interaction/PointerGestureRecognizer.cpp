#include "earth_engine/interaction/PointerGestureRecognizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace earth_engine {
namespace interaction {

namespace {
bool isFinite(double v) { return std::isfinite(v); }
} // namespace

bool PointerGestureRecognizer::valid(const TouchEvent::Point& p) const {
    return isFinite(p.xPx) && isFinite(p.yPx);
}

double PointerGestureRecognizer::pairDistanceMeters(const TouchEvent::Point& a,
                                                    const TouchEvent::Point& b) const {
    const double dx = a.xPx - b.xPx;
    const double dy = a.yPx - b.yPx;
    return std::sqrt(dx * dx + dy * dy);
}

void PointerGestureRecognizer::addOrMove(const TouchEvent::Point& p) {
    if (count_ >= kMaxPointers || !valid(p)) {
        return;
    }
    Pointer& slot = pointers_[static_cast<size_t>(count_)];
    slot.xPx = p.xPx;
    slot.yPx = p.yPx;
    slot.live = true;
    ++count_;
}

GestureDelta PointerGestureRecognizer::onEvent(const TouchEvent& event) {
    if (event.action == TouchEvent::Action::Cancel) {
        reset();
        return GestureDelta();
    }

    // —— 触点增/删（只更新基线，不产增量；防跳变）——
    if (event.action == TouchEvent::Action::Down ||
        event.action == TouchEvent::Action::PointerDown) {
        for (const TouchEvent::Point& p : event.points) {
            addOrMove(p);
        }
        if (count_ >= 2) {
            pinchDistPrev_ = pairDistanceMeters(
                {pointers_[0].xPx, pointers_[0].yPx},
                {pointers_[1].xPx, pointers_[1].yPx});
        }
        GestureDelta d;
        d.anyPointerDown = count_ > 0;
        return d;
    }
    if (event.action == TouchEvent::Action::Up ||
        event.action == TouchEvent::Action::PointerUp) {
        const int idx = event.pointerIndex;
        if (idx >= 0 && idx < kMaxPointers && idx < count_ && pointers_[idx].live) {
            for (int i = idx; i < count_ - 1; ++i) {
                pointers_[i] = pointers_[i + 1];
            }
            pointers_[count_ - 1].live = false;
            --count_;
        }
        // 触点数变化 → 重建双指距离基线（升/落指不跳变）。
        if (count_ >= 2) {
            pinchDistPrev_ = pairDistanceMeters(
                {pointers_[0].xPx, pointers_[0].yPx},
                {pointers_[1].xPx, pointers_[1].yPx});
        } else {
            pinchDistPrev_ = -1.0;
        }
        GestureDelta d;
        d.anyPointerDown = count_ > 0;
        return d;
    }

    // —— Move：更新全部当前触点并计算增量 ——
    if (event.action == TouchEvent::Action::Move) {
        if (static_cast<int>(event.points.size()) != count_) {
            return GestureDelta(); // 批次不完整：忽略（防御）
        }
        double ddx = 0.0;
        double ddy = 0.0;
        bool dirty = false;
        for (int i = 0; i < count_; ++i) {
            const TouchEvent::Point& p = event.points[static_cast<size_t>(i)];
            if (!valid(p)) {
                continue;
            }
            Pointer& slot = pointers_[static_cast<size_t>(i)];
            ddx += p.xPx - slot.xPx;
            ddy += p.yPx - slot.yPx;
            slot.xPx = p.xPx;
            slot.yPx = p.yPx;
            dirty = true;
        }
        if (!dirty) {
            return GestureDelta();
        }
        GestureDelta d;
        d.anyPointerDown = true;
        if (count_ == 1) {
            // 单指 → 旋转增量。
            d.rotateDxPx = ddx;
            d.rotateDyPx = ddy;
        } else {
            // 多指 → 质心平移（平均位移）+ 捏合缩放（前两指距离比）。
            d.panDxPx = ddx / static_cast<double>(count_);
            d.panDyPx = ddy / static_cast<double>(count_);
            const double distNow = pairDistanceMeters(
                {pointers_[0].xPx, pointers_[0].yPx},
                {pointers_[1].xPx, pointers_[1].yPx});
            if (pinchDistPrev_ > 0.0 && distNow > 0.0) {
                d.pinchScale = distNow / pinchDistPrev_;
            }
            pinchDistPrev_ = distNow;
        }
        return d;
    }
    return GestureDelta();
}

void PointerGestureRecognizer::reset() {
    for (Pointer& p : pointers_) {
        p.live = false;
        p.xPx = 0.0;
        p.yPx = 0.0;
    }
    count_ = 0;
    pinchDistPrev_ = -1.0;
}

} // namespace interaction
} // namespace earth_engine
