#pragma once

#include "Cartographic.h"
#include "Ellipsoid.h"
#include "../math/Rectangle.h"
#include "../math/Vec2.h"

namespace earth_engine {

/// 投影抽象：Cartographic（弧度+米高）↔ 平面坐标（**米**，世界原点处）。
/// 瓦片方案（XYZ/TMS/WebMercator）在阶段 3 依此实现。
/// 约定：Projection 处理二维地表；投影丢弃高度，反投影高度恒 0
/// （带高度的点位请走 Transforms 的 ECEF 帧）。
class Projection {
public:
    virtual ~Projection() = default;

    /// 经纬（弧度）→ 平面（米）。
    virtual Vec2 project(const Cartographic& cartographic) const = 0;
    /// 平面（米）→ 经纬（弧度，高度 0）。
    virtual Cartographic unproject(const Vec2& positionMeters) const = 0;

    /// 在给定纬度处，投影坐标 y 对纬度的导数（米/弧度）。
    /// 用处：把"纬度增量 ↔ 投影米"互转、将来算地面分辨率/SSE 时避免重复微积分。
    virtual double northSouthMetersPerRadian(double latitudeRadians) const = 0;

    /// 投影的有效经纬范围（弧度矩形）。
    virtual Rectangle projectableRectangle() const = 0;
};

/// 等距圆柱投影：x = λ·a、y = φ·a（a = 椭球赤道半径）。
/// 低/中纬区域网格近似正方形；高纬经度方向被拉伸（非等积）。
class GeographicProjection : public Projection {
public:
    explicit GeographicProjection(const Ellipsoid& ellipsoid = Ellipsoid::WGS84());

    Vec2 project(const Cartographic& cartographic) const override;
    Cartographic unproject(const Vec2& positionMeters) const override;
    double northSouthMetersPerRadian(double latitudeRadians) const override;
    Rectangle projectableRectangle() const override;

private:
    Ellipsoid ellipsoid_;
};

/// Web Mercator（EPSG:3857）：x = λ·R、y = R·ln(tan(π/4 + φ/2))，R = 赤道半径。
/// 纬度钳制 ±85.05112878° → 全球正方形世界。地形/影像主流 XYZ 瓦片基于它。
class WebMercatorProjection : public Projection {
public:
    explicit WebMercatorProjection(const Ellipsoid& ellipsoid = Ellipsoid::WGS84());

    Vec2 project(const Cartographic& cartographic) const override;
    Cartographic unproject(const Vec2& positionMeters) const override;
    double northSouthMetersPerRadian(double latitudeRadians) const override;
    Rectangle projectableRectangle() const override;

    /// 纬度钳制上限（弧度）：Web Mercator 正方形世界对应的纬度。
    static double maximumLatitudeRadians();

private:
    Ellipsoid ellipsoid_;
};

} // namespace earth_engine
