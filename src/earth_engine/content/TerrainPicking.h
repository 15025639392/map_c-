#pragma once

#include <optional>

#include "TerrainFrameAssembler.h"
#include "../core/math/Vec3.h"

namespace earth_engine {

/// 射线对装配后地形帧的拾取结果。
struct TerrainPickHit {
    bool hit = false;
    double t = 0.0;               // 沿射线（单位方向）的距离
    Vec3 point;                   // ECEF 命中点
    TileKey key;                  // 命中所在瓦
    size_t frameIndex = 0;        // frames 中的索引
    Vec3 faceNormal;              // 命中三角形面法线（外向，单位）
};

/// 拾取地形：在 frames 全部三角形中找射线最近命中（不做背面剔除）。
/// 未命中返回 nullopt。
std::optional<TerrainPickHit> pickTerrainFrame(const Vec3& origin, const Vec3& direction,
                                               const std::vector<TerrainFrameAssembler::Frame>& frames);

} // namespace earth_engine
