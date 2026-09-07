#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace earth_engine {

/// PNG 解码产物：RGB 行（每像素 3 字节，行序 = 图像行序，首行 = 顶行 = 北，
/// 与 HeightmapCodec 约定一致）。
struct RgbImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;

    bool empty() const { return width <= 0 || height <= 0 || rgb.empty(); }
    size_t strideBytes() const { return static_cast<size_t>(width) * 3; }
};

/// PNG 解码（stb_image）。输入非 PNG / 损坏 → nullopt。
/// 输出统一 3 通道 RGB（stb 内部做通道转换）；alpha 丢弃。
std::optional<RgbImage> decodePngToRgb(const uint8_t* data, size_t size);

/// RGBA 解码（4 通道，保留 alpha）——透明叠加层（路网注记/矢量垫层）用。
/// 源无 alpha（JPEG/RGB PNG）→ alpha 全 255。
struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
    bool empty() const { return width <= 0 || height <= 0 || rgba.empty(); }
};
std::optional<RgbaImage> decodePngToRgba(const uint8_t* data, size_t size);

} // namespace earth_engine
