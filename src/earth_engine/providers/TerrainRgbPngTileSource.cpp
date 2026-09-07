#include "earth_engine/providers/TerrainRgbPngTileSource.h"

#include "earth_engine/providers/ImageTileBodyCheck.h"

namespace earth_engine {

TerrainRgbPngTileSource::TerrainRgbPngTileSource(const ITileBytesSource& bytesSource,
                                                 std::string urlTemplate,
                                                 bool cellRegisteredRing, int minZoom,
                                                 int maxZoom)
    : bytesSource_(bytesSource),
      urlTemplate_(std::move(urlTemplate)),
      cellRegisteredRing_(cellRegisteredRing),
      minZoom_(minZoom),
      maxZoom_(maxZoom) {}

std::optional<TerrainGrid> TerrainRgbPngTileSource::requestHeights(
    const WebMercatorTileScheme& /*scheme*/, const TileKey& key, int gridSize) const {
    if (gridSize <= 0 || key.z() < minZoom_ || key.z() > maxZoom_) {
        return std::nullopt; // 源覆盖层级外（如 NASA 源 z6–12；z13 实测 404）
    }
    // 环模式：请求 gridSize 个 cell → 期望 PNG (gridSize+2)²（512 cell + 1px 环）。
    const int raster = cellRegisteredRing_ ? gridSize + 2 : gridSize;
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
    if (!image || image->width != raster || image->height != raster) {
        return std::nullopt; // 非 PNG / 尺寸与请求不符（含环/非环规格错配）
    }
    TerrainGrid grid;
    grid.width = raster;
    grid.height = raster;
    grid.borderInset = cellRegisteredRing_ ? 0.5 : 0.0;
    grid.heights.resize(static_cast<size_t>(raster) * raster);
    if (!HeightmapCodec::decodeTerrainRgb(image->rgb.data(), static_cast<size_t>(raster),
                                          static_cast<size_t>(raster), image->strideBytes(),
                                          grid.heights.data())) {
        return std::nullopt;
    }
    // Terrain-RGB 隐式注册 nodata 哨兵（同 TerrainRgbTileSource；见其注释）。
    grid.noDataValues.push_back(HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    return grid;
}

} // namespace earth_engine
