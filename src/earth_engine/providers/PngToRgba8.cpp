#include "earth_engine/providers/PngToRgba8.h"

#include "earth_engine/providers/StbPngDecoder.h"

namespace earth_engine {

namespace {

render::Texture2DData rgbaToTexture(const RgbaImage& image) {
    render::Texture2DData out;
    out.width = image.width;
    out.height = image.height;
    out.rgba8 = image.rgba; // 已是 RGBA8（4 通道，alpha 保留）
    return out;
}

} // namespace

std::optional<render::Texture2DData> PngToRgba8::decode(const uint8_t* pngBytes, size_t size,
                                                        int maxDimensionPx) {
    const int limit = maxDimensionPx > 0 ? maxDimensionPx : 4096;
    if (pngBytes == nullptr || size == 0) {
        return std::nullopt;
    }
    const std::optional<RgbImage> image = decodePngToRgb(pngBytes, size);
    if (!image || image->width <= 0 || image->height <= 0 || image->width > limit ||
        image->height > limit) {
        return std::nullopt;
    }
    render::Texture2DData out;
    out.width = image->width;
    out.height = image->height;
    out.rgba8.resize(static_cast<size_t>(out.width) * out.height * 4);
    for (int row = 0; row < out.height; ++row) {
        const uint8_t* src = image->rgb.data() + static_cast<size_t>(row) * image->strideBytes();
        uint8_t* dst = out.rgba8.data() + static_cast<size_t>(row) * out.width * 4;
        for (int col = 0; col < out.width; ++col) {
            dst[col * 4 + 0] = src[col * 3 + 0];
            dst[col * 4 + 1] = src[col * 3 + 1];
            dst[col * 4 + 2] = src[col * 3 + 2];
            dst[col * 4 + 3] = 255;
        }
    }
    return out;
}

std::optional<render::Texture2DData> PngToRgba8::decodeKeepAlpha(const uint8_t* bytes,
                                                                 size_t size,
                                                                 int maxDimensionPx) {
    const int limit = maxDimensionPx > 0 ? maxDimensionPx : 4096;
    if (bytes == nullptr || size == 0) {
        return std::nullopt;
    }
    const std::optional<RgbaImage> image = decodePngToRgba(bytes, size);
    if (!image || image->width <= 0 || image->height <= 0 || image->width > limit ||
        image->height > limit) {
        return std::nullopt;
    }
    return rgbaToTexture(*image);
}

} // namespace earth_engine
