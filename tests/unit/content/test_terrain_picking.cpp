#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/content/TerrainPicking.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/tiling/TerrainLodSelector.h"

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

TEST(TerrainPicking, HitsNorthWestCornerOfFirstFrame) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 10);
    ASSERT_TRUE(key.has_value());

    // 4 邻瓦（同层 2×2），确保拾取能跨帧工作。
    TerrainLodResult selection;
    const TileKey k00 = key.value();
    const TileKey k10(k00.z(), k00.x() + 1, k00.y());
    const TileKey k01(k00.z(), k00.x(), k00.y() + 1);
    const TileKey k11(k00.z(), k00.x() + 1, k00.y() + 1);
    ASSERT_TRUE(k10.isValid() && k01.isValid() && k11.isValid());
    selection.tiles = {k00, k10, k01, k11};

    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const Ellipsoid& e = Ellipsoid::WGS84();
    constexpr int kGrid = 17;
    const auto frames = assembler.assemble(scheme, selection, source, e, kGrid, 16);
    ASSERT_EQ(frames.size(), 4u);

    // k00 的西北角（= mesh 顶点 0）。
    const Rectangle cov = scheme.tileRectangleRadians(k00);
    const Cartographic nwCorner(cov.west(), cov.north(), 0.0);
    const double h = hillFn(nwCorner);
    const Vec3 cornerEcef = e.cartographicToCartesian(
        Cartographic(cov.west(), cov.north(), h));
    EXPECT_NEAR(frames[0].mesh.positionsEcef[0].x(), cornerEcef.x(), 1.0e-6);
    EXPECT_NEAR(frames[0].mesh.positionsEcef[0].y(), cornerEcef.y(), 1.0e-6);
    EXPECT_NEAR(frames[0].mesh.positionsEcef[0].z(), cornerEcef.z(), 1.0e-6);

    // 从角上方 2000 m 沿大地法线向下打射线。
    const Cartographic nwAbove(cov.west(), cov.north(), h + 2000.0);
    const Vec3 origin = e.cartographicToCartesian(nwAbove);
    const Vec3 dir = (cornerEcef - origin).normalized();
    const auto hit = pickTerrainFrame(origin, dir, frames);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->t, origin.distanceTo(cornerEcef), 5.0);
    EXPECT_NEAR(hit->point.x(), cornerEcef.x(), 1.0e-3);
    EXPECT_NEAR(hit->point.y(), cornerEcef.y(), 1.0e-3);
    EXPECT_NEAR(hit->point.z(), cornerEcef.z(), 1.0e-3);
    EXPECT_EQ(hit->key, k00);
}

TEST(TerrainPicking, MissWhenAimingUp) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 10);
    ASSERT_TRUE(key.has_value());
    TerrainLodResult selection;
    selection.tiles = {key.value()};
    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const Ellipsoid& e = Ellipsoid::WGS84();
    const auto frames = assembler.assemble(scheme, selection, source, e, 9, 4);
    ASSERT_EQ(frames.size(), 1u);

    // 从地表上方沿"向外"方向打（不打到任何面）。
    const Vec3 upDir = frames[0].mesh.normals[0];
    const Vec3 origin = frames[0].mesh.positionsEcef[0] + upDir * 100.0;
    EXPECT_FALSE(pickTerrainFrame(origin, upDir, frames).has_value());
}

TEST(TerrainPicking, PicksClosestFrameNotFarthest) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 10);
    ASSERT_TRUE(key.has_value());
    const TileKey k00 = key.value();
    const TileKey k10(k00.z(), k00.x() + 1, k00.y());
    TerrainLodResult selection;
    selection.tiles = {k00, k10};
    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const Ellipsoid& e = Ellipsoid::WGS84();
    const auto frames = assembler.assemble(scheme, selection, source, e, 17, 16);
    ASSERT_EQ(frames.size(), 2u);

    // k00 的中心点上空朝下打：应命中 k00（近侧）而非穿过 k00 网格打到别处。
    const Cartographic center = scheme.unprojectMeters(scheme.tileCenterMeters(k00));
    const double h = hillFn(center);
    const Vec3 target = e.cartographicToCartesian(Cartographic(center.longitude(), center.latitude(), h));
    const Vec3 origin = target + (target - Vec3::zero()).normalized() * 3000.0;
    const Vec3 dir = (target - origin).normalized();
    const auto hit = pickTerrainFrame(origin, dir, frames);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->key, k00);
    EXPECT_NEAR(hit->point.distanceTo(target), 0.0, 30.0);
}
