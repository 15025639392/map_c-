#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <vector>

#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/TerrainTileMesh.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {


/// 按"每个像素格点处的地理位置"求值生成高度网格（与 tile 的像素↔地理映射一致）。
/// 同一全局函数 f 喂给两块相邻瓦 → 共享边的格点必然同值。
std::vector<double> makeGridFromGeoFn(const WebMercatorTileScheme& scheme, const TileKey& key,
                                      int w, int h,
                                      const std::function<double(const Cartographic&)>& f) {
    std::vector<double> zeros(static_cast<size_t>(w * h), 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), w, h);
    std::vector<double> out(static_cast<size_t>(w * h));
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const Cartographic c = probe.pixelToCartographic(col, row);
            out[static_cast<size_t>(row * w + col)] = f(c);
        }
    }
    return out;
}

/// 平坦高度函数。
double flatFn(const Cartographic&) { return 1000.0; }

/// 光滑地理高度函数（山丘）。
double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

} // namespace

TEST(TerrainTileMesh, TopologyAndCounts) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    auto grid = makeGridFromGeoFn(scheme, key.value(), 17, 17, flatFn);
    const HeightmapTile tile(scheme, key.value(), grid.data(), 17, 17);
    const TerrainTileMeshBuilder builder;
    const int nodesPerEdge = 8;
    const TerrainMeshData mesh = builder.build(tile, Ellipsoid::WGS84(), nodesPerEdge);

    const int stride = nodesPerEdge + 1;
    EXPECT_EQ(mesh.nodesPerEdge, nodesPerEdge);
    EXPECT_EQ(mesh.positionsEcef.size(), static_cast<size_t>(stride * stride));
    EXPECT_EQ(mesh.normals.size(), mesh.positionsEcef.size());
    EXPECT_EQ(mesh.indices.size(), static_cast<size_t>(nodesPerEdge * nodesPerEdge) * 6);
    // 顶点贴瓦边：角点在地理上 = 瓦的四角（与行序一致）。
    const Vec3& nw = mesh.positionsEcef[0];
    const Cartographic nwCarto = Ellipsoid::WGS84().cartesianToCartographic(nw);
    const Rectangle cov = scheme.tileRectangleRadians(key.value());
    EXPECT_NEAR(nwCarto.longitude(), cov.west(), 1.0e-9);
    EXPECT_NEAR(nwCarto.latitude(), cov.north(), 1.0e-9);
}

TEST(TerrainTileMesh, FlatTileHeightsAndNormals) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    auto grid = makeGridFromGeoFn(scheme, key.value(), 17, 17, flatFn);
    const HeightmapTile tile(scheme, key.value(), grid.data(), 17, 17);
    const TerrainTileMeshBuilder builder;
    const TerrainMeshData mesh = builder.build(tile, Ellipsoid::WGS84(), 8);

    // 每个顶点回读高度 ≈ 1000 m（顶点高度 = 采样格点值，回读经椭球转换数值误差）。
    for (const Vec3& p : mesh.positionsEcef) {
        const Cartographic c = Ellipsoid::WGS84().cartesianToCartographic(p);
        EXPECT_NEAR(c.height(), 1000.0, 1.0e-6);
    }
    // 面法线外向：与瓦中心大地法线点积 > 0。
    const Vec3 centerUp =
        Ellipsoid::WGS84().geodeticSurfaceNormal(
            scheme.unprojectMeters(scheme.tileCenterMeters(key.value())));
    for (size_t t = 0; t < mesh.indices.size(); t += 3) {
        const Vec3& p0 = mesh.positionsEcef[mesh.indices[t]];
        const Vec3& p1 = mesh.positionsEcef[mesh.indices[t + 1]];
        const Vec3& p2 = mesh.positionsEcef[mesh.indices[t + 2]];
        const Vec3 n = (p1 - p0).cross(p2 - p0);
        EXPECT_GT(n.dot(centerUp), 0.0) << "face " << t / 3;
    }
    // 平坦面平滑法线 ≈ 各顶点大地法线（1e-3 余弦容差，瓦内曲率影响小）。
    for (size_t i = 0; i < mesh.normals.size(); ++i) {
        const Cartographic c = Ellipsoid::WGS84().cartesianToCartographic(mesh.positionsEcef[i]);
        const Vec3 geoNormal = Ellipsoid::WGS84().geodeticSurfaceNormal(c);
        EXPECT_GT(mesh.normals[i].dot(geoNormal), 0.999);
    }
}

