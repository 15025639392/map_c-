// 带重叠环源（cell-registered + 1px 邻瓦回填）→ 同级共享边闭合（B2 能力切片）。
//
// gis-md 语义转写：其 DecodedHeightmap 以 borderInset=0.5 采样使相邻瓦在共享边界
// 读到同一批真实世界样本（test_decoded_heightmap_sampler.cpp BorderInsetHalfPixelSeamless
// / SameLevelEastWestEdgeBitwiseEqual）。本文件把它对到本仓形态：瓦数据缓冲 = 每瓦
// C 个 cell + 左右/上下各 1px 邻瓦回填环（缓冲宽 = C+2），采样像素内缩半像元，
// 网格贴瓦界落位（与顶点栅格默认路径共用同一 builder）。
//
// 对照（实测登记，a4-merge-plan §7）：本仓内置 assets 为**无环**连续栅格 → 同级边
// mesh 差 ≈|坡度|×像元（z13 均值 1.8m）；本测试证明**带环源**下 SeamAudit ≈ 0，
// 即该差距的关闭路径 = 源带环（或解码侧回填环），不是调采样糊掉。
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "earth_engine/content/HeightmapSampler.h"
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

constexpr int kCells = 8;           // 每瓦 cell 数
constexpr int kBuffer = kCells + 2; // + 1px 回填环 → 10×10 缓冲
constexpr double kSlope = 1.5;      // 平面场斜率（米/世界单位）
constexpr double kInset = 0.5;

// 全局平面场（抽象世界单位，行/列同斜率）——瓦 (tx,ty) 覆盖 [tx·C, (tx+1)·C]。
double worldField(double wx, double wy) { return kSlope * (wx + wy) + 100.0; }

// 瓦 (tx,ty) 的 cell-registered + 环缓冲：像素 p ↔ wx = tx·C + (p-1) + 0.5
// （p∈[1..C] 为本瓦 cell 中心；p=0 / p=C+1 为西/东邻瓦的相邻 cell —— 回填环）。
TerrainFrameAssembler::Frame buildRingFrame(const WebMercatorTileScheme& scheme,
                                            const TileKey& key, int tx, int ty) {
    std::vector<double> h(static_cast<size_t>(kBuffer) * kBuffer);
    for (int py = 0; py < kBuffer; ++py) {
        const double wy = static_cast<double>(ty * kCells + (py - 1)) + 0.5;
        for (int px = 0; px < kBuffer; ++px) {
            const double wx = static_cast<double>(tx * kCells + (px - 1)) + 0.5;
            h[static_cast<size_t>(py) * kBuffer + px] = worldField(wx, wy);
        }
    }
    const HeightmapTile tile(scheme, key, h.data(), kBuffer, kBuffer, nullptr, 0, kInset);
    const TerrainTileMeshBuilder builder;
    TerrainFrameAssembler::Frame frame;
    frame.key = key;
    frame.mesh = builder.build(tile, Ellipsoid::WGS84(), 8);
    return frame;
}

} // namespace

