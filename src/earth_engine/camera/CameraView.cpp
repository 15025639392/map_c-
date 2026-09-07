#include "earth_engine/camera/CameraView.h"

#include <algorithm>
#include <cmath>

#include "earth_engine/core/geodesy/RayEllipsoid.h"

namespace earth_engine {

CameraView::CameraView(const Vec3& positionEcef, const Vec3& targetEcef, const Vec3& upHintEcef,
                       double fovYRadians, double aspect)
    : position_(positionEcef), fovYRadians_(fovYRadians), aspect_(aspect) {
    forward_ = (targetEcef - positionEcef).normalized();
    if (forward_.magnitudeSquared() <= 0.0) {
        forward_ = Vec3(0.0, 0.0, -1.0); // 退化输入兜底（位置==目标）
    }
    // 施密特正交化：right = normalize(forward × up)；up = right × forward。
    right_ = forward_.cross(upHintEcef).normalized();
    if (right_.magnitudeSquared() <= 0.0) {
        // 视线与 up 平行（竖直看极区等退化）：选任意垂直方向。
        right_ = Vec3(0.0, 1.0, 0.0);
        if (std::fabs(right_.dot(forward_)) > 0.99) {
            right_ = Vec3(1.0, 0.0, 0.0);
        }
        right_ = right_.cross(forward_).normalized();
    }
    cameraUp_ = right_.cross(forward_).normalized();
    tanHalfFovY_ = std::tan(fovYRadians_ * 0.5);
}

Ray CameraView::rayThroughNdc(double ndcX, double ndcY) const {
    // 相机空间方向 = fwd + right·(ndcX·aspect·tanHalf) + up·(ndcY·tanHalf)。
    const Vec3 dir = forward_ + right_ * (ndcX * aspect_ * tanHalfFovY_) +
                     cameraUp_ * (ndcY * tanHalfFovY_);
    return Ray(position_, dir.normalized());
}

std::optional<Rectangle> CameraView::groundFootprintRadians(const Ellipsoid& ellipsoid) const {
    double west = kPi;
    double south = kPiOverTwo;
    double east = -kPi;
    double north = -kPiOverTwo;
    int hits = 0;
    for (const double ny : {-1.0, 1.0}) {
        for (const double nx : {-1.0, 1.0}) {
            const Ray ray = rayThroughNdc(nx, ny);
            const std::optional<double> t = firstRayEllipsoidIntersection(ray, ellipsoid);
            if (!t) {
                return std::nullopt; // 某角看向太空 → 无完整地表脚印
            }
            const Cartographic hit = ellipsoid.cartesianToCartographic(ray.pointAt(t.value()));
            west = std::min(west, hit.longitude());
            east = std::max(east, hit.longitude());
            south = std::min(south, hit.latitude());
            north = std::max(north, hit.latitude());
            ++hits;
        }
    }
    if (hits != 4) {
        return std::nullopt;
    }
    return Rectangle(west, south, east, north);
}

} // namespace earth_engine
