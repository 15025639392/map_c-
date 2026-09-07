#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../util/min_png_writer.h"
#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/providers/TerrainRgbPngTileSource.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

using mapc_test::writePngRgb;

/// PNG fixture 字节源：每瓦按 fn 编码 → 自写 PNG。
class PngFixtureBytesSource : public ITileBytesSource {
public:
    PngFixtureBytesSource(std::function<double(const Cartographic&)> fn, int pixelSize)
        : fn_(std::move(fn)), pixelSize_(pixelSize) {}

    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey& key, const std::string& urlTemplate) const override {
        lastUrls_.push_back(urlTemplate);
        const WebMercatorTileScheme scheme;
        std::vector<double> zeros(static_cast<size_t>(pixelSize_ * pixelSize_), 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), pixelSize_, pixelSize_);
        std::vector<uint8_t> rgb(static_cast<size_t>(pixelSize_ * pixelSize_ * 3));
        for (int row = 0; row < pixelSize_; ++row) {
            for (int col = 0; col < pixelSize_; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                uint8_t r, g, b;
                HeightmapCodec::encodeTerrainRgbPixel(fn_(c), r, g, b);
                const size_t px = (static_cast<size_t>(row) * pixelSize_ + col) * 3;
                rgb[px] = r;
                rgb[px + 1] = g;
                rgb[px + 2] = b;
            }
        }
        return writePngRgb(rgb, pixelSize_, pixelSize_);
    }

    const std::vector<std::string>& lastUrls() const { return lastUrls_; }

private:
    std::function<double(const Cartographic&)> fn_;
    int pixelSize_;
    mutable std::vector<std::string> lastUrls_;
};

/// 返回非 PNG 垃圾字节。
class GarbageBytesSource : public ITileBytesSource {
public:
    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey&, const std::string&) const override {
        return std::vector<uint8_t>{0x89, 'G', 'A', 'R', 'B', 'A', 'G', 'E'};
    }
};

} // namespace

TEST(TerrainRgbPngTileSource, DecodesPngFixtureToGrid) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    constexpr int kSize = 17;
    PngFixtureBytesSource bytes(hillFn, kSize);
    const TerrainRgbPngTileSource source(bytes, "https://t/{z}/{x}/{y}.png");

    const auto grid = source.requestHeights(scheme, key.value(), kSize);
    ASSERT_TRUE(grid.has_value());
    EXPECT_EQ(grid->width, kSize);
    EXPECT_EQ(grid->height, kSize);
    std::vector<double> zeros(static_cast<size_t>(kSize * kSize), 0.0);
    const HeightmapTile probe(scheme, key.value(), zeros.data(), kSize, kSize);
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const Cartographic c = probe.pixelToCartographic(col, row);
            EXPECT_NEAR(grid->heights[static_cast<size_t>(row * kSize + col)], hillFn(c), 0.06)
                << "row " << row << " col " << col;
        }
    }
    ASSERT_EQ(bytes.lastUrls().size(), 1u);
    EXPECT_EQ(bytes.lastUrls()[0].find("https://t/"), 0u);
}

TEST(TerrainRgbPngTileSource, RejectsGarbageAndSizeMismatch) {
    const WebMercatorTileScheme scheme;
    const TileKey key(9, 200, 100);
    const GarbageBytesSource garbage;
    const TerrainRgbPngTileSource s1(garbage, "https://t/{z}/{x}/{y}.png");
    EXPECT_FALSE(s1.requestHeights(scheme, key, 17).has_value());

    // 尺寸不符：fixture 16² PNG，但请求 17 → nullopt。
    PngFixtureBytesSource bytes(hillFn, 16);
    const TerrainRgbPngTileSource s2(bytes, "https://t/{z}/{x}/{y}.png");
    EXPECT_FALSE(s2.requestHeights(scheme, key, 17).has_value());
}
