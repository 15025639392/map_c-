#pragma once

namespace earth_engine {

/// 相机运动模型参数（数值安全界；全部纯函数、确定性——同初值 + 同 dt 序列 → 同输出）。
/// S6 相机导航 host 地基：只做运动模型（惯性衰减 / flyTo 插值），不含输入手势、
/// 不含穿地 clamp（那属后续导航控制器，见头注释边界）。
struct CameraMotionParams {
    /// 惯性阻尼（指数衰减系数 / 秒）：速率每步 ×≈(1 - damping·dt)（有下界 0）。
    double dampingPerSecond = 4.0;
    /// 各轴速率上限（rad/s、m/s）——防发散/跑飞（相机北极星：惯性收敛不跑飞）。
    double maxYawRateRadPerSec = 2.0;
    double maxPitchRateRadPerSec = 1.5;
    double maxDistRateMetersPerSec = 20000.0;
    /// 停机阈值：三轴速率均低于此即 isSettled（收敛）。
    double settleThresholdPerSec = 1e-4;
    /// 单步 dt 上限：防大步长数值跳变。
    double maxDtSeconds = 0.1;
    /// 俯仰角钳制（相对地平线向下为正；防止病态视角翻转）。
    double minPitchRad = -1.55; // ≈ -88.8°
    double maxPitchRad = 1.55;  // ≈ 88.8°
    /// 相机到目标距离下限（不穿地由导航控制器负责，这里只保证数值下界）。
    double minDistanceMeters = 30.0;
};

/// 相机状态：绕目标点的朝向 + 距离（位置/朝向语义由上层把 yaw/pitch/dist 映射到
/// ECEF；本模型只保证数值序列确定性/有界/收敛）。
struct CameraMotionState {
    double yawRad = 0.0;        // 航向
    double pitchRad = -0.7;     // 俯仰（向下为正）
    double distanceMeters = 3000.0;
    // 惯性速率（内部态，stepInertia 消费）。
    double yawRateRadPerSec = 0.0;
    double pitchRateRadPerSec = 0.0;
    double distRateMetersPerSec = 0.0;
    bool settled = true;
};

class CameraMotion {
public:
    explicit CameraMotion(CameraMotionParams params = CameraMotionParams());

    const CameraMotionParams& params() const { return params_; }

    /// 惯性/阻尼一步：消费当前速率推进状态；速率按阻尼衰减并被上限钳制，
    /// dt 被钳到 maxDtSeconds。三轴速率均低于阈值 → settled=true。
    void stepInertia(CameraMotionState& state, double dt) const;

    /// flyTo 一步：以 t∈[0,1]（调用方给或由动画系统推进，单调不倒退由调用方保证）
    /// 在 yaw/pitch/distance 上向目标线性插值；t≥1 时精确钉死到目标并 settled=true
    /// （防浮点残留）。不做水平位移（目标点固定）。
    void stepFlyTo(CameraMotionState& state, double targetYawRad, double targetPitchRad,
                   double targetDistanceMeters, double t) const;

    /// 当前是否已收敛（速率全低于阈值）。
    bool isSettled(const CameraMotionState& state) const;

private:
    CameraMotionParams params_;
};

} // namespace earth_engine
