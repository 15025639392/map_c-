#include "earth_engine/camera/Frustum.h"

#include <cmath>

namespace earth_engine {

namespace {

// 由顶点 P 与"外法线方向"建平面。
Plane sidePlane(const Vec3& position, const Vec3& outwardNormal) {
    return Plane::fromPointAndNormal(position, outwardNormal);
}

} // namespace

Frustum Frustum::fromCamera(const CameraView& camera, double nearDistanceMeters) {
    const Vec3 fwd = camera.forward();
    const Vec3 right = camera.right();
    const Vec3 up = camera.cameraUp();
    const Vec3 pos = camera.position();
    const double tanHalfY = std::tan(camera.fovYRadians() * 0.5);
    const double tanHalfX = tanHalfY * camera.aspect();

    // 边界方向：F ± 侧向·tan(半角)，再归一化。
    const Vec3 leftDir = (fwd - right * tanHalfX).normalized();
    const Vec3 rightDir = (fwd + right * tanHalfX).normalized();
    const Vec3 topDir = (fwd + up * tanHalfY).normalized();
    const Vec3 bottomDir = (fwd - up * tanHalfY).normalized();

    Frustum f;
    // 外法线推导（fwd=-Z, up=+Y, right=+X 验证）：左=up×leftDir、右=rightDir×up、
    // 上=right×topDir、下=bottomDir×right。
    f.side_[0] = sidePlane(pos, up.cross(leftDir));
    f.side_[1] = sidePlane(pos, rightDir.cross(up));
    f.side_[2] = sidePlane(pos, right.cross(topDir));
    f.side_[3] = sidePlane(pos, bottomDir.cross(right));

    if (nearDistanceMeters > 0.0) {
        const Vec3 pointOnPlane = pos + fwd * nearDistanceMeters;
        f.near_ = Plane(-fwd, fwd.dot(pointOnPlane));
    }
    return f;
}

bool Frustum::intersectsSphere(const Vec3& center, double radius) const {
    for (const Plane& p : side_) {
        if (p.distanceTo(center) > radius) {
            return false; // 完全在平面外
        }
    }
    if (near_ && near_->distanceTo(center) > radius) {
        return false;
    }
    return true;
}

bool Frustum::containsPoint(const Vec3& point) const {
    for (const Plane& p : side_) {
        if (p.distanceTo(point) > 0.0) {
            return false;
        }
    }
    if (near_ && near_->distanceTo(point) > 0.0) {
        return false;
    }
    return true;
}

} // namespace earth_engine
