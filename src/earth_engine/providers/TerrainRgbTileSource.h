#pragma once

#include <string>

#include "ITileBytesSource.h"
#include "TileUrlFormatter.h"
#include "../content/HeightmapCodec.h"
#include "../content/TerrainDataSource.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// Terrain-RGB 高度数据源：从字节源拉取原始 RGB 行 → HeightmapCodec 解码 →
/// TerrainGrid（ITerrainDataSource 的一个实现，即"地形 Provider"）。
///
/// 字节容器约定：`gridSize × gridSize` 像素、每像素 3 字节 RGB、行序 = 图像行序
/// （首行=北）。真实 PNG 解码（stb）与网络（curl）是后续接入层——只需满足
/// ITileBytesSource 后本类零改动。
class TerrainRgbTileSource : public ITerrainDataSource {
public:
    TerrainRgbTileSource(const ITileBytesSource& bytesSource, std::string urlTemplate);

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override;

private:
    const ITileBytesSource& bytesSource_;
    std::string urlTemplate_;
};

} // namespace earth_engine
