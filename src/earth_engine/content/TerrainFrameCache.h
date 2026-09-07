#pragma once

#include <map>
#include <vector>

#include "TerrainDataSource.h"
#include "TerrainFrameAssembler.h"
#include "../camera/CameraView.h"
#include "../tiling/TerrainLodSelector.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// 地形帧缓存：按瓦键缓存已装配帧，update(相机) 时只对"缺瓦"请求数据源。
/// host 侧把"加载/重建/淘汰"语义立起来（调度-lite）：
/// - 同相机再 update → 零数据源请求、帧零重建；
/// - 相机小移动 → 大部分瓦复用；
/// - 大跳（视野外全换）→ 旧瓦淘汰、新瓦请求。
class TerrainFrameCache {
public:
    TerrainFrameCache(const WebMercatorTileScheme& scheme, const ITerrainDataSource& source,
                      const Ellipsoid& ellipsoid, int gridSize, int nodesPerEdge);

    /// 一帧更新：脚印 → 选择 → 补缺瓦 → 产出帧（按 TileKey 排序）→ 淘汰视野外瓦。
    struct UpdateReport {
        size_t requested = 0; // 数据源请求次数（= 补的缺瓦数；缺瓦被源拒绝时 < 补瓦数）
        size_t reused = 0;    // 缓存命中帧数
        size_t evicted = 0;   // 淘汰的旧瓦数
        size_t frameCount = 0;
    };
    UpdateReport update(const CameraView& camera, const TerrainLodConfig& lodConfig);

    /// 当前帧（按 TileKey 排序；与缓存内对象同源）。
    const std::vector<TerrainFrameAssembler::Frame>& frames() const { return frames_; }

    /// 缓存大小（含当前帧与已装配对象）。
    size_t cacheSize() const { return cache_.size(); }

private:
    const WebMercatorTileScheme& scheme_;
    const ITerrainDataSource& source_;
    const Ellipsoid& ellipsoid_;
    int gridSize_;
    int nodesPerEdge_;
    std::map<TileKey, TerrainFrameAssembler::Frame> cache_;
    std::vector<TerrainFrameAssembler::Frame> frames_;
};

} // namespace earth_engine
