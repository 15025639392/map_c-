#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/TerrainCameraPipeline.h"
#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/TerrainPicking.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/providers/TerrainRgbTileSource.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

/// 内存 RGB fixture 源：按 fn 编码生成 Terrain-RGB 行（记录请求 URL）。
class FixtureRgbBytesSource : public ITileBytesSource {
public:
    FixtureRgbBytesSource(std::function<double(const Cartographic&)> fn, int pixelSize)
        : fn_(std::move(fn)), pixelSize_(pixelSize) {}

    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey& key, const std::string& urlTemplate) const override {
        lastUrls_.push_back(urlTemplate);
        // 用 scheme 探针做像素↔地理映射。
        const WebMercatorTileScheme scheme;
        std::vector<double> zeros(static_cast<size_t>(pixelSize_ * pixelSize_), 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), pixelSize_, pixelSize_);
        std::vector<uint8_t> bytes(static_cast<size_t>(pixelSize_ * pixelSize_ * 3));
        for (int row = 0; row < pixelSize_; ++row) {
            for (int col = 0; col < pixelSize_; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                uint8_t r, g, b;
                HeightmapCodec::encodeTerrainRgbPixel(fn_(c), r, g, b);
                const size_t px = (static_cast<size_t>(row) * pixelSize_ + col) * 3;
                bytes[px] = r;
                bytes[px + 1] = g;
                bytes[px + 2] = b;
            }
        }
        return bytes;
    }

    const std::vector<std::string>& lastUrls() const { return lastUrls_; }

private:
    std::function<double(const Cartographic&)> fn_;
    int pixelSize_;
    mutable std::vector<std::string> lastUrls_;
};

/// 恒定返回畸形字节的源。
class TruncatedBytesSource : public ITileBytesSource {
public:
    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey&, const std::string&) const override {
        return std::vector<uint8_t>{1, 2, 3, 4}; // 长度不符
    }
};

/// 永远不可用的源。
class MissingBytesSource : public ITileBytesSource {
public:
    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey&, const std::string&) const override {
        return std::nullopt;
    }
};

} // namespace

TEST(TerrainRgbTileSource, DecodesFixtureBytesToGrid) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    constexpr int kSize = 17;
    FixtureRgbBytesSource bytes(hillFn, kSize);
    const TerrainRgbTileSource source(bytes, "https://t/{z}/{x}/{y}.png");

    const auto grid = source.requestHeights(scheme, key.value(), kSize);
    ASSERT_TRUE(grid.has_value());
    EXPECT_EQ(grid->width, kSize);
    EXPECT_EQ(grid->height, kSize);
    // 与 fn 直接求值逐点对照（0.1m 量化容差）。
    std::vector<double> zeros(static_cast<size_t>(kSize * kSize), 0.0);
    const HeightmapTile probe(scheme, key.value(), zeros.data(), kSize, kSize);
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const Cartographic c = probe.pixelToCartographic(col, row);
            const double expected = hillFn(c);
            EXPECT_NEAR(grid->heights[static_cast<size_t>(row * kSize + col)], expected, 0.06)
                << "row " << row << " col " << col;
        }
    }
    // URL 模板被正确替换后传给字节源。
    ASSERT_EQ(bytes.lastUrls().size(), 1u);
    EXPECT_EQ(bytes.lastUrls()[0],
              "https://t/" + std::to_string(key->z()) + "/" + std::to_string(key->x()) + "/" +
                  std::to_string(key->y()) + ".png");
}

TEST(TerrainRgbTileSource, RejectsMalformedAndMissing) {
    const WebMercatorTileScheme scheme;
    const TileKey key(9, 200, 100);
    // 长度不符 → nullopt。
    const TruncatedBytesSource trunc;
    const TerrainRgbTileSource s1(trunc, "https://t/{z}/{x}/{y}.png");
    EXPECT_FALSE(s1.requestHeights(scheme, key, 17).has_value());
    // 不可用 → nullopt。
    const MissingBytesSource missing;
    const TerrainRgbTileSource s2(missing, "https://t/{z}/{x}/{y}.png");
    EXPECT_FALSE(s2.requestHeights(scheme, key, 17).has_value());
}

TEST(TerrainRgbTileSource, FeedsCameraPipeline) {
    // 端到端：RGB fixture → TerrainRgbTileSource → camera 管线 → 帧 + 拾取。
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), 9000.0));
    const Vec3 up = e.geodeticSurfaceNormal(center);
    const CameraView camera(pos, e.cartographicToCartesian(center), up, degreesToRadians(60.0),
                            4.0 / 3.0);

    const WebMercatorTileScheme scheme;
    constexpr int kSize = 17;
    FixtureRgbBytesSource bytes(hillFn, kSize);
    const TerrainRgbTileSource source(bytes, "https://t/{z}/{x}/{y}.png");

    TerrainCameraPipelineConfig config;
    config.lod.maxScreenSpaceErrorPx = 8.0;
    config.lod.maxLevel = 11;
    config.gridSize = kSize;
    config.nodesPerEdge = 8;
    const auto frames = assembleTerrainFrameForCamera(scheme, camera, e, source, config);
    ASSERT_FALSE(frames.empty());
    // 中心射线拾取有地形（海拔在山丘值域）。
    const auto hit = pickTerrainFrame(camera.rayThroughNdc(0.0, 0.0).origin(),
                                      camera.rayThroughNdc(0.0, 0.0).direction(), frames);
    ASSERT_TRUE(hit.has_value());
    const Cartographic hitCarto = e.cartesianToCartographic(hit->point);
    EXPECT_GT(hitCarto.height(), 199.0);
    EXPECT_LT(hitCarto.height(), 801.0);
    // 所有请求 URL 形如 模板/{z}/{x}/{y}.png。
    ASSERT_FALSE(bytes.lastUrls().empty());
    for (const auto& u : bytes.lastUrls()) {
        EXPECT_EQ(u.find("https://t/"), 0u);
        EXPECT_EQ(u.substr(u.size() - 4), std::string(".png"));
    }
}
