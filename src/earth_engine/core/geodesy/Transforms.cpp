#include "earth_engine/core/geodesy/Transforms.h"

#include <cmath>

namespace earth_engine {
namespace Transforms {

namespace {

/// 构造 ENU 局部帧的旋转部分 Rᵀ（ECEF→ENU 用），列主序。
/// R 的列 = [east north up]；Rᵀ 的列 = (east_c, north_c, up_c), c∈{x,y,z}。
Mat4 inverseRotationColumns(const Vec3& east, const Vec3& north, const Vec3& up) {
    return Mat4(east.x(), north.x(), up.x(), 0.0,
                east.y(), north.y(), up.y(), 0.0,
                east.z(), north.z(), up.z(), 0.0,
                0.0, 0.0, 0.0, 1.0);
}

} // namespace

Mat4 eastNorthUpToFixedFrame(const Cartographic& origin, const Ellipsoid& ellipsoid) {
    const double lon = origin.longitude();

    // up = 椭球面大地法线（只依赖经纬，与高度无关）。
    const Vec3 up = ellipsoid.geodeticSurfaceNormal(origin);
    // east = 沿经度增大的切线方向（恒在赤道面内，单位向量）。
    const Vec3 east(-std::sin(lon), std::cos(lon), 0.0);
    // north = up × east（右手系：east × north = up）。
    const Vec3 north = up.cross(east);

    const Vec3 originEcef = ellipsoid.cartographicToCartesian(origin);
    return Mat4(east, north, up, originEcef);
}

Mat4 fixedFrameToEastNorthUp(const Cartographic& origin, const Ellipsoid& ellipsoid) {
    const double lon = origin.longitude();

    const Vec3 up = ellipsoid.geodeticSurfaceNormal(origin);
    const Vec3 east(-std::sin(lon), std::cos(lon), 0.0);
    const Vec3 north = up.cross(east);
    const Vec3 originEcef = ellipsoid.cartographicToCartesian(origin);

    // 旋转部分 = Rᵀ（正交阵逆 = 转置）；平移 = -Rᵀ · origin。
    const Mat4 rotationOnly = inverseRotationColumns(east, north, up);
    const Vec3 t = rotationOnly.transformDirection(originEcef);
    return Mat4(east.x(), north.x(), up.x(), 0.0,
                east.y(), north.y(), up.y(), 0.0,
                east.z(), north.z(), up.z(), 0.0,
                -t.x(), -t.y(), -t.z(), 1.0);
}

} // namespace Transforms
} // namespace earth_engine
