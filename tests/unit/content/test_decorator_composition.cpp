// 装饰器组合端到端（真实相机管线里的启用配方）：
//   相机 → LOD 选择 → [HeightDatumCorrecting ∘ AncestorFallback ∘ 源] → 帧
// 验证：
// - 祖先回退填充源缺层造成的空洞（帧键 ⊇ 裸帧键）；
// - EGM96 undulation（常数 13m 网格）叠加后同键帧 min/max 恰 +13；
// - 中心拾取海拔随改正整体抬升 ≈ undulation。
// 配方即"真实 DEM + 缺瓦回退 + EGM96"三件套在 host 的组合证据。
#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/TerrainCameraPipeline.h"
#include "earth_engine/content/AncestorFallbackDataSource.h"
#include "earth_engine/content/HeightDatumCorrectingDataSource.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainPicking.h"
#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/geodesy/HeightDatumCorrector.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

/// 程序化高度源；failAtMinZ：z ≥ 该值的请求全部失败（模拟源在该区缺层）。
class FunctionalTerrainSource : public ITerrainDataSource {
public:
    explicit FunctionalTerrainSource(int failAtMinZ = -1) : failAtMinZ_(failAtMinZ) {}

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override {
        if (failAtMinZ_ >= 0 && key.z() >= failAtMinZ_) {
            return std::nullopt;
        }
        TerrainGrid grid;
        grid.width = gridSize;
        grid.height = gridSize;
        grid.heights.resize(static_cast<size_t>(gridSize) * gridSize);
        std::vector<double> zeros(static_cast<size_t>(gridSize) * gridSize, 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), gridSize, gridSize);
        for (int row = 0; row < gridSize; ++row) {
            for (int col = 0; col < gridSize; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                grid.heights[static_cast<size_t>(row) * gridSize + col] = hillFn(c);
            }
        }
        return grid;
    }

private:
    int failAtMinZ_ = -1;
};

CameraView makeCamera(const Ellipsoid& ellipsoid) {
    const Cartographic center = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const Vec3 pos =
        ellipsoid.cartographicToCartesian(Cartographic(center.longitude(), center.latitude(),
                                                       9000.0));
    const Vec3 up = ellipsoid.geodeticSurfaceNormal(center);
    return CameraView(pos, ellipsoid.cartographicToCartesian(center), up, degreesToRadians(60.0),
                      4.0 / 3.0);
}

std::map<TileKey, const TerrainFrameAssembler::Frame*> byKey(
    const std::vector<TerrainFrameAssembler::Frame>& frames) {
    std::map<TileKey, const TerrainFrameAssembler::Frame*> m;
    for (const auto& f : frames) {
        m.emplace(f.key, &f);
    }
    return m;
}

} // namespace

TEST(DecoratorComposition, FallbackFillsCameraSelectionHoles) {
    const WebMercatorTileScheme scheme;
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();
    const CameraView camera = makeCamera(ellipsoid);

    TerrainCameraPipelineConfig config;
    config.lod.maxScreenSpaceErrorPx = 8.0;
    config.lod.maxLevel = 12;
    config.gridSize = 17;
    config.nodesPerEdge = 8;

    FunctionalTerrainSource inner(/*failAtMinZ=*/10); // z≥10 缺层
    const AncestorFallbackDataSource fallback(inner);

    const auto bare = assembleTerrainFrameForCamera(scheme, camera, ellipsoid, inner, config);
    const auto composed =
        assembleTerrainFrameForCamera(scheme, camera, ellipsoid, fallback, config);
    ASSERT_FALSE(composed.empty());
    EXPECT_GE(composed.size(), bare.size()); // 回退填掉缺层造成的空洞

    // 每个裸帧键都在组合帧里（不丢瓦，只增不删）。
    const auto comp = byKey(composed);
    for (const auto& f : bare) {
        EXPECT_TRUE(comp.count(f.key) == 1u) << "key " << f.key.toString();
    }
}

TEST(DecoratorComposition, Egm96UndulationShiftsComposedFramesByN) {
    const WebMercatorTileScheme scheme;
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();
    const CameraView camera = makeCamera(ellipsoid);

    TerrainCameraPipelineConfig config;
    config.lod.maxScreenSpaceErrorPx = 8.0;
    config.lod.maxLevel = 12;
    config.gridSize = 17;
    config.nodesPerEdge = 8;

    FunctionalTerrainSource inner(/*failAtMinZ=*/10);
    const AncestorFallbackDataSource fallback(inner);
    const double values[20] = {13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
                               13, 13, 13, 13, 13, 13, 13, 13, 13, 13};
    const GridHeightDatumCorrector grid(80.0, 10.0, 10.0, 5, 4, values);
    const HeightDatumCorrectingDataSource datum(fallback, grid);

    const auto plain =
        assembleTerrainFrameForCamera(scheme, camera, ellipsoid, fallback, config);
    const auto lifted = assembleTerrainFrameForCamera(scheme, camera, ellipsoid, datum, config);
    ASSERT_FALSE(plain.empty());
    ASSERT_EQ(lifted.size(), plain.size()); // 同选择、同回退 → 同键集

    const auto plainMap = byKey(plain);
    for (const auto& f : lifted) {
        const auto it = plainMap.find(f.key);
        ASSERT_TRUE(it != plainMap.end()) << f.key.toString();
        EXPECT_NEAR(f.minHeight - it->second->minHeight, 13.0, 1e-6);
        EXPECT_NEAR(f.maxHeight - it->second->maxHeight, 13.0, 1e-6);
    }

    // 中心拾取：地形整体抬升 undulation → 命中海拔 ≈ 抬高 13m。
    const auto hitLifted = pickTerrainFrame(camera.rayThroughNdc(0.0, 0.0).origin(),
                                            camera.rayThroughNdc(0.0, 0.0).direction(), lifted);
    const auto hitPlain = pickTerrainFrame(camera.rayThroughNdc(0.0, 0.0).origin(),
                                           camera.rayThroughNdc(0.0, 0.0).direction(), plain);
    ASSERT_TRUE(hitPlain.has_value());
    ASSERT_TRUE(hitLifted.has_value());
    const Cartographic cp = ellipsoid.cartesianToCartographic(hitPlain->point);
    const Cartographic cl = ellipsoid.cartesianToCartographic(hitLifted->point);
    EXPECT_NEAR(cl.height() - cp.height(), 13.0, 6.0);
}
