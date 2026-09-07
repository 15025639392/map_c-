#include "earth_engine/renderer/HeightTextureCodec.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace earth_engine::render {

namespace {

constexpr double kTerrainRgbOffset = 10000.0;
constexpr double kTerrainRgbScale = 10.0; // 每单位 0.1 m → ×10
constexpr double kMaxEncoded24 = 16777215.0;

} // namespace

HeightTextureResult HeightTextureCodec::encode(const std::vector<double>& heights, int w,
                                               int h) {
    HeightTextureResult result;
    if (w <= 0 || h <= 0 || heights.size() != static_cast<size_t>(w) * h) {
        return result; // 无效输入 → 空
    }
    result.texture.width = w;
    result.texture.height = h;
    result.texture.rgba8.resize(static_cast<size_t>(w) * h * 4);
    double minH = std::numeric_limits<double>::max();
    double maxH = std::numeric_limits<double>::lowest();
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const double hgt = heights[static_cast<size_t>(row) * w + col];
            minH = std::min(minH, hgt);
            maxH = std::max(maxH, hgt);
            // Terrain-RGB 绝对编码（0.1m 步长；与内容解码同公式同纪律）。
            double raw = (hgt + kTerrainRgbOffset) * kTerrainRgbScale;
            raw = std::clamp(raw, 0.0, kMaxEncoded24);
            const uint32_t value = static_cast<uint32_t>(std::llround(raw));
            uint8_t* px =
                result.texture.rgba8.data() + (static_cast<size_t>(row) * w + col) * 4;
            px[0] = static_cast<uint8_t>((value >> 16) & 0xFFu);
            px[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
            px[2] = static_cast<uint8_t>(value & 0xFFu);
            px[3] = 255;
        }
    }
    result.stats.minHeightMeters = minH;
    result.stats.maxHeightMeters = maxH;
    result.stats.byteCount = static_cast<size_t>(w) * h * 4;
    return result;
}

double HeightTextureCodec::decodeHeightAt(const Texture2DData& texture, int col, int row) {
    if (col < 0 || row < 0 || col >= texture.width || row >= texture.height) {
        return 0.0;
    }
    const uint8_t* px =
        texture.rgba8.data() + (static_cast<size_t>(row) * texture.width + col) * 4;
    const double value =
        static_cast<double>(px[0]) * 65536.0 + static_cast<double>(px[1]) * 256.0 +
        static_cast<double>(px[2]);
    return -kTerrainRgbOffset + value / kTerrainRgbScale;
}

} // namespace earth_engine::render
