// ImageryTileSource：退化决议 × 字节源 × PNG→纹理数据 的 S4 装配链（host 可测）。
#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../util/min_png_writer.h"
#include "earth_engine/imagery/ImageryTileSource.h"
#include "earth_engine/providers/ITileBytesSource.h"
#include "earth_engine/renderer/IRenderDevice.h"
#include "earth_engine/tiling/TileKey.h"

using namespace earth_engine;

namespace {

/// 按 URL 末位（z/x/y 路径）生成 2×2 纯色 PNG 的假字节源；记录请求 URL。
class PngBytesSource : public ITileBytesSource {
public:
    std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey&,
                                                         const std::string& url) const override {
        lastUrls_.push_back(url);
        // 用 URL 种一个小 RGB 值，便于断言"取的是哪一瓦"。
        const uint8_t seed = static_cast<uint8_t>(url.size() % 251u + 1u);
        const std::vector<uint8_t> rgb = {
            seed, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0,
        };
        return mapc_test::writePngRgb(rgb, 2, 2);
    }
    mutable std::vector<std::string> lastUrls_;
};

// 按 zoom 范围判定可用（含）：minZ..maxZ。
auto zRange(int minZ, int maxZ) {
    return [minZ, maxZ](const TileKey& k) { return k.z() >= minZ && k.z() <= maxZ; };
}

} // namespace

TEST(ImageryTileSource, FetchesOwnTileWhenAvailable) {
    PngBytesSource bytes;
    ImageryTileSource src(bytes, "https://i/{z}/{x}/{y}.png", zRange(6, 12));
    const auto r = src.fetchTexture(TileKey(9, 200, 100));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->resolvedKey, TileKey(9, 200, 100)); // 自身可用 → 自身
    EXPECT_TRUE(r->texture.valid());
    EXPECT_EQ(r->texture.width, 2);
    ASSERT_FALSE(bytes.lastUrls_.empty());
    EXPECT_EQ(bytes.lastUrls_.back(), "https://i/9/200/100.png"); // 自身 URL
}

TEST(ImageryTileSource, FetchesNearestAvailableAncestorWhenChildMissing) {
    PngBytesSource bytes;
    ImageryTileSource src(bytes, "https://i/{z}/{x}/{y}.png", zRange(6, 8)); // 只到 z8
    const auto r = src.fetchTexture(TileKey(9, 200, 100));                   // z9 缺 → z8 祖
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->resolvedKey, TileKey(8, 100, 50));
    EXPECT_EQ(bytes.lastUrls_.back(), "https://i/8/100/50.png"); // 取的是祖先瓦真实数据
}

TEST(ImageryTileSource, EntireChainMissingIsEmpty) {
    PngBytesSource bytes;
    ImageryTileSource src(bytes, "https://i/{z}/{x}/{y}.png", zRange(10, 12));
    EXPECT_FALSE(src.fetchTexture(TileKey(6, 32, 16)).has_value());
    EXPECT_TRUE(bytes.lastUrls_.empty()); // 决议空 → 不发任何请求
}

TEST(ImageryTileSource, BadBytesYieldEmpty) {
    class EmptyBytes : public ITileBytesSource {
    public:
        std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey&,
                                                             const std::string&) const override {
            return std::vector<uint8_t>{0x89, 'G', 'A', 'R', 'B'};
        }
    };
    EmptyBytes bytes;
    ImageryTileSource src(bytes, "https://i/{z}/{x}/{y}.png", zRange(0, 20));
    EXPECT_FALSE(src.fetchTexture(TileKey(5, 10, 5)).has_value());
}
