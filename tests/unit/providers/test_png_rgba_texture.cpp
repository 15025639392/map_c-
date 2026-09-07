// PngToRgba8：PNG 瓦片字节 → RGBA8 纹理数据（S4 影像瓦→GPU 纹理 host 腿）。
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../util/min_png_writer.h"
#include "earth_engine/providers/PngToRgba8.h"
#include "earth_engine/renderer/IRenderDevice.h"

using namespace earth_engine;

namespace {

std::vector<uint8_t> twoByTwoRgb() {
    // 2×2：红、绿 / 蓝、白
    const std::vector<uint8_t> rgb = {
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 255,
    };
    return mapc_test::writePngRgb(rgb, 2, 2);
}

} // namespace

TEST(PngToRgba8, DecodesFixturePngToRgba) {
    const std::vector<uint8_t> png = twoByTwoRgb();
    const auto tex = PngToRgba8::decode(png.data(), png.size());
    ASSERT_TRUE(tex.has_value());
    EXPECT_EQ(tex->width, 2);
    EXPECT_EQ(tex->height, 2);
    ASSERT_TRUE(tex->valid());
    // 行序 = 图像行序（首行=顶=北）：逐像素 RGBA 对照。
    EXPECT_EQ(tex->rgba8[0], 255);  // R
    EXPECT_EQ(tex->rgba8[1], 0);
    EXPECT_EQ(tex->rgba8[3], 255); // alpha 恒 255
    EXPECT_EQ(tex->rgba8[4 + 0], 0);
    EXPECT_EQ(tex->rgba8[4 + 1], 255);
    EXPECT_EQ(tex->rgba8[12 + 2], 255); // 白
}

TEST(PngToRgba8, GarbageAndEmptyRejected) {
    const uint8_t garbage[] = {0x89, 'G', 'A', 'R', 'B', 'A', 'G', 'E'};
    EXPECT_FALSE(PngToRgba8::decode(garbage, sizeof(garbage)).has_value());
    EXPECT_FALSE(PngToRgba8::decode(nullptr, 0).has_value());
}

TEST(PngToRgba8, DimensionLimitEnforced) {
    // 2×2 PNG 配 maxDimensionPx=1 → 拒。
    const std::vector<uint8_t> png = twoByTwoRgb();
    EXPECT_FALSE(PngToRgba8::decode(png.data(), png.size(), /*maxDimensionPx=*/1).has_value());
}
