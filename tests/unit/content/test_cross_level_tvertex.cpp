// 跨级 T-顶点裂缝度量（B4 数值取证）：粗瓦(z) 与东/南邻 z+1 子瓦共享边界。
//
// 语义：子瓦边界节点以自身密度采样真实表面；粗瓦边界 = 过其顶点（真实表面点）
// 的**直线弦折线**。落在粗弦段之间的子瓦 T-顶点与粗弦的 ECEF 距离 = 换代裂缝，
// 含两项：椭球曲率弦垂（平坦地形也有）与地形曲率弦垂。修复方向 = 子瓦边界
// 顶点吸附到粗弦（B4 remap/边 LUT），吸附量 = 本度量。
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

constexpr int kGrid = 17; // 高度图 17×17（顶点栅格 fixture）

// 强地形曲率场：沿纬度二次（经度无关）→ 粗弦段内 sagitta 显著。
// φc 取粗瓦东边中纬；系数使全边 (Δφ≈7.7e-4 rad) 弦垂 ≈ 30m。
double quadLatFn(const Cartographic& c, double latCenterRad) {
    const double d = c.latitude() - latCenterRad;
    return 500.0 + 2.0e8 * d * d;
}

// 线性纬度场（无地形曲率）→ 裂缝只剩椭球曲率项。
double linearLatFn(const Cartographic& c, double latCenterRad) {
    return 500.0 + 3.0e4 * (c.latitude() - latCenterRad);
}

// 顶点栅格瓦：像素 (col,row) 高度 = fn(该像素地理坐标)。
TerrainFrameAssembler::Frame buildFrame(const WebMercatorTileScheme& scheme, const TileKey& key,
                                        int nodesPerEdge,
                                        double (*fn)(const Cartographic&, double),
                                        double latCenterRad) {
    std::vector<double> h(static_cast<size_t>(kGrid) * kGrid);
    std::vector<double> zeros(static_cast<size_t>(kGrid) * kGrid, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), kGrid, kGrid);
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            h[static_cast<size_t>(row) * kGrid + col] =
                fn(probe.pixelToCartographic(col, row), latCenterRad);
        }
    }
    const HeightmapTile tile(scheme, key, h.data(), kGrid, kGrid);
    TerrainFrameAssembler::Frame frame;
    frame.key = key;
    frame.mesh = TerrainTileMeshBuilder().build(tile, Ellipsoid::WGS84(), nodesPerEdge);
    return frame;
}

// 标准场景：粗瓦 z12 中心瓦 + 东邻 (z12,x+1) 的两个 z13 子瓦（共享粗瓦东边）。
// 返回 {粗瓦 + 2 子瓦}；latCenter = 粗瓦东边中纬（近似 29.7°）。
struct Scenario {
    std::vector<TerrainFrameAssembler::Frame> frames;
    double latCenterRad = 0.0;
};
Scenario makeEastScenario(const WebMercatorTileScheme& scheme, int coarseNodes,
                          int childNodes,
                          double (*fn)(const Cartographic&, double)) {
    const TileKey coarse(12, 3259, 1693); // 106.44E 29.70N 一带
    // 粗瓦东边中纬：用其自身像素映射估计。
    std::vector<double> zeros(static_cast<size_t>(kGrid) * kGrid, 0.0);
    const HeightmapTile probe(scheme, coarse, zeros.data(), kGrid, kGrid);
    const Cartographic mid = probe.pixelToCartographic(kGrid - 1, (kGrid - 1) / 2);
    Scenario sc;
    sc.latCenterRad = mid.latitude();
    sc.frames.push_back(buildFrame(scheme, coarse, coarseNodes, fn, sc.latCenterRad));
    sc.frames.push_back(buildFrame(scheme, TileKey(13, 6520, 3386), childNodes, fn,
                                   sc.latCenterRad));
    sc.frames.push_back(buildFrame(scheme, TileKey(13, 6520, 3387), childNodes, fn,
                                   sc.latCenterRad));
    return sc;
}

// 东/南双轴场景：粗瓦 z12 + 东邻两子瓦 + 南邻两子瓦（粗瓦东边与南边都出现 T-顶点）。
Scenario makeCornerScenario(const WebMercatorTileScheme& scheme, int coarseNodes,
                            int childNodes, double (*fn)(const Cartographic&, double)) {
    Scenario sc = makeEastScenario(scheme, coarseNodes, childNodes, fn);
    // 南邻 (z12, x, y+1) 的两个 z13 子瓦（共享粗瓦南边；2(y+1)=3388）。
    sc.frames.push_back(buildFrame(scheme, TileKey(13, 6518, 3388), childNodes, fn,
                                   sc.latCenterRad));
    sc.frames.push_back(buildFrame(scheme, TileKey(13, 6519, 3388), childNodes, fn,
                                   sc.latCenterRad));
    return sc;
}

} // namespace

