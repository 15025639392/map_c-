// 同级共享边审计（T-V5「瓦界 <1m」取证仪器）——
//
// 两种源配准模型的机制对照：
// 1. **顶点栅格（带共享边界 post）**：相邻瓦共享边像素按经纬求值一致 → 双侧网格
//    共享边应逐点重合（≈0）——本仓既有 mesh 无缝契约在审计口径下的再确认。
// 2. **无重叠环的连续栅格**（本仓 assets 实测形态：相邻瓦边界 posts 相距 1 像元、
//    无共享列）：贴边 CLAMP 采样让西瓦东沿取到其最后一个 post、东瓦西沿取到其
//    第一个 post，两者相差 |坡度|×像元间距 → 审计如实报出差距（回归门禁：
//    该差距只能由带重叠环源（B2）或边 LUT/吸附（B4）关闭，不能靠调采样糊掉）。
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/SeamAudit.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/content/TerrainTileMesh.h"
#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

constexpr int kGrid = 17; // 高度图 17×17
constexpr int kNodes = 8; // 网格每边 8 段 → 9 节点

// 按经纬（经 tile 像素↔经纬映射）求值的高程场——跨瓦一致的世界函数。
double hillWorld(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 120.0) * std::cos(c.latitude() * 140.0);
}

// 顶点栅格瓦：像素 (col,row) 高度 = 世界函数在像素地理坐标处取值。
TerrainFrameAssembler::Frame buildVertexGridFrame(const WebMercatorTileScheme& scheme,
                                                   const TileKey& key) {
    std::vector<double> h(static_cast<size_t>(kGrid) * kGrid);
    std::vector<double> zeros(static_cast<size_t>(kGrid) * kGrid, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), kGrid, kGrid);
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            h[static_cast<size_t>(row) * kGrid + col] =
                hillWorld(probe.pixelToCartographic(col, row));
        }
    }
    const HeightmapTile tile(scheme, key, h.data(), kGrid, kGrid);
    const TerrainTileMeshBuilder builder;
    TerrainFrameAssembler::Frame frame;
    frame.key = key;
    frame.mesh = builder.build(tile, Ellipsoid::WGS84(), kNodes);
    return frame;
}

// 无重叠环连续栅格（assets 实测形态）：全局统一 post 网格，每瓦 kGrid 个 post
// （像素 p 的全局下标 = tileIndex·kGrid + p，瓦界两侧 posts 恰相邻、无共享列）。
// 高度 = 平面 kSlopePerPixel·(gx + gy) → 任何跨瓦边界的相邻 post 差 = 1 步 = g 米。
constexpr double kSlopePerPixel = 10.0;

double continuumPlane(const TileKey& key, int col, int row) {
    const int gx = key.x() * kGrid + col;
    const int gy = key.y() * kGrid + row;
    return kSlopePerPixel * static_cast<double>(gx + gy);
}

TerrainFrameAssembler::Frame buildPixelFrame(const WebMercatorTileScheme& scheme,
                                             const TileKey& key,
                                             double (*fn)(const TileKey&, int, int)) {
    std::vector<double> h(static_cast<size_t>(kGrid) * kGrid);
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            h[static_cast<size_t>(row) * kGrid + col] = fn(key, col, row);
        }
    }
    const HeightmapTile tile(scheme, key, h.data(), kGrid, kGrid);
    const TerrainTileMeshBuilder builder;
    TerrainFrameAssembler::Frame frame;
    frame.key = key;
    frame.mesh = builder.build(tile, Ellipsoid::WGS84(), kNodes);
    return frame;
}

} // namespace

TEST(SeamAudit, VertexGridSharedEdgesAreCoincident) {
    // 顶点栅格源（相邻瓦边界像素按经纬一致）→ 共享边 ECEF 逐点重合（T-V5 前提）。
    const WebMercatorTileScheme scheme;
    std::vector<TerrainFrameAssembler::Frame> frames;
    frames.push_back(buildVertexGridFrame(scheme, TileKey(10, 100, 100)));
    frames.push_back(buildVertexGridFrame(scheme, TileKey(10, 101, 100))); // 东邻
    frames.push_back(buildVertexGridFrame(scheme, TileKey(10, 100, 101))); // 南邻

    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.edgePairsFound, 2);
    EXPECT_EQ(audit.comparedEdges, 2);
    EXPECT_EQ(audit.comparedNodePairs, 2 * (kNodes + 1));
    EXPECT_LT(audit.maxMeters, 1e-6);
    EXPECT_EQ(audit.nodePairsOverOneMeter, 0);
}

TEST(SeamAudit, SingleFrameHasNoEdges) {
    const WebMercatorTileScheme scheme;
    std::vector<TerrainFrameAssembler::Frame> frames;
    frames.push_back(buildVertexGridFrame(scheme, TileKey(10, 100, 100)));
    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.edgePairsFound, 0);
    EXPECT_EQ(audit.comparedEdges, 0);
    EXPECT_EQ(audit.maxMeters, 0.0);
}

TEST(SeamAudit, NoOverlapRingContinuumExposesPixelGap) {
    // 无重叠环连续栅格（assets 实测形态）：瓦界两侧 posts 相距 1 像元 →
    // 贴边网格差 = |坡度|×Δ（本测试 g=10m）。仪器如实报出，不掩盖。
    const WebMercatorTileScheme scheme;
    std::vector<TerrainFrameAssembler::Frame> frames;
    frames.push_back(buildPixelFrame(scheme, TileKey(10, 100, 100), continuumPlane));
    frames.push_back(buildPixelFrame(scheme, TileKey(10, 101, 100), continuumPlane));
    frames.push_back(buildPixelFrame(scheme, TileKey(10, 100, 101), continuumPlane));

    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.edgePairsFound, 2);
    EXPECT_EQ(audit.comparedEdges, 2);
    EXPECT_EQ(audit.comparedNodePairs, 2 * (kNodes + 1));
    // 每对节点差 ≈ kSlopePerPixel m（贴边 CLAMP 取到边界外/内的相邻 post）。
    EXPECT_NEAR(audit.maxMeters, kSlopePerPixel, 1e-6);
    EXPECT_NEAR(audit.meanMeters, kSlopePerPixel, 1e-6);
    EXPECT_EQ(audit.nodePairsOverOneMeter, audit.comparedNodePairs);
}
