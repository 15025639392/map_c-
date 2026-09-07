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
    return grid;
}

} // namespace earth_engine
