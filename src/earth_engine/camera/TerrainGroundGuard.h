#pragma once

#include <functional>
#include <optional>

#include "../core/geodesy/Cartographic.h"

namespace earth_engine {

/// 贴地防护（S6 导航 host 切片）：相机/对象高度相对地形的"不穿地"策略纯函数。
///
/// 语义（供导航控制器调用；判据口径：相机北极星"不穿地"）：
/// - groundHeightFn：给定经纬（弧度）返回该处地表高度（米，椭球高口径；调用方
///   提供，可以是地形帧查高/装饰器源）；
/// - 相机高度必须 ≥ ground + minClearanceMeters，否则抬到该值（clamp 不穿地）；
/// - 经纬无地形数据（groundHeightFn 返回 nullopt，如海/无覆盖区）时按"无地表约束"
///   处理（原高度不动）——是否回落椭球属渲染/回落策略，不在此层；
/// - 输入含 NaN/Inf：返回 nullopt（不吞脏数据、不产脏输出）。
struct GroundClearanceResult {
    double heightMeters = 0.0;
    /// 是否被抬升过（false = 原本就合法 / 无约束 / 无效输入）。
    bool clamped = false;
};

class TerrainGroundGuard {
public:
    using GroundHeightFn = std::function<std::optional<double>(const Cartographic&)>;

    explicit TerrainGroundGuard(double minClearanceMeters = 5.0);

    double minClearanceMeters() const { return minClearanceMeters_; }

    /// 对经纬位置（弧度）处、给定高度 heightMeters 做贴地 clamp。
    /// 无效输入（非有限经纬/高度）→ nullopt。groundHeightFn 由调用方提供
    /// （无地表数据返回 nullopt → 原高度不动，clamped=false）。
    std::optional<GroundClearanceResult> enforceClearance(double lonRad, double latRad,
                                                          double heightMeters,
                                                          GroundHeightFn ground) const;

private:
    double minClearanceMeters_;
};

} // namespace earth_engine
