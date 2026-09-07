#include "earth_engine/providers/StbPngDecoder.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace earth_engine {

std::optional<RgbImage> decodePngToRgb(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return std::nullopt;
    }
    int w = 0;
    int h = 0;
    int channels = 0;
    // 强制 3 通道：PNG 灰度/调色板/带 alpha 统一转 RGB。
    unsigned char* pixels =
        stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &channels, 3);
    if (pixels == nullptr || w <= 0 || h <= 0) {
        return std::nullopt;
    }
    RgbImage image;
    image.width = w;
    image.height = h;
    const size_t count = static_cast<size_t>(w) * h * 3;
    image.rgb.assign(pixels, pixels + count);
    stbi_image_free(pixels);
    return image;
}

} // namespace earth_engine
