#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/TerrainCameraPipeline.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainPicking.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

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

} // namespace

TEST(TerrainCameraPipeline, CameraProducesTerrainFrames) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    // 相机：正下方俯瞰 12 km，fov 60°，4:3。
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), 12000.0));
    const Vec3 target = e.cartographicToCartesian(center);
    const Vec3 up = e.geodeticSurfaceNormal(center);
    const CameraView camera(pos, target, up, degreesToRadians(60.0), 4.0 / 3.0);

    const WebMercatorTileScheme scheme;
    const FunctionalTerrainSource source(hillFn);
    TerrainCameraPipelineConfig config;
    config.lod.maxScreenSpaceErrorPx = 8.0;
    config.lod.geometricErrorScale = 0.001;
    config.lod.maxLevel = 13;
    const auto frames = assembleTerrainFrameForCamera(scheme, camera, e, source, config);
    ASSERT_FALSE(frames.empty());

    // 正下方点必须被某个帧覆盖。
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
    // 帧高度范围与函数一致。
    for (const auto& f : frames) {
        EXPECT_GT(f.maxHeight, 200.0);
        EXPECT_LT(f.minHeight, 800.0);
    }

    // 屏幕中心射线应拾取到地形（而不是只看太空/椭球）。
    const Ray centerRay = camera.rayThroughNdc(0.0, 0.0);
    const auto hit = pickTerrainFrame(centerRay.origin(), centerRay.direction(), frames);
    ASSERT_TRUE(hit.has_value());
    // 命中点海拔在函数值域内。
    const Cartographic hitCarto = e.cartesianToCartographic(hit->point);
    EXPECT_GT(hitCarto.height(), 199.0);
    EXPECT_LT(hitCarto.height(), 801.0);
    // 命中点靠近相机正下方（地形起伏下不会偏出太多）。
    EXPECT_NEAR(hitCarto.longitude(), center.longitude(), degreesToRadians(0.1));
    EXPECT_NEAR(hitCarto.latitude(), center.latitude(), degreesToRadians(0.1));
}

TEST(TerrainCameraPipeline, LookingAtSpaceYieldsNoFrames) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Vec3 pos = e.cartographicToCartesian(Cartographic::fromDegrees(0.0, 0.0, 500000.0));
    const CameraView camera(pos, pos + Vec3(0.0, 0.0, 1.0), Vec3(0.0, 0.0, 1.0),
                            degreesToRadians(60.0), 1.0);
    const WebMercatorTileScheme scheme;
    const FunctionalTerrainSource source(hillFn);
    const auto frames = assembleTerrainFrameForCamera(scheme, camera, e, source,
                                                      TerrainCameraPipelineConfig());
    EXPECT_TRUE(frames.empty());
}
