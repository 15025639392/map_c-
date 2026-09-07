#include "earth_engine/camera/GestureToMotion.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

namespace {
double clampAbs(double v, double bound) { return std::clamp(v, -bound, bound); }
} // namespace

GestureToMotion::GestureToMotion(GestureToRatesParams params) : params_(params) {}

GestureRates GestureToMotion::compute(double dragDxPx, double dragDyPx, double pinchScale,
                                     double screenHeightPx, double dtSeconds) const {
    GestureRates out;
    if (dtSeconds <= 0.0) {
        return out;
    }
    // 屏幕高归一：同样的像素增量在高屏上产生更小角速率（参考高 1080）。
    const double norm = std::max(1.0, screenHeightPx / params_.referenceScreenHeight);

    // 拖动 → 角速率（像素增量 / 帧时）。
    out.yawRateRadPerSec =
        (dragDxPx / norm) * params_.yawRadPerPixel / dtSeconds; // 右拖 → 右转（正）
    out.pitchRateRadPerSec =
        -(dragDyPx / norm) * params_.pitchRadPerPixel / dtSeconds; // 下拖 → 上抬（pitch 减）

    // 缩放 → 距离速率：scale>1 拉近（负速率）；对数值线性映射。
    if (pinchScale > 0.0 && std::abs(pinchScale - 1.0) > 1e-6) {
        const double logScale = std::log(pinchScale);
        out.distRateMetersPerSec = -logScale * params_.distMetersPerScaleUnit / dtSeconds;
    }

    // 各轴独立钳制（互不影响；超速 clamp 不放大）。
    out.yawRateRadPerSec = clampAbs(out.yawRateRadPerSec, params_.maxYawRateRadPerSec);
    out.pitchRateRadPerSec =
        clampAbs(out.pitchRateRadPerSec, params_.maxPitchRateRadPerSec);
    out.distRateMetersPerSec =
        clampAbs(out.distRateMetersPerSec, params_.maxDistRateMetersPerSec);
    return out;
}

} // namespace earth_engine
