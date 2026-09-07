#include "earth_engine/providers/TerrainRgbTileSource.h"

namespace earth_engine {

TerrainRgbTileSource::TerrainRgbTileSource(const ITileBytesSource& bytesSource,
                                           std::string urlTemplate)
    : bytesSource_(bytesSource), urlTemplate_(std::move(urlTemplate)) {}

std::optional<TerrainGrid> TerrainRgbTileSource::requestHeights(
    const WebMercatorTileScheme& /*scheme*/, const TileKey& key, int gridSize) const {
    if (gridSize <= 0) {
        return std::nullopt;
    }
    const std::string url = TileUrlFormatter::format(urlTemplate_, key);
    const std::optional<std::vector<uint8_t>> bytes = bytesSource_.requestTileBytes(key, url);
    if (!bytes) {
        return std::nullopt;
    }
    const size_t expected = static_cast<size_t>(gridSize) * gridSize * 3;
    if (bytes->size() != expected) {
        return std::nullopt; // 畸形（长度不符）
    }
    TerrainGrid grid;
    grid.width = gridSize;
    grid.height = gridSize;
    grid.heights.resize(expected / 3);
    if (!HeightmapCodec::decodeTerrainRgb(bytes->data(), static_cast<size_t>(gridSize),
                                          static_cast<size_t>(gridSize),
                                          static_cast<size_t>(gridSize) * 3,
                                          grid.heights.data())) {
        return std::nullopt;
    }
    // Terrain-RGB 隐式注册 nodata 哨兵：RGB(0,0,0) 解码恰为 -10000m = 数据空洞/
    // 缺邻居重叠环的底值（镜像 gis-md decodeTile 的隐式注册）。不注册则 -10000
    // 被当合法高度混进 min/max 与边缘双线性 → km 级假深沟 + 假悬崖法线。
    grid.noDataValues.push_back(HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    return grid;
}

} // namespace earth_engine