TEST(TerrainTileMesh, HillyHeightsRoundTrip) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    auto grid = makeGridFromGeoFn(scheme, key.value(), 33, 33, hillFn);
    const HeightmapTile tile(scheme, key.value(), grid.data(), 33, 33);
    const TerrainTileMeshBuilder builder;
    // 几何网格（17 节点）比内容（33²）粗——T-E1 解耦原则的 host 侧体现。
    const TerrainMeshData mesh = builder.build(tile, Ellipsoid::WGS84(), 16);

    // 顶点回读高度 ≈ 该节点处采样值（网格点在 33² 内容上双线性插值）。
    const Ellipsoid& e = Ellipsoid::WGS84();
    for (int row = 0; row <= 16; ++row) {
        for (int col = 0; col <= 16; ++col) {
            const size_t idx = static_cast<size_t>(row * 17 + col);
            const Cartographic c = e.cartesianToCartographic(mesh.positionsEcef[idx]);
            // 独立重算：同节点像素坐标 → 内容采样。
            const double px = static_cast<double>(col) / 16.0 * 32.0;
            const double py = static_cast<double>(row) / 16.0 * 32.0;
            const Cartographic base = tile.pixelToCartographic(px, py);
            const std::optional<double> expectedH = tile.sampleHeightAt(base);
            ASSERT_TRUE(expectedH.has_value());
            EXPECT_NEAR(c.height(), expectedH.value(), 1.0e-6);
        }
    }
    // 山丘高度范围落回 [200, 800]。
    for (const Vec3& p : mesh.positionsEcef) {
        const Cartographic c = e.cartesianToCartographic(p);
        EXPECT_GT(c.height(), 199.0);
        EXPECT_LT(c.height(), 801.0);
    }
}

TEST(TerrainTileMesh, SameLevelNeighborsShareEdgeVertices) {
    // 无缝契约的机制地基（T-V5）：同级相邻瓦共享边顶点 ECEF 必须逐点重合。
    const WebMercatorTileScheme scheme;
    const auto keyA = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 8);
    ASSERT_TRUE(keyA.has_value());
    // 显式构造东西/南北邻居（z8 下 Chongqing 不贴世界边，邻居必合法）。
    const TileKey keyEast(keyA->z(), keyA->x() + 1, keyA->y());
    const TileKey keySouth(keyA->z(), keyA->x(), keyA->y() + 1);
    ASSERT_TRUE(keyEast.isValid());
    ASSERT_TRUE(keySouth.isValid());

    constexpr int kGrid = 17; // 内容 17×17；网格每边 16 段 → 节点贴所有格点
    auto gridA = makeGridFromGeoFn(scheme, keyA.value(), kGrid, kGrid, hillFn);
    auto gridEast = makeGridFromGeoFn(scheme, keyEast, kGrid, kGrid, hillFn);
    auto gridSouth = makeGridFromGeoFn(scheme, keySouth, kGrid, kGrid, hillFn);
    const HeightmapTile tileA(scheme, keyA.value(), gridA.data(), kGrid, kGrid);
    const HeightmapTile tileEast(scheme, keyEast, gridEast.data(), kGrid, kGrid);
    const HeightmapTile tileSouth(scheme, keySouth, gridSouth.data(), kGrid, kGrid);

    const TerrainTileMeshBuilder builder;
    const Ellipsoid& e = Ellipsoid::WGS84();
    const TerrainMeshData meshA = builder.build(tileA, e, 16);
    const TerrainMeshData meshEast = builder.build(tileEast, e, 16);
    const TerrainMeshData meshSouth = builder.build(tileSouth, e, 16);

    // A 东边节点 col=16 ↔ 东瓦西边节点 col=0，逐 row 对比。
    for (int row = 0; row <= 16; ++row) {
        const size_t idxA = static_cast<size_t>(row * 17 + 16);
        const size_t idxN = static_cast<size_t>(row * 17 + 0);
        EXPECT_NEAR(meshA.positionsEcef[idxA].x(), meshEast.positionsEcef[idxN].x(), 1.0e-6);
        EXPECT_NEAR(meshA.positionsEcef[idxA].y(), meshEast.positionsEcef[idxN].y(), 1.0e-6);
        EXPECT_NEAR(meshA.positionsEcef[idxA].z(), meshEast.positionsEcef[idxN].z(), 1.0e-6);
    }
    // A 南边节点 row=16 ↔ 南瓦北边节点 row=0，逐 col 对比。
    for (int col = 0; col <= 16; ++col) {
        const size_t idxA = static_cast<size_t>(16 * 17 + col);
        const size_t idxN = static_cast<size_t>(0 * 17 + col);
        EXPECT_NEAR(meshA.positionsEcef[idxA].x(), meshSouth.positionsEcef[idxN].x(), 1.0e-6);
        EXPECT_NEAR(meshA.positionsEcef[idxA].y(), meshSouth.positionsEcef[idxN].y(), 1.0e-6);
        EXPECT_NEAR(meshA.positionsEcef[idxA].z(), meshSouth.positionsEcef[idxN].z(), 1.0e-6);
    }
}

TEST(TerrainTileMesh, DegenerateRejected) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    const double one[1] = {0.0};
    const HeightmapTile tile(scheme, key.value(), one, 1, 1);
    const TerrainTileMeshBuilder builder;
    const TerrainMeshData mesh = builder.build(tile, Ellipsoid::WGS84(), 1); // nodesPerEdge < 2
    EXPECT_EQ(mesh.positionsEcef.size(), 0u);
    EXPECT_EQ(mesh.indices.size(), 0u);
}