// gis-md BorderInsetHalfPixelSeamless 语义（采样层）：带环缓冲内缩半像元采样，
// 西瓦东界 == 东瓦西界 == 平面场在共享边界 (world x = C) 的值（位级一致）。
TEST(RingSourceSeam, SamplerHalfPixelInsetReadsSharedBoundary) {
    constexpr int C = kCells;
    constexpr int W = kBuffer;
    const double sentinel = -10000.0; // 表非空占位（不参与匹配）
    // 西瓦 (0,0) 缓冲：p ↔ (p-1)+0.5；边界像素 = px 0.5（world x = C 处）。
    std::vector<double> west(static_cast<size_t>(W) * W);
    std::vector<double> east(static_cast<size_t>(W) * W);
    for (int py = 0; py < W; ++py) {
        const double wy = static_cast<double>(py - 1) + 0.5;
        for (int px = 0; px < W; ++px) {
            west[static_cast<size_t>(py) * W + px] =
                worldField(static_cast<double>(px - 1) + 0.5, wy);
            // 东瓦 (1,0)：p ↔ C + (p-1) + 0.5
            east[static_cast<size_t>(py) * W + px] =
                worldField(static_cast<double>(C + px - 1) + 0.5, wy);
        }
    }
    // 内缩采样像素 = inset + f·((W-1) − 2·inset)，f 为边界分数。
    const double span = static_cast<double>(W - 1) - 2.0 * kInset; // = C
    const double westEast = HeightmapSampler(west.data(), W, W, &sentinel, 1)
                                .sampleBilinear(kInset + 1.0 * span, 4.5); // 西瓦东界
    const double eastWest = HeightmapSampler(east.data(), W, W, &sentinel, 1)
                                .sampleBilinear(kInset + 0.0 * span, 4.5); // 东瓦西界
    const double rowY = 4.0; // 采样行中心的 world y
    const double expected = worldField(static_cast<double>(C), rowY); // 共享边界 x=C
    EXPECT_NEAR(westEast, expected, 1e-6); // 位级同一批样本 → 与解析平面一致
    EXPECT_NEAR(eastWest, expected, 1e-6);
    EXPECT_DOUBLE_EQ(westEast, eastWest); // 无缝：两侧同参数运算 → 逐位相等
}

// 带环源 → 帧级同级共享边 ECEF 逐点重合（SeamAudit ≈ 0）。
// 对照 test_seam_audit 的 NoOverlapRingContinuumExposesPixelGap：无环源报 |坡度|×Δ；
// 带环源应闭合——这是"assets 1.8–4.7m 边差"的机制关闭路径（B2）。
TEST(RingSourceSeam, MeshSharedEdgesCoincideWithOverlapRing) {
    const WebMercatorTileScheme scheme;
    std::vector<TerrainFrameAssembler::Frame> frames;
    frames.push_back(buildRingFrame(scheme, TileKey(10, 100, 100), 0, 0));
    frames.push_back(buildRingFrame(scheme, TileKey(10, 101, 100), 1, 0)); // 东邻
    frames.push_back(buildRingFrame(scheme, TileKey(10, 100, 101), 0, 1)); // 南邻

    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.edgePairsFound, 2);
    EXPECT_EQ(audit.comparedEdges, 2);
    EXPECT_EQ(audit.comparedNodePairs, 2 * (8 + 1));
    EXPECT_LT(audit.maxMeters, 1e-6); // 与无环源的 ~10m fixture 成对照
    EXPECT_EQ(audit.nodePairsOverOneMeter, 0);
}

// 网格段数与 cell 数互质/无关：无论 nodesPerEdge 取值，带环源共享边都闭合。
TEST(RingSourceSeam, MeshSeamIndependentOfNodesPerEdge) {
    const WebMercatorTileScheme scheme;
    std::vector<TerrainFrameAssembler::Frame> frames;
    // nodesPerEdge=5（与 C=8 互质）：节点采样窗全落在半像元上，仍须一致。
    auto frame = [&](const TileKey& key, int tx, int ty) {
        std::vector<double> h(static_cast<size_t>(kBuffer) * kBuffer);
        for (int py = 0; py < kBuffer; ++py) {
            const double wy = static_cast<double>(ty * kCells + (py - 1)) + 0.5;
            for (int px = 0; px < kBuffer; ++px) {
                const double wx = static_cast<double>(tx * kCells + (px - 1)) + 0.5;
                h[static_cast<size_t>(py) * kBuffer + px] = worldField(wx, wy);
            }
        }
        const HeightmapTile tile(scheme, key, h.data(), kBuffer, kBuffer, nullptr, 0, kInset);
        TerrainFrameAssembler::Frame f;
        f.key = key;
        f.mesh = TerrainTileMeshBuilder().build(tile, Ellipsoid::WGS84(), 5);
        return f;
    };
    frames.push_back(frame(TileKey(10, 100, 100), 0, 0));
    frames.push_back(frame(TileKey(10, 101, 100), 1, 0));
    frames.push_back(frame(TileKey(10, 100, 101), 0, 1));

    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.comparedNodePairs, 2 * (5 + 1));
    EXPECT_LT(audit.maxMeters, 1e-6);
}
