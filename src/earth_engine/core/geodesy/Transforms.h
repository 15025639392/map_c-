#pragma once

#include "Cartographic.h"
#include "Ellipsoid.h"
#include "../math/Mat4.h"

namespace earth_engine {

/// 参考椭球上的局部切平面帧变换（cesium-native 的 Transforms 同族功能）。
/// 目前提供最常用的 ENU（东-北-上）帧；后续相机/交互需要时再加
/// heading-pitch-roll、局部→ECEF 通用旋转帧。
namespace Transforms {

/// 以 cartographic 原点（含大地高）建立的 ENU→ECEF 刚体变换。
/// 列依次为：东(east) / 北(north) / 上(up) 单位轴 + 原点 ECEF 平移。
Mat4 eastNorthUpToFixedFrame(const Cartographic& origin, const Ellipsoid& ellipsoid);

/// eastNorthUpToFixedFrame 的逆（ECEF→ENU）。旋转部分正交，直接转置构造。
Mat4 fixedFrameToEastNorthUp(const Cartographic& origin, const Ellipsoid& ellipsoid);

} // namespace Transforms

} // namespace earth_engine
