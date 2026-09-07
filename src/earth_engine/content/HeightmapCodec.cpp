#include "earth_engine/content/HeightmapCodec.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

namespace {

/// 24bit 编码值的上下界：0xFFFFFF。
constexpr double kMaxEncoded24 = 16777215.0;
constexpr double kTerrainRgbOffset = 10000.0;
constexpr double kTerrainRgbScale = 10.0; // 每单位 0.1 m → ×10
constexpr double kTerrariumOffset = 32768.0;

} // namespace

double HeightmapCodec::decodeTerrainRgbPixel(uint8_t r, uint8_t g, uint8_t b) {
    const double value = static_cast<double>(r) * 65536.0 +
                         static_cast<double>(g) * 256.0 +
                         static_cast<double>(b);
    return -kTerrainRgbOffset + value * (1.0 / kTerrainRgbScale);
}

double HeightmapCodec::decodeTerrariumPixel(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<double>(r) * 256.0 + static_cast<double>(g) +
            static_cast<double>(b) / 256.0) -
           kTerrariumOffset;
}

void HeightmapCodec::encodeTerrainRgbPixel(double heightMeters, uint8_t& outR, uint8_t& outG,
                                           uint8_t& outB) {
    // value = (h + 10000) * 10，取整到 0.1 m 网格。
    const double raw = (heightMeters + kTerrainRgbOffset) * kTerrainRgbScale;
    const double clamped = std::clamp(raw, 0.0, kMaxEncoded24);
    const uint32_t value = static_cast<uint32_t>(std::llround(clamped));
    outR = static_cast<uint8_t>((value >> 16) & 0xFFu);
    outG = static_cast<uint8_t>((value >> 8) & 0xFFu);
    outB = static_cast<uint8_t>(value & 0xFFu);
}

bool HeightmapCodec::decodeTerrarium(const uint8_t* pixels, size_t width, size_t height,
                                        size_t strideBytes, double* outHeights) {
    if (pixels == nullptr || outHeights == nullptr || width == 0 || height == 0 ||
        strideBytes < width * 3) {
        return false;
    }
    for (size_t row = 0; row < height; ++row) {
        const uint8_t* src = pixels + row * strideBytes;
        double* dst = outHeights + row * width;
        for (size_t col = 0; col < width; ++col) {
            dst[col] = decodeTerrariumPixel(src[col * 3], src[col * 3 + 1], src[col * 3 + 2]);
        }
    }
    return true;
}

bool HeightmapCodec::decodeTerrainRgb(const uint8_t* pixels, size_t width, size_t height,
                                      size_t strideBytes, double* outHeights) {
    if (pixels == nullptr || outHeights == nullptr || width == 0 || height == 0 ||
        strideBytes < width * 3) {
        return false;
    }
    for (size_t row = 0; row < height; ++row) {
        const uint8_t* src = pixels + row * strideBytes;
        double* dst = outHeights + row * width;
        for (size_t col = 0; col < width; ++col) {
            dst[col] = decodeTerrainRgbPixel(src[col * 3], src[col * 3 + 1], src[col * 3 + 2]);
        }
    }
    return true;
}

} // namespace earth_engine
