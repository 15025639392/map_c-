#include <gtest/gtest.h>

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/TerrainCameraPipeline.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainPicking.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

// 缙云山一带的固定机位（与 docs/northstar/terrain.md 验收机位同区）。
constexpr double kStationLonDeg = 106.44;
constexpr double kStationLatDeg = 29.70;

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

class FunctionalTerrainSource : public ITerrainDataSource {
public:
    explicit FunctionalTerrainSource(std::function<double(const Cartographic&)> fn)
        : fn_(std::move(fn)) {}

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override {
        TerrainGrid grid;
        grid.width = gridSize;
        grid.height = gridSize;
        grid.heights.resize(static_cast<size_t>(gridSize * gridSize));
        std::vector<double> zeros(static_cast<size_t>(gridSize * gridSize), 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), gridSize, gridSize);
        for (int row = 0; row < gridSize; ++row) {
            for (int col = 0; col < gridSize; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                grid.heights[static_cast<size_t>(row * gridSize + col)] = fn_(c);
            }
        }
        return grid;
    }

private:
    std::function<double(const Cartographic&)> fn_;
};

// 帧统计摘要（打进 stdout，便于把数字抄进 docs 基线表）。
std::string frameSummary(const std::vector<TerrainFrameAssembler::Frame>& frames) {
    int maxZ = -1;
    int minZ = 999;
    size_t triangles = 0;
    double minH = 1e30;
    double maxH = -1e30;
    for (const auto& f : frames) {
        maxZ = std::max(maxZ, f.key.z());
        minZ = std::min(minZ, f.key.z());
        triangles += f.mesh.indices.size() / 3;
        minH = std::min(minH, f.minHeight);
        maxH = std::max(maxH, f.maxHeight);
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), "frames=%zu z=[%d..%d] triangles=%zu height=[%.1f..%.1f]",
                  frames.size(), minZ, maxZ, triangles, minH, maxH);
    return buf;
}

} // namespace

// M-mid 型固定机位（camH 15km，斜视），host 基线：帧 + 统计 + 拾取。
// 目标：任何后续改动（选择/网格/解码）不破坏"该机位可出帧、可拾取、统计量级稳定"。
TEST(FixedStationBaseline, MidAltitudeOblique) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(kStationLonDeg, kStationLatDeg, 0.0);
    const Vec3 up = e.geodeticSurfaceNormal(center);
    // camH 15000 m 正上方，目标偏移制造斜视（~等效 pitch 下沉视角）。
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), 15000.0));
    const Vec3 target = e.cartographicToCartesian(
        Cartographic::fromDegrees(kStationLonDeg + 0.08, kStationLatDeg - 0.05, 0.0));
    const CameraView camera(pos, target, up, degreesToRadians(60.0), 16.0 / 9.0);

    const WebMercatorTileScheme scheme;
    const FunctionalTerrainSource source(hillFn);
    TerrainCameraPipelineConfig config;
    config.lod.maxScreenSpaceErrorPx = 8.0;
    config.lod.geometricErrorScale = 0.001;
    config.lod.maxLevel = 13;
    config.gridSize = 17;
    config.nodesPerEdge = 8;
    const auto frames = assembleTerrainFrameForCamera(scheme, camera, e, source, config);

    ASSERT_FALSE(frames.empty());
    const std::string summary = frameSummary(frames);
    std::printf("[baseline M-mid] %s\n", summary.c_str());

    // 统计量级（本机确定性数字，宽松区间防过度耦合）：
    // 15km + 8px + scale=1e-3 → 多数瓦 z≈8~10，帧数几十内。
    int maxZ = 0;
    size_t tri = 0;
    for (const auto& f : frames) {
        maxZ = std::max(maxZ, f.key.z());
        tri += f.mesh.indices.size() / 3;
    }
    EXPECT_GE(maxZ, 7);
    EXPECT_LE(maxZ, 11);
    EXPECT_GE(frames.size(), 1u);
    EXPECT_LT(frames.size(), 200u);
    EXPECT_GT(tri, 100u);

    // 正下方点被某帧覆盖。
    bool covered = false;
    for (const auto& f : frames) {
        const Rectangle r = scheme.tileRectangleRadians(f.key);
        if (center.longitude() >= r.west() && center.longitude() <= r.east() &&
            center.latitude() >= r.south() && center.latitude() <= r.north()) {
            covered = true;
            break;
        }
    }
    EXPECT_TRUE(covered);

    // 屏幕中心拾取到地形，海拔在山丘值域。
    const Ray centerRay = camera.rayThroughNdc(0.0, 0.0);
    const auto hit = pickTerrainFrame(centerRay.origin(), centerRay.direction(), frames);
    ASSERT_TRUE(hit.has_value());
    const Cartographic hitCarto = e.cartesianToCartographic(hit->point);
    EXPECT_GT(hitCarto.height(), 199.0);
    EXPECT_LT(hitCarto.height(), 801.0);

    // 视野内帧高度无 NaN/Inf。
    for (const auto& f : frames) {
        for (const Vec3& p : f.mesh.positionsEcef) {
            EXPECT_TRUE(std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()));
        }
    }
}
