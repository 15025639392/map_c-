#pragma once

#include <optional>

#include "Ellipsoid.h"
#include "../math/Ray.h"

namespace earth_engine {

/// 射线与参考椭球的相交结果。
/// 解 |O' + t·D'| = 1（O'、D' 为按椭球半径缩放的坐标），得到两个根 t_enter <= t_exit；
/// 根的语义是沿射线方向的参数 t（可为负，表示交点在射线起点之后/前）。
struct RayEllipsoidHit {
    /// 是否与椭球相交（判别式 >= 0；即缩放球体与直线至少相切）。
    bool hasHit = false;
    /// 判别式 < 0 时为真（射线完全错过椭球）。
    bool isMiss = true;
    /// 两个根（未命中时为 NaN）。
    double enterT = 0.0;
    double exitT = 0.0;

    /// 第一个 >= 0 的根（相机/拾取语义：沿射线向前看第一个交点）。
    /// 起点在椭球内部时返回正退出根（看向地表外时）；完全不命中返回 nullopt。
    std::optional<double> firstPositiveT() const {
        if (!hasHit) {
            return std::nullopt;
        }
        if (enterT >= 0.0) {
            return enterT;
        }
        if (exitT >= 0.0) {
            return exitT;
        }
        return std::nullopt;
    }
};

/// 射线与椭球求交。单位：t 与射线方向同尺度（方向非单位时 t 含义 = 方向倍数）。
/// 内部用缩放球法：x/a、y/a、z/b，退化到单位球解析二次方程。
RayEllipsoidHit intersectRayEllipsoid(const Ray& ray, const Ellipsoid& ellipsoid);

/// 便捷接口：沿射线正向第一个命中点（没有则 nullopt）。
/// 等价于 intersectRayEllipsoid(...).firstPositiveT()。
std::optional<double> firstRayEllipsoidIntersection(const Ray& ray, const Ellipsoid& ellipsoid);

} // namespace earth_engine
