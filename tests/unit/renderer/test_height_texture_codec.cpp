// HeightTextureCodec：每瓦高度纹理数值核（GPU 位移数据前提；RGBA8 全局绝对编码）。
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "earth_engine/renderer/HeightTextureCodec.h"

using namespace earth_engine::render;

namespace {
// Terrain-RGB 24bit 上界：-10000 + 16777215*0.1 = 1,667,721.5 m。
constexpr double kTerrainRgbUpperMeters = -10000.0 + 16777215.0 * 0.1;
} // namespace

TEST(HeightTextureCodec, RoundTripsLinearField) {
    constexpr int k = 17;
    std::vector<double> h(static_cast<size_t>(k) * k);
    for (int row = 0; row < k; ++row) {
        for (int col = 0; col < k; ++col) {
            h[static_cast<size_t>(row) * k + col] = -50.0 + 10.0 * col + 3.0 * row;
        }
    }
    const HeightTextureResult r = HeightTextureCodec::encode(h, k, k);
    ASSERT_TRUE(r.texture.valid());
    EXPECT_EQ(r.texture.width, k);
    EXPECT_EQ(r.texture.height, k);
    EXPECT_EQ(r.stats.byteCount, static_cast<size_t>(k) * k * 4);
    EXPECT_NEAR(r.stats.minHeightMeters, -50.0, 1e-9);
    EXPECT_NEAR(r.stats.maxHeightMeters, -50.0 + 10.0 * 16 + 3.0 * 16, 1e-9);
    for (int row = 0; row < k; ++row) {
        for (int col = 0; col < k; ++col) {
            const double expected = -50.0 + 10.0 * col + 3.0 * row;
            // 0.1m 编码步长 → 往返误差 ≤ 0.06m（与 Terrain-RGB 内容链同容差）。
            EXPECT_NEAR(HeightTextureCodec::decodeHeightAt(r.texture, col, row), expected,
                        0.06)
                << "col " << col << " row " << row;
        }
    }
}

TEST(HeightTextureCodec, InvalidInputYieldsEmpty) {
    const HeightTextureResult r = HeightTextureCodec::encode({}, 4, 4);
    EXPECT_FALSE(r.texture.valid());
    EXPECT_EQ(r.stats.byteCount, 0u);
}

TEST(HeightTextureCodec, AlphaChannelAlways255AndBlackIsFloor) {
    constexpr int k = 2;
    const std::vector<double> h(static_cast<size_t>(k) * k, -10000.0); // 全域底值
    const HeightTextureResult r = HeightTextureCodec::encode(h, k, k);
    ASSERT_TRUE(r.texture.valid());
    for (const auto v : r.texture.rgba8) {
        (void)v;
    }
    // 每像素 alpha=255；RGB=0 → 回读 -10000。
    for (int row = 0; row < k; ++row) {
        for (int col = 0; col < k; ++col) {
            const uint8_t* px =
                r.texture.rgba8.data() + (static_cast<size_t>(row) * k + col) * 4;
            EXPECT_EQ(px[3], 255);
            EXPECT_DOUBLE_EQ(HeightTextureCodec::decodeHeightAt(r.texture, col, row),
                             -10000.0);
        }
    }
}

TEST(HeightTextureCodec, OutOfRangeClampedNotWrapped) {
    constexpr int k = 2;
    const std::vector<double> h = {1e7, -2e4, 500.0, 0.0}; // 超量程两端
    const HeightTextureResult r = HeightTextureCodec::encode(h, k, k);
    ASSERT_TRUE(r.texture.valid());
    const double hi = HeightTextureCodec::decodeHeightAt(r.texture, 0, 0); // 1e7 → 上界
    const double lo = HeightTextureCodec::decodeHeightAt(r.texture, 1, 0); // -2e4 → 下界
    EXPECT_DOUBLE_EQ(hi, kTerrainRgbUpperMeters); // 钳到 24bit 上界，不绕回
    EXPECT_DOUBLE_EQ(lo, -10000.0);               // 下界钳到 -10000
    EXPECT_GT(hi, 1000000.0);                     // 上界量级 ~1.67M m
}
