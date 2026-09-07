#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../core/geodesy/Cartographic.h"
#include "../core/geodesy/Ellipsoid.h"
#include "../core/math/Vec3.h"

namespace earth_engine {
namespace vector {

/// 最小矢量数据腿（S5 host 切片）：GeoJSON 点要素子集解码。
/// 解析 `type: Point | MultiPoint` 的 `coordinates`（[lon,lat] 对），无第三方 JSON。
struct VectorPoint {
    double lonRad = 0.0;
    double latRad = 0.0;
    std::string styleKey; // 样式键（如 "road"/""，默认样式）
};

/// 解码 GeoJSON 点要素（顶层 features[]，或单个 Geometry）。
/// 支持点列表形式 [lon,lat] 与 [[lon,lat],…]；非法/空 → 空结果。
std::vector<VectorPoint> decodeGeoJsonPoints(const std::string& json);

/// 最小样式规则（S5 最小切片：确定性的键→样式映射）。
struct PointStyle {
    std::uint32_t colorArgb = 0xFFFFFFFF;
    double widthPx = 2.0;
    bool visible = true;
};

/// 键 → 样式（未知键给默认样式；host 可测/确定性）。
PointStyle styleForKey(const std::string& kind);

/// 矢量贴地管线（S5 host 切片）：经纬 + 可选浮空 offset → 地表 ECEF。
/// groundHeightFn 返回该处椭球高（米）；无地表数据 → nullopt（不落点）。
/// offsetMeters 沿地表法向抬升（标牌/线杆浮空语义，默认 0 = 精确贴地）。
class VectorGrounding {
public:
    using GroundHeightFn = std::function<std::optional<double>(const Cartographic&)>;

    VectorGrounding(GroundHeightFn ground, const Ellipsoid& ellipsoid = Ellipsoid::WGS84())
        : ground_(std::move(ground)), ellipsoid_(ellipsoid) {}

    /// 把点投影到地表（+offset）：返回 ECEF 位置；无地表数据/脏输入 → nullopt。
    std::optional<Vec3> projectToTerrain(const VectorPoint& point,
                                         double offsetMeters = 0.0) const;

private:
    GroundHeightFn ground_;
    Ellipsoid ellipsoid_;
};

} // namespace vector
} // namespace earth_engine
