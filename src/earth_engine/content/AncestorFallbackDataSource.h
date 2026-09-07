#pragma once

#include "TerrainDataSource.h"
#include "../tiling/TileKey.h"

namespace earth_engine {

/// 祖先回退数据源（调度-lite 前身，对应 stage6-merge-checkpoint 的
/// 「TerrainFrameCache ↔ 调度/缓存/帧收敛」行 / 本仓 roadmap 的
/// 「缺失瓦跳过（祖先回退属调度阶段）」缺口）。
///
/// 包装任一 ITerrainDataSource：请求瓦缺失（nullopt）时沿父链上溯最多
/// maxFallbackLevels 层，取到最近可用祖先栅格并**重采样到请求瓦的栅格**返回——
/// 下游（装配/网格）无感知，帧不因单瓦缺失/瞬时解码失败而出现空洞
/// （北极星「任意加载阶段地貌可辨」、换代体面机制族）。源在任意祖先层都无覆盖
/// 时照旧返回 nullopt（不冒充数据）。
///
/// 语义注记：输出 = 「父数据、子几何」占位内容（父栅格双线性重采样到子瓦像素），
/// 与 gis-md GltfTerrainUpsampler（跨级细分）同属换代过渡机制族；本仓为 CPU 网格
/// 形态先落「祖先回退 + 重采样」这一半，跨级无缝（T 顶点 remap）仍在 B4。
class AncestorFallbackDataSource final : public ITerrainDataSource {
public:
    /// @param inner 被包装的数据源（生命周期由调用方管理）。
    /// @param maxFallbackLevels 上溯层数上限（默认 4：z13 缺失可回退到 z9）。
    explicit AncestorFallbackDataSource(const ITerrainDataSource& inner,
                                        int maxFallbackLevels = 4);

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override;

private:
    const ITerrainDataSource& inner_;
    int maxFallbackLevels_;
};

} // namespace earth_engine
