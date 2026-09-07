// NASA Terrain-RGB 514 源（Mapbox 标准：512 cell + 1px 裙边环）——B2 环模式的
// providers 级落地与真实端点规格回归。
//
// 真实端点（用户指定，2026-09-09 实测）：`https://mapoverlay.xinzhi.space/3dterrain/
// nasa/tiles/{z}/{x}/{y}.png` —— PNG **514×514** Terrain-RGB；环布局 = cell-
// registered + 1px 邻瓦回填（实测 A.col512 == B.col0 逐行一致 514/514）；覆盖
// **z6–12**（z13 实测 404）。本套件用同规格 fixture 走全链（字节→PNG→Terrain-
// RGB→栅格→网格→SeamAudit），另附 env 开关的真实端点烟测（MAPC_LIVE_NET=1）。
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../util/min_png_writer.h"

#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/SeamAudit.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/content/TerrainTileMesh.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/providers/CurlBytesSource.h"
#include "earth_engine/providers/TerrainRgbPngTileSource.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

constexpr int kCells = 32;        // fixture 用 32 cell（512 全尺寸太慢；规格同构）
constexpr int kRaster = kCells + 2; // 环缓冲 34

// 与真实源同构的缓冲：像素 p ↔ world = tx·C + (p-1) + 0.5（cell 中心；px0/pxC+1 =
// 邻瓦相邻 cell = 环）。连续平面场 → 环即邻瓦真实数据。
double worldField(double wx, double wy) { return 400.0 + 1.5 * (wx + wy); }

std::vector<uint8_t> encodeRingPng(int tx, int ty) {
    std::vector<uint8_t> rgb(static_cast<size_t>(kRaster) * kRaster * 3);
    for (int py = 0; py < kRaster; ++py) {
        const double wy = static_cast<double>(ty * kCells + (py - 1)) + 0.5;
        for (int px = 0; px < kRaster; ++px) {
            const double wx = static_cast<double>(tx * kCells + (px - 1)) + 0.5;
            uint8_t r, g, b;
            HeightmapCodec::encodeTerrainRgbPixel(worldField(wx, wy), r, g, b);
            const size_t off = (static_cast<size_t>(py) * kRaster + px) * 3;
            rgb[off] = r;
            rgb[off + 1] = g;
            rgb[off + 2] = b;
        }
    }
    return mapc_test::writePngRgb(rgb, kRaster, kRaster);
}

// 按瓦生成 ring PNG 的内存字节源（tx/ty 从瓦键 x/y 映射）。
class RingPngBytesSource : public ITileBytesSource {
public:
    std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey& key,
                                                         const std::string&) const override {
        return encodeRingPng(key.x(), key.y());
    }
};

TerrainFrameAssembler::Frame frameFromGrid(const WebMercatorTileScheme& scheme,
                                           const TileKey& key, const TerrainGrid& grid) {
    const HeightmapTile tile(scheme, key, grid.heights.data(), grid.width, grid.height,
                             grid.noDataValues.data(),
                             static_cast<int>(grid.noDataValues.size()), grid.borderInset);
    TerrainFrameAssembler::Frame frame;
    frame.key = key;
    frame.mesh = TerrainTileMeshBuilder().build(tile, Ellipsoid::WGS84(), 16);
    return frame;
}

} // namespace

TEST(NasaRingSource, RingModeDecodesGridSpec) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 100, 100);
    RingPngBytesSource bytes;
    const TerrainRgbPngTileSource source(bytes, "https://t/{z}/{x}/{y}.png",
                                         /*cellRegisteredRing=*/true, /*minZoom=*/6,
                                         /*maxZoom=*/12);
    const auto grid = source.requestHeights(scheme, key, kCells);
    ASSERT_TRUE(grid.has_value());
    EXPECT_EQ(grid->width, kRaster); // 512 cell + 1px 环
    EXPECT_EQ(grid->height, kRaster);
    EXPECT_DOUBLE_EQ(grid->borderInset, 0.5);
    ASSERT_EQ(grid->noDataValues.size(), 1u); // Terrain-RGB 隐式哨兵照常注册
    EXPECT_DOUBLE_EQ(grid->noDataValues[0], HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    // 尺寸错配（环模式期望 C+2）：513 请求 → nullopt。
    EXPECT_FALSE(source.requestHeights(scheme, key, kRaster).has_value());
}

TEST(NasaRingSource, ZoomRangeRespected) {
    const WebMercatorTileScheme scheme;
    RingPngBytesSource bytes;
    const TerrainRgbPngTileSource source(bytes, "https://t/{z}/{x}/{y}.png", true, 6, 12);
    // 覆盖内 → 正常解码。
    EXPECT_TRUE(source.requestHeights(scheme, TileKey(6, 32, 32), kCells).has_value());
    EXPECT_TRUE(source.requestHeights(scheme, TileKey(12, 2048, 1024), kCells).has_value());
    // 覆盖外（z5 / z13；真实端点 z13 实测 404）→ nullopt 不冒充。
    EXPECT_FALSE(source.requestHeights(scheme, TileKey(5, 16, 16), kCells).has_value());
    EXPECT_FALSE(source.requestHeights(scheme, TileKey(13, 4096, 2048), kCells).has_value());
}

