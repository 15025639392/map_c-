#pragma once

#include "TerrainDataSource.h"
#include "../core/geodesy/HeightDatumCorrector.h"

namespace earth_engine {

/// 高程基准改正数据源（engine-targets §6 的接入路径）。
///
/// 内置 DEM / Terrain-RGB 源的高度多为 EGM96 正高（大地水准面起算）；本仓渲染按
/// WGS84 **椭球高**（椭球面沿法线抬升）。椭球高 ≈ 正高 + undulation。包装任一
/// ITerrainDataSource，把解码栅格每个样本的经纬 undulation 叠加进高度：
///   椭球高 = 源高 + corrector.undulationMeters(lat, lon)
/// 源高度含 no-data 哨兵（TerrainGrid::noDataValues 命中）时不改正、原样保留；
/// 恒等改正器（IdentityHeightDatumCorrector，默认）走零拷贝快路径——即本仓默认
/// 口径（不启用）不变，启用 = 把真实 EGM96 网格喂给 GridHeightDatumCorrector 后
/// 包一层本装饰器。
class HeightDatumCorrectingDataSource final : public ITerrainDataSource {
public:
    /// @param inner 被包装的数据源（生命周期由调用方管理）。
    /// @param corrector undulation 改正器（生命周期由调用方管理）。
    HeightDatumCorrectingDataSource(const ITerrainDataSource& inner,
                                    const IHeightDatumCorrector& corrector);

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override;

private:
    const ITerrainDataSource& inner_;
    const IHeightDatumCorrector& corrector_;
};

} // namespace earth_engine
