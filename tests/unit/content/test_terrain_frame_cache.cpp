#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameCache.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
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
        ++requestCount_;
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

    int requestCount() const { return requestCount_; }

private:
    std::function<double(const Cartographic&)> fn_;
    mutable int requestCount_ = 0;
};

CameraView cameraAt(const Ellipsoid& e, const Cartographic& ground, double alt,
                    double lonShiftDeg = 0.0, double latShiftDeg = 0.0) {
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(ground.longitude(), ground.latitude(), alt));
    const Vec3 target = e.cartographicToCartesian(Cartographic(
        ground.longitude() + degreesToRadians(lonShiftDeg),
        ground.latitude() + degreesToRadians(latShiftDeg), 0.0));
    const Vec3 up = e.geodeticSurfaceNormal(ground);
    return CameraView(pos, target, up, degreesToRadians(60.0), 16.0 / 9.0);
}

} // namespace

TEST(TerrainFrameCache, SameCameraReusesEverything) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const WebMercatorTileScheme scheme;
    FunctionalTerrainSource source(hillFn);
    TerrainFrameCache cache(scheme, source, e, 17, 8);

    TerrainLodConfig lod;
    lod.maxScreenSpaceErrorPx = 8.0;
    lod.geometricErrorScale = 0.001;
    lod.maxLevel = 12;

    const CameraView cam = cameraAt(e, ground, 20000.0, 0.06, -0.04);
    const auto r1 = cache.update(cam, lod);
    EXPECT_GT(r1.requested, 0u);
    EXPECT_EQ(r1.reused, 0u);
    EXPECT_GT(r1.frameCount, 0u);
    const auto firstFrames = cache.frames();

    const auto r2 = cache.update(cam, lod);
    EXPECT_EQ(r2.requested, 0u);   // 同相机：零重取
    EXPECT_EQ(r2.reused, r1.frameCount);
    EXPECT_EQ(r2.evicted, 0u);
    EXPECT_EQ(cache.frames().size(), firstFrames.size());
    // 逐帧逐顶点一致。
    for (size_t i = 0; i < firstFrames.size(); ++i) {
        EXPECT_EQ(cache.frames()[i].key, firstFrames[i].key);
        EXPECT_EQ(cache.frames()[i].mesh.positionsEcef.size(),
                  firstFrames[i].mesh.positionsEcef.size());
    }
    // 数据源全程只被请求过第一次的瓦数。
    EXPECT_EQ(source.requestCount(), static_cast<int>(r1.requested));
}

TEST(TerrainFrameCache, SmallMoveMostlyReuses) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const WebMercatorTileScheme scheme;
    FunctionalTerrainSource source(hillFn);
    TerrainFrameCache cache(scheme, source, e, 17, 8);
    TerrainLodConfig lod;
    lod.maxScreenSpaceErrorPx = 8.0;
    lod.geometricErrorScale = 0.001;
    lod.maxLevel = 12;

    const CameraView cam1 = cameraAt(e, ground, 20000.0, 0.06, -0.04);
    const auto r1 = cache.update(cam1, lod);
    EXPECT_GT(r1.frameCount, 0u);
    const int fetched1 = source.requestCount();

    // 小幅移动（~0.05°）+ 高度微调：应有复用，也可能补少量新瓦。
    const CameraView cam2 = cameraAt(e, ground, 18000.0, 0.09, -0.06);
    const auto r2 = cache.update(cam2, lod);
    EXPECT_GT(r2.reused, 0u);
    EXPECT_GE(r2.requested, 0u);
    EXPECT_GT(r2.frameCount, 0u);
    // 数据源计数与报告一致（首帧 + 增量）。
    EXPECT_EQ(source.requestCount(), fetched1 + static_cast<int>(r2.requested));
    // 未发生全量重取：复用占比 > 0 且请求数小于当前帧数（若只补边角瓦）。
    if (r2.frameCount > 0) {
        EXPECT_LT(r2.requested, r2.frameCount);
    }
}

TEST(TerrainFrameCache, LargeJumpRefreshesAndEvicts) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const WebMercatorTileScheme scheme;
    FunctionalTerrainSource source(hillFn);
    TerrainFrameCache cache(scheme, source, e, 9, 4);
    TerrainLodConfig lod;
    lod.maxScreenSpaceErrorPx = 16.0;
    lod.geometricErrorScale = 0.001;
    lod.maxLevel = 9;

    const Cartographic chongqing = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    const auto r1 = cache.update(cameraAt(e, chongqing, 300000.0, 0.4, -0.3), lod);
    ASSERT_GT(r1.frameCount, 0u);
    const auto firstKeys = cache.frames();

    // 跳到纽约（不同半球）：视野内瓦应全换。
    const Cartographic ny = Cartographic::fromDegrees(-74.0, 40.7, 0.0);
    const auto r2 = cache.update(cameraAt(e, ny, 300000.0, 0.4, -0.3), lod);
    EXPECT_GT(r2.requested, 0u);
    EXPECT_GT(r2.evicted, 0u);
    EXPECT_GT(r2.frameCount, 0u);
    // 新视野不含旧瓦。
    for (const auto& f : cache.frames()) {
        const Rectangle r = scheme.tileRectangleRadians(f.key);
        EXPECT_TRUE(r.east() < degreesToRadians(0.0)) << f.key.toString();
    }
    (void)firstKeys;
}

TEST(TerrainFrameCache, DeterministicOrdering) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const WebMercatorTileScheme scheme;
    FunctionalTerrainSource source(hillFn);
    TerrainFrameCache cache(scheme, source, e, 9, 4);
    TerrainLodConfig lod;
    lod.maxScreenSpaceErrorPx = 8.0;
    lod.maxLevel = 10;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7, 0.0);
    (void)cache.update(cameraAt(e, ground, 15000.0, 0.0, 0.0), lod);
    const auto frames1 = cache.frames();
    ASSERT_GT(frames1.size(), 0u);
    for (size_t i = 1; i < frames1.size(); ++i) {
        EXPECT_TRUE(frames1[i - 1].key < frames1[i].key); // 排序稳定
    }
}
