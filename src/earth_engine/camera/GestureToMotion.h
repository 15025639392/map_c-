#pragma once

namespace earth_engine {

/// 手势增量 → 相机运动速率（S6 输入侧 host 语义：北极星"组合手势各轴独立"）。
///
/// 输入每帧/每事件的手势增量，输出应喂给 CameraMotion 惯性步进的速率
/// （yaw/pitch/distance），即手势不做硬跳变、由惯性收敛（不跑飞）；
/// 映射为**线性且各轴独立**：拖动 dx → yawRate、dy → pitchRate、
/// 双指缩放 scale（>1 放大/拉近）→ distRate（负 = 拉近，正 = 拉远）。
/// 屏幕 y 与俯仰方向约定：向下拖动（dy>0 屏幕向下）→ 视角上抬（pitch 减小，
/// 俯仰向下为正）——与手势直觉一致；映射系数按屏幕高度归一（同样的像素拖动在
/// 高分辨率屏上的角速度一致）。
///
/// 纯函数、确定性；输出边界钳到给定上限（超速 → clamp 不放大）。
struct GestureToRatesParams {
    double yawRadPerPixel = 0.004;      // 参考屏幕高 1080 下每像素航向角
    double pitchRadPerPixel = 0.003;
    double distMetersPerScaleUnit = 2500.0; // 每"一档"缩放的距离变化
    double referenceScreenHeight = 1080.0;
    double maxYawRateRadPerSec = 2.0;   // 对应惯性模型速率上限（防跑飞）
    double maxPitchRateRadPerSec = 1.5;
    double maxDistRateMetersPerSec = 20000.0;
};

struct GestureRates {
    double yawRateRadPerSec = 0.0;
    double pitchRateRadPerSec = 0.0;
    double distRateMetersPerSec = 0.0;
};

class GestureToMotion {
public:
    explicit GestureToMotion(GestureToRatesParams params = GestureToRatesParams());

    /// @param dragDxPx / dragDyPx 本帧拖动增量（屏幕像素，y 向下为正）。
    /// @param pinchScale 双指缩放比（>1 = 放大拉近；==1 无缩放）。
    /// @param screenHeightPx 当前屏幕高（归一化像素灵敏度）。
    /// @param dtSeconds 本帧时长（增量 → 速率）。
    GestureRates compute(double dragDxPx, double dragDyPx, double pinchScale,
                         double screenHeightPx, double dtSeconds) const;

    const GestureToRatesParams& params() const { return params_; }

private:
    GestureToRatesParams params_;
};

} // namespace earth_engine
