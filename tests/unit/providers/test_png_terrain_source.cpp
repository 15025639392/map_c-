#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/providers/TerrainRgbPngTileSource.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

// ---------------------------------------------------------------------------
// 最小 PNG 编码器（测试 fixture 用）：8bit RGB、zlib stored-deflate。
// 由 stb（库内 STB_IMAGE_IMPLEMENTATION）解码验证——自足，不依赖第三方测试头。
// ---------------------------------------------------------------------------
class Crc32 {
public:
    Crc32() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table_[i] = c;
        }
    }
    uint32_t update(uint32_t crc, const uint8_t* data, size_t n) const {
        crc = crc ^ 0xFFFFFFFFu;
        for (size_t i = 0; i < n; ++i) {
            crc = table_[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }

private:
    uint32_t table_[256];
};

void appendBigEndian32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void appendPngChunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    static const Crc32 crc;
    appendBigEndian32(out, static_cast<uint32_t>(data.size()));
    const size_t typePos = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t c = crc.update(0, out.data() + typePos, 4 + data.size());
    appendBigEndian32(out, c);
}

/// 生成 RGB 行（filter 0）→ zlib stored → PNG 字节。
std::vector<uint8_t> writePngRgb(const std::vector<uint8_t>& rgb, int w, int h) {
    std::vector<uint8_t> png;
    png.insert(png.end(), {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
    // IHDR
    std::vector<uint8_t> ihdr;
    appendBigEndian32(ihdr, static_cast<uint32_t>(w));
    appendBigEndian32(ihdr, static_cast<uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0}); // bit8, color RGB
    appendPngChunk(png, "IHDR", ihdr);
    // IDAT：每行前插 filter 0，然后 zlib stored 块。
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (w * 3 + 1));
    for (int row = 0; row < h; ++row) {
        raw.push_back(0);
        const size_t off = static_cast<size_t>(row) * w * 3;
        raw.insert(raw.end(), rgb.begin() + static_cast<long>(off),
                   rgb.begin() + static_cast<long>(off) + static_cast<size_t>(w) * 3);
    }
    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        const size_t remain = raw.size() - pos;
        const size_t len = remain < 65535 ? remain : 65535;
        const bool final = (pos + len == raw.size());
        zlib.push_back(static_cast<uint8_t>((final ? 1 : 0) | (0 << 1))); // stored 块头
        zlib.push_back(static_cast<uint8_t>(len & 0xFF));
        zlib.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        const uint16_t nlen = static_cast<uint16_t>(~len & 0xFFFF);
        zlib.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<long>(pos),
                    raw.begin() + static_cast<long>(pos + len));
        pos += len;
    }
    // zlib adler-32（两轮求和）。
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for (const uint8_t b : raw) {
        s1 = (s1 + b) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    const uint32_t adlerFinal = (s2 << 16) | s1;
    appendBigEndian32(zlib, adlerFinal);
    appendPngChunk(png, "IDAT", zlib);
    // IEND
    appendPngChunk(png, "IEND", {});
    return png;
}

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
