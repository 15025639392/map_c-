#pragma once

#include <string>

#include "ITileBytesSource.h"
#include "StbPngDecoder.h"
#include "TileUrlFormatter.h"
#include "../content/HeightmapCodec.h"
#include "../content/TerrainDataSource.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// Terrain-RGB **PNG** 高度源：字节源 → PNG 解码 → Terrain-RGB 解码 → TerrainGrid。
/// 真实源（NASA Terrain-RGB 等）的瓦片字节就是 PNG——本类是 providers 栈的"真值"形态；
/// 纯 RGB 行变体见 TerrainRgbTileSource（供已解码管线/测试）。
class TerrainRgbPngTileSource : public ITerrainDataSource {
public:
    TerrainRgbPngTileSource(const ITileBytesSource& bytesSource, std::string urlTemplate);

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& /*scheme*/,
                                              const TileKey& key, int gridSize) const override;

private:
    const ITileBytesSource& bytesSource_;
    std::string urlTemplate_;
};

} // namespace earth_engine
