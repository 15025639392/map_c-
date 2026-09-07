#include "earth_engine/renderer/DisplacementTemplate.h"

#include <vector>

namespace earth_engine::render {

MeshUploadData buildTileDisplacementTemplate(const WebMercatorTileScheme& scheme,
                                             const TileKey& key, const Ellipsoid& ellipsoid,
                                             const Vec3& cameraEcef, int nodesPerEdge) {
    MeshUploadData mesh;
    if (nodesPerEdge < 2) {
        return mesh;
    }
    const int stride = nodesPerEdge + 1;
    const size_t vertexCount = static_cast<size_t>(stride) * stride;
    mesh.positions.reserve(vertexCount * 3);
    mesh.normals.reserve(vertexCount * 3);
    mesh.uvs.reserve(vertexCount * 2);
    mesh.indices.reserve(static_cast<size_t>(nodesPerEdge) * nodesPerEdge * 6);

    // 用零高度探针做"网格分数 → 经纬"（与 CPU 网格同一像素映射口径）。
    std::vector<double> zeros(vertexCount, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), stride, stride);
    for (int row = 0; row < stride; ++row) {
        for (int col = 0; col < stride; ++col) {
            const Cartographic c = probe.pixelToCartographic(col, row);
            const Vec3 normal = ellipsoid.geodeticSurfaceNormal(c).normalized();
            const Vec3 base = ellipsoid.cartographicToCartesian(
                Cartographic(c.longitude(), c.latitude(), 0.0));
            const Vec3 rel = base - cameraEcef; // RTC
            mesh.positions.push_back(static_cast<float>(rel.x()));
            mesh.positions.push_back(static_cast<float>(rel.y()));
            mesh.positions.push_back(static_cast<float>(rel.z()));
            mesh.normals.push_back(static_cast<float>(normal.x()));
            mesh.normals.push_back(static_cast<float>(normal.y()));
            mesh.normals.push_back(static_cast<float>(normal.z()));
            // uv：mercator 瓦内归一（北=0 行→v 小；与影像/高度纹理行序一致）。
            const double u = static_cast<double>(col) / nodesPerEdge;
            const double v = static_cast<double>(row) / nodesPerEdge;
            mesh.uvs.push_back(static_cast<float>(u));
            mesh.uvs.push_back(static_cast<float>(v));
        }
    }
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
    return mesh;
}

} // namespace earth_engine::render
