#include "earth_engine/providers/StbPngDecoder.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace earth_engine {

std::optional<RgbaImage> decodePngToRgba(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return std::nullopt;
    }
    int w = 0;
    int h = 0;
    int channels = 0;
    // 4 通道输出：PNG alpha 保留；无 alpha 格式 alpha=255。
    unsigned char* pixels =
        stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &channels, 4);
    if (pixels == nullptr || w <= 0 || h <= 0) {
        return std::nullopt;
    }
    RgbaImage image;
    image.width = w;
    image.height = h;
    const size_t count = static_cast<size_t>(w) * h * 4;
    image.rgba.assign(pixels, pixels + count);
    stbi_image_free(pixels);
    return image;
}

std::optional<RgbImage> decodePngToRgb(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return std::nullopt;
    }
    int w = 0;
    int h = 0;
    int channels = 0;
    // 强制 3 通道：灰度/调色板/带 alpha/其它格式统一转 RGB。
    // 注：函数名保留 decodePngToRgb（历史名），实际支持 stb 启用的所有格式——
    // 2026-09-09 为高德卫星 JPEG 源放开 STBI_ONLY_PNG（PNG/JPEG 等）。
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
