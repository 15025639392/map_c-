// DisplacementTemplate：每瓦椭球面模板网格（GPU 位移路径几何核，host 可测）。
#include <gtest/gtest.h>

#include <cmath>

#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/Vec3.h"
#include "earth_engine/renderer/DisplacementTemplate.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;
using namespace earth_engine::render;

namespace {
constexpr int kN = 8;

// 瓦中心上方 15km 相机（贴合实际位移渲染用法，float RTC 精度最优）。
Vec3 cameraAboveTileCenter(const WebMercatorTileScheme& scheme, const TileKey& key,
                           const Ellipsoid& e) {
    constexpr int kProbe = 9;
    std::vector<double> zeros(static_cast<size_t>(kProbe) * kProbe, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), kProbe, kProbe);
    const Cartographic c = probe.pixelToCartographic((kProbe - 1) / 2, (kProbe - 1) / 2);
    return e.cartographicToCartesian(Cartographic(c.longitude(), c.latitude(), 15000.0));
}
} // namespace

TEST(DisplacementTemplate, TopologyAndCounts) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 cam = cameraAboveTileCenter(scheme, key, e);
    const MeshUploadData m =
        buildTileDisplacementTemplate(scheme, key, e, cam, kN);
    ASSERT_TRUE(m.valid());
    const int stride = kN + 1;
    EXPECT_EQ(m.positions.size(), static_cast<size_t>(stride * stride * 3));
    EXPECT_EQ(m.normals.size(), m.positions.size());
    EXPECT_EQ(m.uvs.size(), static_cast<size_t>(stride * stride * 2));
    EXPECT_EQ(m.indices.size(), static_cast<size_t>(kN * kN * 6));
    EXPECT_TRUE(m.heights.empty()); // 高度不进顶点（走高度纹理）
}

TEST(DisplacementTemplate, VerticesLieOnEllipsoidSurface) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 cam = cameraAboveTileCenter(scheme, key, e);
    const MeshUploadData m = buildTileDisplacementTemplate(scheme, key, e, cam, kN);
    for (size_t i = 0; i < m.positions.size() / 3; ++i) {
        const Vec3 rel(m.positions[i * 3], m.positions[i * 3 + 1], m.positions[i * 3 + 2]);
        const Vec3 absP = rel + cam;
        const Cartographic c = e.cartesianToCartographic(absP);
        // float RTC 往返：量级 cm，容差 0.05m。
        EXPECT_LT(std::abs(c.height()), 0.05) << "模板顶点应在椭球面（高 0）";
    }
}

TEST(DisplacementTemplate, NormalsAreOutwardAndUvCorners) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 cam = cameraAboveTileCenter(scheme, key, e);
    const MeshUploadData m = buildTileDisplacementTemplate(scheme, key, e, cam, kN);
    const int stride = kN + 1;
    // 抽查若干顶点：法线与该顶点大地法线一致（近径向，逐瓦独立核对）。
    for (size_t i = 0; i < m.positions.size() / 3; i += stride) {
        const Vec3 rel(m.positions[i * 3], m.positions[i * 3 + 1], m.positions[i * 3 + 2]);
        const Vec3 expected = e.geodeticSurfaceNormal(rel + cam).normalized();
        const Vec3 n(m.normals[i * 3], m.normals[i * 3 + 1], m.normals[i * 3 + 2]);
        EXPECT_GT(n.dot(expected), 0.999);
    }
    // uv 角点：col0..n / row0..n → u∈{0,1} v∈{0,1}。
    EXPECT_FLOAT_EQ(m.uvs[0], 0.0f); // (0,0)
    EXPECT_FLOAT_EQ(m.uvs[1], 0.0f);
    EXPECT_FLOAT_EQ(m.uvs[(stride - 1) * 2], 1.0f); // col n row 0 → u=1
    EXPECT_FLOAT_EQ(m.uvs[(stride - 1) * 2 + 1], 0.0f);
}

TEST(DisplacementTemplate, InvalidNodesRejected) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 cam = e.cartographicToCartesian(Cartographic::fromDegrees(106.5, 29.7, 1000.0));
    EXPECT_FALSE(buildTileDisplacementTemplate(scheme, key, e, cam, 1).valid());
}
