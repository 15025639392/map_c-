#include "earth_engine/imagery/ImageryTileSource.h"

#include "earth_engine/providers/PngToRgba8.h"
#include "earth_engine/providers/TileUrlFormatter.h"

namespace earth_engine {

ImageryTileSource::ImageryTileSource(const ITileBytesSource& bytesSource,
                                     std::string urlTemplate,
                                     std::function<bool(const TileKey&)> hasData)
    : bytesSource_(bytesSource),
      urlTemplate_(std::move(urlTemplate)),
      hasData_(std::move(hasData)) {}

std::optional<ImageryTileSource::Result> ImageryTileSource::fetchTexture(
    const TileKey& request) const {
    const ImageryTileResolution resolution = resolveImageryTile(hasData_, request);
    if (!resolution.resolved.has_value() || !resolution.covered) {
        return std::nullopt; // 全链无真实数据：明确空，不取假瓦
    }
    const TileKey& key = *resolution.resolved;
    const std::string url = TileUrlFormatter::format(urlTemplate_, key);
    const std::optional<std::vector<uint8_t>> bytes = bytesSource_.requestTileBytes(key, url);
    if (!bytes) {
        return std::nullopt;
    }
    std::optional<render::Texture2DData> tex = PngToRgba8::decode(bytes->data(), bytes->size());
    if (!tex) {
        return std::nullopt;
    }
    return Result{key, std::move(*tex)};
}

} // namespace earth_engine
