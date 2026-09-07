#include "earth_engine/content/TerrainTileMesh.h"

#include <algorithm>

#include "earth_engine/content/HeightmapSampler.h"

namespace earth_engine {

TerrainMeshData TerrainTileMeshBuilder::build(const HeightmapTile& tile,
                                              const Ellipsoid& ellipsoid,
                                              int nodesPerEdge) const {
    TerrainMeshData mesh;
    mesh.nodesPerEdge = nodesPerEdge;
    if (nodesPerEdge < 2) {
        return mesh;
    }

    const int width = tile.width();
    const int height = tile.height();
    const double fw = static_cast<double>(width - 1);
    const double fh = static_cast<double>(height - 1);
    const int stride = nodesPerEdge + 1;
    const int vertexCount = stride * stride;

    mesh.positionsEcef.reserve(static_cast<size_t>(vertexCount));
    mesh.normals.resize(static_cast<size_t>(vertexCount), Vec3::zero());

    // 节点采样带瓦哨兵表：节点窗若压到 no-data 像素（数据空洞/重叠环），只对
    // 有效角归一化，避免网格向 -10000 底值沉出假深沟（并入 gis-md 语义，B1）。
    const HeightmapSampler sampler(tile.heights(), width, height, tile.noDataValues(),
                                   tile.noDataCount());

    for (int row = 0; row < stride; ++row) {
        for (int col = 0; col < stride; ++col) {
            // 节点 (col,row) → 像素坐标（双线性采样坐标，行 0 = 北）。
            const double px = static_cast<double>(col) / static_cast<double>(nodesPerEdge) * fw;
            const double py = static_cast<double>(row) / static_cast<double>(nodesPerEdge) * fh;
            const Cartographic base = tile.pixelToCartographic(px, py);
            const double h = sampler.sampleBilinear(px, py);
            mesh.positionsEcef.push_back(
                ellipsoid.cartographicToCartesian(Cartographic(base.longitude(), base.latitude(), h)));
        }
    }

    // 三角形（外向绕序，见头文件推导：{a,c,b} 与 {b,c,d}）。
    const size_t quadCount = static_cast<size_t>(nodesPerEdge) * nodesPerEdge;
    mesh.indices.reserve(quadCount * 6);
    for (int row = 0; row < nodesPerEdge; ++row) {
        for (int col = 0; col < nodesPerEdge; ++col) {
            const uint32_t a = static_cast<uint32_t>(row * stride + col);
            const uint32_t b = a + 1;
            const uint32_t c = a + static_cast<uint32_t>(stride);
            const uint32_t d = c + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    // 平滑法线：邻接面法线等权平均。
    std::vector<Vec3> faceAccumulator(mesh.positionsEcef.size(), Vec3::zero());
    for (size_t t = 0; t < mesh.indices.size(); t += 3) {
        const Vec3& p0 = mesh.positionsEcef[mesh.indices[t]];
        const Vec3& p1 = mesh.positionsEcef[mesh.indices[t + 1]];
        const Vec3& p2 = mesh.positionsEcef[mesh.indices[t + 2]];
        Vec3 faceNormal = (p1 - p0).cross(p2 - p0);
        if (faceNormal.magnitudeSquared() <= 0.0) {
            continue; // 退化面（零面积）跳过
        }
        for (int k = 0; k < 3; ++k) {
            faceAccumulator[mesh.indices[t + static_cast<size_t>(k)]] += faceNormal;
        }
    }
    for (int i = 0; i < vertexCount; ++i) {
        mesh.normals[static_cast<size_t>(i)] = faceAccumulator[static_cast<size_t>(i)].normalized();
    }
    return mesh;
}

} // namespace earth_engine
