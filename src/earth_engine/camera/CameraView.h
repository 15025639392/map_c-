#pragma once

#include <optional>

#include "../core/geodesy/Ellipsoid.h"
#include "../core/math/Ray.h"
#include "../core/math/Rectangle.h"
#include "../core/math/Vec3.h"

namespace earth_engine {

/// 相机视图（位置/朝向 + 透视参数），地球引擎的地表相机地基。
/// 不处理手势/惯性——那是交互模块；这里只把"位姿 → 射线/地表覆盖"钉死，
/// 供拾取与将来视锥驱动瓦片选择复用。
class CameraView {
public:
    /// (positionEcef, targetEcef, upHintEcef, fovYRadians(垂直), aspect=宽/高)。
    /// upHint 不必与视线垂直，会被施密特正交化。
    CameraView(const Vec3& positionEcef, const Vec3& targetEcef, const Vec3& upHintEcef,
               double fovYRadians, double aspect);

    const Vec3& position() const { return position_; }
    double fovYRadians() const { return fovYRadians_; }
    double aspect() const { return aspect_; }

    /// 归一化视线方向（position → target）。
    Vec3 forward() const { return forward_; }
    /// 右方向（单位）。
    Vec3 right() const { return right_; }
    /// 上方向（单位，≈ 正交化后的 upHint）。
    Vec3 cameraUp() const { return cameraUp_; }

    /// NDC 屏幕坐标（x,y ∈ [-1,1]；y 向上）→ 射线（方向已归一化）。
    Ray rayThroughNdc(double ndcX, double ndcY) const;

    /// 视锥四角射线打到参考椭球面的**地表脚印矩形**（经纬，弧度）。
    /// 任一角射线未命中椭球（看向太空）返回 nullopt。
    /// 不处理跨反经线矩形（当前机位假设在 [−π, π) 中部）。
    std::optional<Rectangle> groundFootprintRadians(const Ellipsoid& ellipsoid) const;

private:
    Vec3 position_;
    Vec3 forward_;
    Vec3 right_;
    Vec3 cameraUp_;
    double fovYRadians_;
    double aspect_;
    double tanHalfFovY_;
};

} // namespace earth_engine
