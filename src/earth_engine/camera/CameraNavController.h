#pragma once

#include <functional>
#include <optional>

#include "CameraMotion.h"
#include "TerrainGroundGuard.h"
#include "../core/geodesy/Cartographic.h"

namespace earth_engine {

/// 相机导航控制器（S6 host 切片）：把 CameraMotion（惯性/飞行）与
/// TerrainGroundGuard（不穿地）组装成**每步联动**的可测控制器。
///
/// 语义与近似（原型口径，头注释声明）：
/// - 控制器围绕"瞄准点"（target 经纬 + targetElevMeters 高程）工作；相机位姿由
///   CameraMotionState 的 yaw/pitch/distance 决定（pitch 向下为正）；
/// - 相机高度 ≈ targetElev + distance·sin(pitch)（相对目标俯仰的竖直分量）；
/// - 相机经纬 ≈ 瞄准点沿 yaw、水平距离 = distance·cos(pitch) 的小角球面平移
///   （R=6378137m；近距/小角下足够，精确 ECEF 属上层导航实现，不在此原型）；
/// - stepInertia 每步先推进惯性，再对相机所在经纬处做贴地 clamp：若低于
///   地面+净空，把 distance 抬到满足 clearance 的最小值（sin(pitch) 过小——
///   水平/上扬视角——不做距离强制，注明由近地俯仰限制兜底）；
/// - 确定性：同初值同 dt 序列 → 同输出；无 NaN（边界钳制）。
class CameraNavController {
public:
    using GroundHeightFn = TerrainGroundGuard::GroundHeightFn;

    CameraNavController(const CameraMotionParams& motionParams = CameraMotionParams(),
                        double minClearanceMeters = 5.0);

    void setTarget(const Cartographic& target, double targetElevMeters);
    void setGroundFn(GroundHeightFn ground);

    CameraMotionState& state() { return state_; }
    const CameraMotionState& state() const { return state_; }

    /// 一步：惯性推进 → 贴地 clamp（联动不穿地）。
    void step(double dtSeconds);

    /// 由当前状态推算相机经纬高（米，椭球口径近似，见类注释）。
    struct CameraPose {
        double lonRad = 0.0;
        double latRad = 0.0;
        double heightMeters = 0.0;
    };
    CameraPose pose() const;

    bool isSettled() const { return motion_.isSettled(state_); }

private:
    CameraMotion motion_;
    TerrainGroundGuard guard_;
    Cartographic target_;
    double targetElevMeters_ = 0.0;
    GroundHeightFn ground_;
    CameraMotionState state_;
};

} // namespace earth_engine
