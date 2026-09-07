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

} // namespace earth_engine