TEST(NasaRingSource, AdjacentRingTilesSeamCoincidentThroughProvider) {
    // 真实规格（514 同构 fixture）：provider 全链解码两邻瓦 → 帧级同级共享边
    // SeamAudit ≈0（B2 闭合在真实字节形态上成立）。
    const WebMercatorTileScheme scheme;
    RingPngBytesSource bytes;
    const TerrainRgbPngTileSource source(bytes, "https://t/{z}/{x}/{y}.png", true, 0, 20);
    const TileKey west(10, 100, 100);
    std::vector<TerrainFrameAssembler::Frame> frames;
    const auto gw = source.requestHeights(scheme, west, kCells);
    const auto ge = source.requestHeights(scheme, TileKey(10, 101, 100), kCells);
    const auto gs = source.requestHeights(scheme, TileKey(10, 100, 101), kCells);
    ASSERT_TRUE(gw && ge && gs);
    frames.push_back(frameFromGrid(scheme, west, *gw));
    frames.push_back(frameFromGrid(scheme, TileKey(10, 101, 100), *ge));
    frames.push_back(frameFromGrid(scheme, TileKey(10, 100, 101), *gs));
    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.edgePairsFound, 2);
    EXPECT_EQ(audit.comparedNodePairs, 2 * (16 + 1));
    EXPECT_LT(audit.maxMeters, 1e-6); // 环源共享边逐点重合
    EXPECT_EQ(audit.nodePairsOverOneMeter, 0);
}

TEST(NasaRingSource, TileMinMaxExcludesRingPixels) {
    // 环 = 邻瓦数据：min/max 只统计本瓦 cell 区（1..C），不含 px0/pxC+1 环列行。
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 100, 100);
    RingPngBytesSource bytes;
    const TerrainRgbPngTileSource source(bytes, "https://t/{z}/{x}/{y}.png", true, 0, 20);
    const auto grid = source.requestHeights(scheme, key, kCells);
    ASSERT_TRUE(grid.has_value());
    const HeightmapTile tile(scheme, key, grid->heights.data(), grid->width, grid->height,
                             grid->noDataValues.data(),
                             static_cast<int>(grid->noDataValues.size()), grid->borderInset);
    const auto [minH, maxH] = tile.minMaxHeight();
    // 平面场（瓦 tx=ty=100，C=32）：cell 区（row/col ∈ [1..32]）min 在 (px1,row1)、
    // max 在 (px32,row32)；环 px0 比 cellMin 更低（在瓦西界外半像元）。
    const double base = static_cast<double>(100 * kCells);
    const double cellMin = worldField(base + 0.5, base + 0.5);
    const double cellMax = worldField(base + static_cast<double>(kCells - 1) + 0.5,
                                      base + static_cast<double>(kCells - 1) + 0.5);
    EXPECT_NEAR(minH, cellMin, 0.06); // Terrain-RGB 0.1m 量化容差
    EXPECT_NEAR(maxH, cellMax, 0.06);
    // 环 px0 比 cell min 更低 → 若未排除 min 会被拉低。
    EXPECT_LT(worldField(base - 0.5, base - 0.5), minH);
}

// ---- 真实端点烟测（env MAPC_LIVE_NET=1 才跑；离线 CI 自动跳过） -------------

TEST(NasaRingSourceLive, RealEndpointTileAndZoomCoverage) {
    if (std::getenv("MAPC_LIVE_NET") == nullptr) {
        GTEST_SKIP() << "设 MAPC_LIVE_NET=1 跑真实端点（https://mapoverlay.xinzhi.space…）";
    }
    const WebMercatorTileScheme scheme;
    const CurlBytesSource curl(/*timeoutMs=*/15000);
    const TerrainRgbPngTileSource source(curl,
                                         "https://mapoverlay.xinzhi.space/3dterrain/nasa/tiles/"
                                         "{z}/{x}/{y}.png",
                                         /*cellRegisteredRing=*/true, /*minZoom=*/6,
                                         /*maxZoom=*/12);
    // 缙云山 z12 中心瓦：真实字节 514² → 栅格 spec + 高度可解码。
    const auto grid = source.requestHeights(scheme, TileKey(12, 3259, 1693), 512);
    ASSERT_TRUE(grid.has_value()) << "真实端点不可达或瓦缺失";
    EXPECT_EQ(grid->width, 514);
    EXPECT_EQ(grid->height, 514);
    EXPECT_DOUBLE_EQ(grid->borderInset, 0.5);
    double mn = 1e30, mx = -1e30;
    for (const double v : grid->heights) {
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    EXPECT_GT(mn, 50.0); // 缙云山脚（实测 interior 165–630）
    EXPECT_LT(mx, 1200.0);
    // z13（覆盖外，实测 404）→ nullopt。
    EXPECT_FALSE(
        source.requestHeights(scheme, TileKey(13, 6518, 3387), 512).has_value());
}
