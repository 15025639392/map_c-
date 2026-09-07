#include "earth_engine/content/TerrainFrameCache.h"

#include <algorithm>

namespace earth_engine {

TerrainFrameCache::TerrainFrameCache(const WebMercatorTileScheme& scheme,
                                     const ITerrainDataSource& source,
                                     const Ellipsoid& ellipsoid, int gridSize, int nodesPerEdge)
    : scheme_(scheme),
      source_(source),
      ellipsoid_(ellipsoid),
      gridSize_(gridSize),
      nodesPerEdge_(nodesPerEdge) {}

TerrainFrameCache::UpdateReport TerrainFrameCache::update(const CameraView& camera,
                                                          const TerrainLodConfig& lodConfig) {
    UpdateReport report;

    // 1) 脚印 → 选择（期望瓦集合）。
    const TerrainLodSelector selector;
    std::vector<TileKey> desired;
    if (const std::optional<Rectangle> footprint = camera.groundFootprintRadians(ellipsoid_)) {
        const TerrainLodResult selection =
            selector.selectTiles(scheme_, camera.position(), footprint.value(), lodConfig);
        desired = selection.tiles;
    }
    std::sort(desired.begin(), desired.end());

    // 2) 补缺瓦：仅对不在缓存的键向数据源请求并装配。
    const TerrainFrameAssembler assembler;
    std::vector<TileKey> toFetch;
    for (const TileKey& key : desired) {
        if (cache_.find(key) != cache_.end()) {
            ++report.reused; // 取前已在缓存 = 真正复用
        } else {
            toFetch.push_back(key);
        }
    }
    if (!toFetch.empty()) {
        TerrainLodResult fetchSelection;
        fetchSelection.tiles = std::move(toFetch);
        const std::vector<TerrainFrameAssembler::Frame> fetched =
            assembler.assemble(scheme_, fetchSelection, source_, ellipsoid_, gridSize_,
                               nodesPerEdge_);
        report.requested = fetched.size(); // 源实际给的瓦数（缺瓦被拒不计入）
        for (const TerrainFrameAssembler::Frame& frame : fetched) {
            cache_[frame.key] = frame;
        }
    }

    // 3) 淘汰视野外瓦 + 产出当前帧（排序）。
    frames_.clear();
    frames_.reserve(desired.size());
    for (const TileKey& key : desired) {
        const auto it = cache_.find(key);
        if (it != cache_.end()) {
            frames_.push_back(it->second);
        }
    }
    report.frameCount = frames_.size();

    std::vector<TileKey> stale;
    for (const auto& [key, frame] : cache_) {
        if (!std::binary_search(desired.begin(), desired.end(), key)) {
            stale.push_back(key);
        }
    }
    for (const TileKey& key : stale) {
        cache_.erase(key);
        ++report.evicted;
    }
    return report;
}

} // namespace earth_engine
