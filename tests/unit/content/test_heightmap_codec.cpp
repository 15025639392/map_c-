#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapSampler.h"

using namespace earth_engine;

TEST(HeightmapCodec, TerrainRgbKnownPixels) {
    // (0,0,0) → -10000 m（编码值 0）。
    EXPECT_EQ(HeightmapCodec::decodeTerrainRgbPixel(0, 0, 0), -10000.0);
    // 编码值 100000 → 0 m：65536 + 134·256 + 160 = 100000。
    EXPECT_NEAR(HeightmapCodec::decodeTerrainRgbPixel(1, 134, 160), 0.0, 1.0e-9);
    // 编码值 65536（=1m 档）×10：R=1 → 0.1·65536·? 直接验：R=1 全零 → 6553.6-10000。
    EXPECT_NEAR(HeightmapCodec::decodeTerrainRgbPixel(1, 0, 0), -3446.4, 1.0e-9);
    // 全 255 → -10000 + 16777215·0.1 = 1667721.5 m。
    EXPECT_NEAR(HeightmapCodec::decodeTerrainRgbPixel(255, 255, 255), 1667721.5, 1.0e-6);
}

TEST(HeightmapCodec, TerrariumKnownPixels) {
    EXPECT_EQ(HeightmapCodec::decodeTerrariumPixel(0, 0, 0), -32768.0);
    EXPECT_NEAR(HeightmapCodec::decodeTerrariumPixel(128, 0, 0), 0.0, 1.0e-9);
    // 全 255：65280+255+255/256 = 65535.9961 - 32768 = 32767.9961。
    EXPECT_NEAR(HeightmapCodec::decodeTerrariumPixel(255, 255, 255), 32767.99609375, 1.0e-6);
}

TEST(HeightmapCodec, TerrainRgbEncodeDecodeRoundTrip) {
    uint8_t r = 0, g = 0, b = 0;
    // 0 m → 编码值 100000 → (1,134,160)。
    HeightmapCodec::encodeTerrainRgbPixel(0.0, r, g, b);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(g, 134);
    EXPECT_EQ(b, 160);
    EXPECT_NEAR(HeightmapCodec::decodeTerrainRgbPixel(r, g, b), 0.0, 1.0e-9);

    // 0.1 m 量化往返：误差 ≤ 0.05 m。
    for (const double h : {-3000.5, -1.0, 0.0, 12.3, 456.78, 8848.86, 1500.05}) {
        HeightmapCodec::encodeTerrainRgbPixel(h, r, g, b);
        const double back = HeightmapCodec::decodeTerrainRgbPixel(r, g, b);
        EXPECT_NEAR(back, h, 0.05 + 1.0e-6) << "height " << h;
    }

    // 越界钳制：极低 → (0,0,0)；极高 → (255,255,255)。
    HeightmapCodec::encodeTerrainRgbPixel(-20000.0, r, g, b);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 0);
    HeightmapCodec::encodeTerrainRgbPixel(2.0e6, r, g, b);
    EXPECT_EQ(r, 255);
    EXPECT_EQ(g, 255);
    EXPECT_EQ(b, 255);
}

TEST(HeightmapCodec, DecodeBufferWithRowPadding) {
    // 2 行 × 3 列 RGB，行尾 padding 7 字节（模拟解码后带对齐的缓冲），stride = 9 + 7。
    constexpr size_t kWidth = 3;
    constexpr size_t kRowPad = 7;
    const size_t stride = kWidth * 3 + kRowPad;
    std::vector<uint8_t> pixels(2 * stride, 0xAA); // padding 用非零值，验证不越界读
    const std::vector<double> heights = {100.0, -50.0, 3000.0, 0.0, 25.5, 8848.86};
    for (size_t row = 0; row < 2; ++row) {
        for (size_t col = 0; col < kWidth; ++col) {
            const size_t px = row * stride + col * 3;
            uint8_t r, g, b;
            HeightmapCodec::encodeTerrainRgbPixel(heights[row * kWidth + col], r, g, b);
            pixels[px] = r;
            pixels[px + 1] = g;
            pixels[px + 2] = b;
        }
    }
    std::vector<double> out(2 * kWidth, -1.0);
    ASSERT_TRUE(HeightmapCodec::decodeTerrainRgb(pixels.data(), kWidth, 2, stride, out.data()));
    for (size_t i = 0; i < heights.size(); ++i) {
        EXPECT_NEAR(out[i], heights[i], 0.05 + 1.0e-6) << "idx " << i;
    }
}

TEST(HeightmapCodec, InvalidArguments) {
    double out[4] = {0, 0, 0, 0};
    uint8_t px[12] = {0};
    EXPECT_FALSE(HeightmapCodec::decodeTerrainRgb(nullptr, 2, 2, 6, out));
    EXPECT_FALSE(HeightmapCodec::decodeTerrainRgb(px, 2, 2, 6, nullptr));
    EXPECT_FALSE(HeightmapCodec::decodeTerrainRgb(px, 0, 2, 6, out));
    EXPECT_FALSE(HeightmapCodec::decodeTerrainRgb(px, 2, 2, 5, out)); // stride 不够 3 字节/px
}

TEST(HeightmapCodec, DecodeThenSampleSmoke) {
    // 合成 4×4 高度面 h = 10·col + row，编码→解码→双线性采样中心。
    constexpr int kSize = 4;
    std::vector<uint8_t> pixels(kSize * kSize * 3);
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            uint8_t r, g, b;
            HeightmapCodec::encodeTerrainRgbPixel(10.0 * col + row, r, g, b);
            const size_t px = (row * kSize + col) * 3;
            pixels[px] = r;
            pixels[px + 1] = g;
            pixels[px + 2] = b;
        }
    }
    std::vector<double> heights(kSize * kSize);
    ASSERT_TRUE(HeightmapCodec::decodeTerrainRgb(pixels.data(), kSize, kSize, kSize * 3,
                                                 heights.data()));
    const HeightmapSampler sampler(heights.data(), kSize, kSize);
    // 中心 (1.5,1.5)：解析值 10·1.5+1.5 = 16.5；量化误差 ≤ ~0.1。
    EXPECT_NEAR(sampler.sampleBilinear(1.5, 1.5), 16.5, 0.15);
    EXPECT_NEAR(sampler.sampleBilinear(0.0, 0.0), 0.0, 0.05);
    EXPECT_NEAR(sampler.sampleBilinear(3.0, 3.0), 33.0, 0.05);
}