// 端点共享点应精确重合（同 fn 同几何）：裂缝集中在段内 T-顶点。
// （通过 corner 节点 ~0 间接验证：corner 节点距离应 ≪ 段内最大值。）
TEST(CrossLevelTVertex, QuadraticFieldCrackIsLargeAndDecreasesWithCoarseSegments) {
    const WebMercatorTileScheme scheme;
    // 粗瓦 2 段/边：段长 = 半条边 → 弦垂 ≈ 7m 级（地形曲率）。
    Scenario coarse2 = makeEastScenario(scheme, 2, 2, &quadLatFn);
    const SeamAuditResult gap2 = auditCrossLevelTVertexGap(coarse2.frames);
    EXPECT_EQ(gap2.edgePairsFound, 2); // 两个子瓦（东邻细分）
    EXPECT_EQ(gap2.comparedEdges, 2);
    EXPECT_EQ(gap2.comparedNodePairs, 2 * (2 + 1));
    EXPECT_GT(gap2.maxMeters, 5.0); // 强曲率 → 明显裂缝
    EXPECT_GT(gap2.meanMeters, 0.0);

    // 粗瓦 8 段/边 + 子瓦 3 段（1/6 步长与粗瓦 1/8 步长错位 → T-顶点落在粗弦段内）。
    Scenario coarse8 = makeEastScenario(scheme, 8, 3, &quadLatFn);
    const SeamAuditResult gap8 = auditCrossLevelTVertexGap(coarse8.frames);
    EXPECT_GT(gap8.maxMeters, 0.0);
    EXPECT_LT(gap8.maxMeters, gap2.maxMeters * 0.3);
    EXPECT_LT(gap8.maxMeters, 2.0); // 段长 1/4 → 弦垂 ≈ (1/4)²·7m ≈ 0.4-1.1m 量级
}

TEST(CrossLevelTVertex, FlatFieldCrackIsEarthCurvatureSagitta) {
    const WebMercatorTileScheme scheme;
    Scenario sc = makeEastScenario(scheme, 2, 2, &linearLatFn);
    const SeamAuditResult gap = auditCrossLevelTVertexGap(sc.frames);
    // 线性场：无地形曲率 → 裂缝 ≈ 椭球曲率弦垂（z12 段 ~2.4km → ~0.1m 量级）。
    EXPECT_GT(gap.maxMeters, 0.01);
    EXPECT_LT(gap.maxMeters, 2.0);
    // 端点/共享粗顶点仍精确（≈0）→ comparedNodePairs 中含 corner 对，mean 远小于 max。
    EXPECT_LT(gap.meanMeters, gap.maxMeters);
}

TEST(CrossLevelTVertex, TerrainCurvatureDominatesOverEarthCurvature) {
    const WebMercatorTileScheme scheme;
    Scenario quad = makeEastScenario(scheme, 2, 2, &quadLatFn);
    Scenario lin = makeEastScenario(scheme, 2, 2, &linearLatFn);
    const SeamAuditResult gapQuad = auditCrossLevelTVertexGap(quad.frames);
    const SeamAuditResult gapLin = auditCrossLevelTVertexGap(lin.frames);
    EXPECT_GT(gapQuad.maxMeters, gapLin.maxMeters); // 强曲率 >> 椭球项
}

// ---- B4 吸附数值原型：snapChildBoundariesToCoarse ---------------------------

TEST(CrossLevelTVertex, SnapClosesEastEdgeGapAndKeepsTopology) {
    const WebMercatorTileScheme scheme;
    Scenario sc = makeEastScenario(scheme, 2, 2, &quadLatFn);
    const SeamAuditResult before = auditCrossLevelTVertexGap(sc.frames);
    ASSERT_GT(before.maxMeters, 5.0);

    // 拓扑计数（吸附不改索引/三角形，只改边界顶点位置）。
    size_t verts = 0, tris = 0;
    for (const auto& f : sc.frames) {
        verts += f.mesh.positionsEcef.size();
        tris += f.mesh.indices.size() / 3;
    }

    const int snapped = snapChildBoundariesToCoarse(sc.frames);
    EXPECT_GT(snapped, 0); // 段内 T-顶点被吸附（corner 本就精确，不算入）

    const SeamAuditResult after = auditCrossLevelTVertexGap(sc.frames);
    EXPECT_EQ(after.comparedNodePairs, before.comparedNodePairs);
    EXPECT_LT(after.maxMeters, 1e-6); // 子瓦西列已贴粗瓦东边弦 → 裂缝归零

    size_t vertsAfter = 0, trisAfter = 0;
    for (const auto& f : sc.frames) {
        vertsAfter += f.mesh.positionsEcef.size();
        trisAfter += f.mesh.indices.size() / 3;
    }
    EXPECT_EQ(vertsAfter, verts); // 水密性：拓扑不变
    EXPECT_EQ(trisAfter, tris);
}

TEST(CrossLevelTVertex, SnapCoversBothAxesEastAndSouth) {
    const WebMercatorTileScheme scheme;
    Scenario sc = makeCornerScenario(scheme, 2, 2, &quadLatFn);
    const SeamAuditResult before = auditCrossLevelTVertexGap(sc.frames);
    EXPECT_EQ(before.edgePairsFound, 4); // 东 2 + 南 2
    ASSERT_GT(before.maxMeters, 5.0);

    const int snapped = snapChildBoundariesToCoarse(sc.frames);
    EXPECT_GT(snapped, 0);
    const SeamAuditResult after = auditCrossLevelTVertexGap(sc.frames);
    EXPECT_EQ(after.edgePairsFound, 4);
    EXPECT_LT(after.maxMeters, 1e-6); // 东/南双轴都闭合
}
