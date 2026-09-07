#include "earth_engine/providers/TerrainRgbPngTileSource.h"

#include "earth_engine/providers/ImageTileBodyCheck.h"

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
    // 网络硬化：魔数白名单（PNG/JPEG/WebP）把 CDN 200 错误体（NoSuchKey XML 等）
    // 挡在 PNG 解码之前——语义转写 gis-md ImageTileBodyCheck，见该头文件注释。
    if (!looksLikeImageTileBody(*bytes)) {
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
    // Terrain-RGB 隐式注册 nodata 哨兵（同 TerrainRgbTileSource；见其注释）。
    grid.noDataValues.push_back(HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    return grid;
}

} // namespace earth_engine
