#pragma once

#include <vector>

namespace earth_engine {
namespace interaction {

/// 单次触摸事件的识别输出（语义与 MapCameraSystem 输入对齐）：
/// - 单指移动 → rotate 增量（屏幕右/x+、下/y+）；
/// - 多指移动 → pan 增量（全部活动指平均位移）+ pinchScale（前两指距离比，>1=张指拉近）；
/// - 触点数量变化事件（Down/PointerDown/Up…）输出空增量（只更新基线防跳变）。
struct GestureDelta {
    double rotateDxPx = 0.0;
    double rotateDyPx = 0.0;
    double panDxPx = 0.0;
    double panDyPx = 0.0;
    double pinchScale = 1.0; // 1 = 无缩放
    bool anyPointerDown = false; // 是否有指按住（可用于"按住制动/输入态"判定）
};

/// 一次平台触摸事件（Android MotionEvent 语义同构；points 携带事件所含全部触点）。
struct TouchEvent {
    enum class Action { Down, Move, PointerDown, PointerUp, Up, Cancel };
    Action action = Action::Cancel;
    /// Down/PointerDown/PointerUp/Up 的目标指 index（Move 忽略；points 顺序索引）。
    int pointerIndex = 0;
    struct Point {
        double xPx = 0.0;
        double yPx = 0.0;
    };
    std::vector<Point> points; // Move：全部当前触点新位置（顺序 index）；Down/Up：单点
};

/// 平台无关的多点触摸流 → 相机/导航手势增量识别器（S6 输入层 / S11 事件基，host 语义）。
///
/// - 无平台依赖：只消费 (action, 触点批次) 序列；确定性：同输入序列 → 同输出；
/// - 防跳变：触点数量变化事件只更新基线（Down 落点/PointerDown 双指距/PointerUp 后单指
///   位移基线），不产生增量；
/// - 语义映射：1 指移动 = rotate（照相机旋转意图）；≥2 指移动 = 质心 pan + 双指距离比
///   pinchScale（与 demo 既有"单指转视角/双指平移+缩放"一致，见 MapCameraSystem 输入）；
/// - 脏输入（NaN）忽略该触点本次更新；Cancel/Up(最后指) 复位距离基线并清空活动集。
class PointerGestureRecognizer {
public:
    /// 处理一个触摸事件，返回本次事件的增量（增量是**事件级**的，调用方负责跨帧累计）。
    GestureDelta onEvent(const TouchEvent& event);

    bool hasPointer() const { return count_ > 0; }
    int pointerCount() const { return count_; }
    /// 全复位（表面丢失/切后台）。
    void reset();

private:
    struct Pointer {
        double xPx = 0.0;
        double yPx = 0.0;
        bool live = false;
    };
    static constexpr int kMaxPointers = 16;

    void addOrMove(const TouchEvent::Point& p);
    double pairDistanceMeters(const TouchEvent::Point& a,
                              const TouchEvent::Point& b) const;
    bool valid(const TouchEvent::Point& p) const;

    std::vector<Pointer> pointers_{static_cast<size_t>(kMaxPointers)};
    int count_ = 0;
    double pinchDistPrev_ = -1.0; // 前两指最近一次距离（事件间基线）
};

} // namespace interaction
} // namespace earth_engine
