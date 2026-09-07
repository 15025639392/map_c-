#pragma once

#include <cmath>
#include <limits>

namespace earth_engine {

/// 屏幕空间误差（SSE）度量：地形/几何 LOD 细化的判定依据。
/// 定义：一段真实长度 geometricError（米）在屏幕上占多少像素。
///
/// sse(px) = geometricError / (2 · distance · tan(fov/2)) · viewportHeightPx
///
/// 几何推导：垂直视场 fov 张满 viewportHeight 像素；在 distance 处视场覆盖的
/// 垂直弧长约 2·distance·tan(fov/2)（米）→ 米/像素 = 2d·tan(fov/2)/viewportHeight；
/// 误差米数 ÷ 米/像素 = 误差像素数。
namespace QuadtreeGeometricError {

/// 计算屏幕空间误差（像素）。
/// 约定：distance <= 0 或 geometricError < 0 视为无效，返回 +inf（触发细化，保守方向）；
/// viewportHeightPx <= 0 返回 0（无屏幕可消费）。geometricError == 0 返回 0。
double screenSpaceError(double geometricErrorMeters, double distanceMeters,
                        double viewportHeightPx, double fovRadians);

/// 细化判定：sse 超过阈值（像素）就继续细化（返回 true）。
inline bool shouldRefine(double geometricErrorMeters, double distanceMeters,
                         double viewportHeightPx, double fovRadians,
                         double maximumScreenSpaceErrorPx) {
    if (geometricErrorMeters <= 0.0) {
        return false; // 零误差：没有细化动机
    }
    if (distanceMeters <= 0.0 || fovRadians <= 0.0) {
        return true; // 视线退化/参数无效：保守细化
    }
    const double sse = screenSpaceError(geometricErrorMeters, distanceMeters,
                                        viewportHeightPx, fovRadians);
    return sse > maximumScreenSpaceErrorPx;
}

} // namespace QuadtreeGeometricError

} // namespace earth_engine
