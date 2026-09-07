#pragma once

#include <functional>
#include <optional>

#include "CameraMotion.h"
#include "GestureToMotion.h"
#include "TerrainGroundGuard.h"
#include "../core/geodesy/Cartographic.h"

namespace earth_engine {

/// 引擎层**地图相机系统**（S6 主机侧；与 CameraNavController 的 orbit 语义互补）。
///
/// 语义（类头注释声明，判据口径：相机北极星"惯性收敛/不穿地/视角不翻"）：
/// - 相机位于**中心点（lon/lat，即相机正下方地面点）的正上方**，朝向由 heading/pitch
///   决定（heading 0=北 顺时针；pitch 相对地平线**向下为正**）；缩放 = 相机高度。
///   —— 即地图应用的"俯视相机"参数化（Google Earth / Mapbox 同型），非环绕巡视。
/// - 每帧喂屏幕手势增量（拖动 dx/dy、双指缩放比 scale>1 拉近）与屏幕高，
///   内部经 GestureToMotion 映射为 yaw/pitch/dist(=alt) 速率；随后
///   CameraMotion.stepInertia 做惯性/阻尼（速率上限、俯仰界、距离下限由模型保证）：
///   抬手后按最后速率指数衰减收敛（不跑飞、可收敛判定）；
/// - 贴地防护：相机高度 < 正下方地表高度 + minClearance → 抬到 floor（不穿地）；
///   无地表数据（GroundHeightFn 返回 nullopt）按"无地表约束"处理，回落 minAltitude；
/// - flyTo：把 yaw/pitch/altitude 以平滑步向目标插值（时长 params.flyToSeconds），
///   结束后钉死并 settled；任意手势输入取消 flyTo；
/// - 中心平移（lon/lat 跟随）属后续轮（引擎先俯仰/航向/高度三轴 + 防穿地）。
/// - 确定性：同初值同 (输入, dt) 序列 → 同输出；输入含 NaN/Inf → 忽略该帧手势
///   （不吞脏数据）；各轴边界钳制保证无 NaN。
/// - 本类不依赖任何平台/渲染：地面查高由调用方注入（回调），引擎只做运动/防护语义。
class MapCameraSystem {
public:
    /// 输入/输出与子模型参数（角均为弧度）。
    struct Params {
        Params() { motion.minPitchRad = 0.0; } // 默认不抬头（0 = 地平线）；调用方可再收紧
        GestureToRatesParams gesture = GestureToRatesParams(); // 拖动/缩放 → 速率
        CameraMotionParams motion = CameraMotionParams();      // 惯性/阻尼/俯仰界/距离下限
        double minClearanceMeters = 5.0;   // 贴地净空（不穿地，米）
        double minAltitudeMeters = 30.0;   // 无地表数据时的高度下限
        double maxAltitudeMeters = 250000.0; // 拉远上限（防无限出带）
        double flyToSeconds = 2.5;         // flyTo 时长（秒）
    };

    struct Pose {
        double lonRad = 0.0;       // 相机正下方中心点
        double latRad = 0.0;
        double altitudeMeters = 15000.0;
        double headingRad = 0.0;   // 0=北 顺时针（输出已 wrap 到 [0,2π)）
        double pitchRad = 0.7;     // 相对地平线向下为正
    };

    using GroundHeightFn = TerrainGroundGuard::GroundHeightFn;

    explicit MapCameraSystem(Params params = Params());

    const Params& params() const { return params_; }
    /// 当前相机位姿（由内部状态换算；每次 step 后有效）。
    Pose pose() const;
    /// 是否收敛（无惯性/无 flyTo；手势按住不计）。
    bool isSettled() const { return settled_ && !flying_; }
    bool flying() const { return flying_; }

    /// 直接跳转位姿（station 预设/初始/外部绝对控制；清惯性、取消 flyTo）。
    /// 输入会被边界钳制：heading wrap、pitch 与 altitude 收敛到模型界内。
    void setPose(const Pose& pose);

    /// 地面查高服务（贴地防护数据腿；nullptr/无数据 → 无地表约束）。
    void setGroundFn(GroundHeightFn ground) { ground_ = std::move(ground); }

    /// 注入本帧手势增量（拖动像素 dx/dy：y 向下为正；双指缩放比 scale>1=拉近）。
    /// 每次 step 前调用；0/1 表示无对应轴输入。screenHeightPx 用于像素归一。
    void setGesture(double dragDxPx, double dragDyPx, double pinchScale,
                    double screenHeightPx);

    /// 一步：手势→速率（或惯性衰减）→ 位置积分 → 边界/贴地 clamp → flyTo 进度。
    /// dt 被模型内部钳到 maxDtSeconds；settled 且无输入时零开销早退。
    void step(double dtSeconds);

    /// flyTo：从当前位姿平滑飞到目标（仅 yaw/pitch/altitude 三轴；中心点平移未实现，
    /// target 的 lon/lat 被忽略）。已在 fly 中 → 改目标并重计时。
    void flyTo(const Pose& target);
    void cancelFlyTo();

private:
    Params params_;
    CameraMotion motion_;
    GestureToMotion gestureMapper_;
    TerrainGroundGuard guard_;
    GroundHeightFn ground_;

    CameraMotionState state_; // turret 语义：yaw=heading、pitch=向下为正、distance=alt
    double centerLonRad_ = 0.0;
    double centerLatRad_ = 0.0;
    bool settled_ = true;

    // 手势暂存（单帧消费）。
    double gDxPx_ = 0.0;
    double gDyPx_ = 0.0;
    double gScale_ = 1.0;
    double gScreenHeightPx_ = 1080.0;
    bool gHasInput_ = false;

    // flyTo 内部态。
    bool flying_ = false;
    double flyClockSeconds_ = 0.0;
    double flyYawRad_ = 0.0;     // 目标（绝对 yaw，与当前状态可差多圈）
    double flyPitchRad_ = 0.7;
    double flyAltitudeMeters_ = 15000.0;
};

} // namespace earth_engine
