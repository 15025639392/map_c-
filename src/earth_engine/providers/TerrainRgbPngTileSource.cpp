#include "earth_engine/providers/TerrainRgbPngTileSource.h"

namespace earth_engine {

TerrainRgbPngTileSource::TerrainRgbPngTileSource(const ITileBytesSource& bytesSource,
                                                 std::string urlTemplate)
    : bytesSource_(bytesSource), urlTemplate_(std::move(urlTemplate)) {}

std::optional<TerrainGrid> TerrainRgbPngTileSource::requestHeights(
    const WebMercatorTileScheme& /*scheme*/, const TileKey& key, int gridSize) const {
    if (gridSize <= 0) {
        return std::nullopt;
    }
    const std::string url = TileUrlFormatter::format(urlTemplate_, key);
    const std::optional<std::vector<uint8_t>> bytes = bytesSource_.requestTileBytes(key, url);
    if (!bytes) {
        return std::nullopt;
    }
    const std::optional<RgbImage> image = decodePngToRgb(bytes->data(), bytes->size());
    if (!image || image->width != gridSize || image->height != gridSize) {
        return std::nullopt; // 非 PNG / 尺寸与请求不符
    }
    TerrainGrid grid;
    grid.width = gridSize;
    grid.height = gridSize;
    grid.heights.resize(static_cast<size_t>(gridSize) * gridSize);
    if (!HeightmapCodec::decodeTerrainRgb(image->rgb.data(), static_cast<size_t>(gridSize),
                                          static_cast<size_t>(gridSize), image->strideBytes(),
                                          grid.heights.data())) {
        return std::nullopt;
    }
    return grid;
}

} // namespace earth_engine
