#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <map>

#include "earth_engine/core/geodesy/QuadtreeGeometricError.h"
#include "earth_engine/tiling/TerrainLodSelector.h"

using namespace earth_engine;

namespace {

// 重庆上空某高度的相机（ECEF）。
Vec3 cameraAbove(const Cartographic& ground, double altitudeMeters) {
    return Ellipsoid::WGS84().cartographicToCartesian(
        Cartographic(ground.longitude(), ground.latitude(), altitudeMeters));
}

// 兴趣矩形：以 ground 为中心、半径 halfSpanDeg 的经纬窗口。
Rectangle interestAround(const Cartographic& center, double halfSpanDeg) {
    return Rectangle(center.longitude() - degreesToRadians(halfSpanDeg),
                     center.latitude() - degreesToRadians(halfSpanDeg),
                     center.longitude() + degreesToRadians(halfSpanDeg),
                     center.latitude() + degreesToRadians(halfSpanDeg));
}

// 统计结果：层级 → 瓦片数、总瓦片数、最深层级。
struct Stats {
    int total = 0;
    int maxLevel = -1;
    std::map<int, int> perLevel;
};
Stats collect(const TerrainLodResult& r) {
    Stats s;
    for (const TileKey& k : r.tiles) {
        s.total++;
        s.perLevel[k.z()]++;
        s.maxLevel = std::max(s.maxLevel, k.z());
    }
    return s;
}

// 是否存在"父子同时被选中"（四叉树划分不应有）。
bool hasAncestorPair(const TerrainLodResult& r) {
    for (const TileKey& a : r.tiles) {
        for (const TileKey& b : r.tiles) {
            if (a.z() >= b.z()) {
                continue;
            }
            const auto anc = b.ancestor(b.z() - a.z());
            if (anc && anc.value() == a) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

TEST(TerrainLodSelector, FarCameraSelectsCoarseTiles) {
    const WebMercatorTileScheme scheme;
    const TerrainLodSelector selector;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);
    // 高空 300 km：视场 ~60° 的地面兴趣窗 ±2°。
    const Rectangle interest = interestAround(ground, 2.0);
    const Vec3 camera = cameraAbove(ground, 300000.0);

    TerrainLodConfig config;
    config.viewportHeightPx = 1080.0;
    config.fovRadians = degreesToRadians(60.0);
    config.maxScreenSpaceErrorPx = 16.0;
    config.geometricErrorScale = 0.001; // 默认代理
    config.maxLevel = 14;
    const TerrainLodResult result = selector.selectTiles(scheme, camera, interest, config);

    ASSERT_FALSE(result.tiles.empty());
    const Stats stats = collect(result);
    // 300 km 高度 + 16px 预算 + scale=1e-3：误差代理 ≈ 5km/瓦 即停 → z3~z4，瓦数小。
    EXPECT_LE(stats.maxLevel, 5);
    EXPECT_LT(stats.total, 64);
    EXPECT_GE(stats.maxLevel, 2);

    // 划分性质：无父子同时出现。
    EXPECT_FALSE(hasAncestorPair(result));
    // 停止性：选中瓦的 z 要么 = maxLevel，要么 sse ≤ 阈值（同款代理 + 最近点距离重算）。
    const Ellipsoid& e = Ellipsoid::WGS84();
    for (const TileKey& k : result.tiles) {
        if (k.z() == config.maxLevel) {
            continue;
        }
        const Rectangle r = scheme.tileRectangleRadians(k);
        const Cartographic cam = e.cartesianToCartographic(camera);
        const double lon = std::clamp(cam.longitude(), r.west(), r.east());
        const double lat = std::clamp(cam.latitude(), r.south(), r.north());
        const double dist = camera.distanceTo(e.cartographicToCartesian(Cartographic(lon, lat, 0.0)));
        const double err = scheme.tileSizeMeters(k.z()).x() * config.geometricErrorScale;
        const double sse = QuadtreeGeometricError::screenSpaceError(
            err, dist, config.viewportHeightPx, config.fovRadians);
        EXPECT_LE(sse, config.maxScreenSpaceErrorPx * (1.0 + 1.0e-6)) << k.toString();
    }
}

TEST(TerrainLodSelector, NearCameraSelectsDeepTiles) {
    const WebMercatorTileScheme scheme;
    const TerrainLodSelector selector;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);
    // 低空 1.5 km，小兴趣窗 ±0.01°。
    const Rectangle interest = interestAround(ground, 0.01);
    const Vec3 camera = cameraAbove(ground, 1500.0);

    TerrainLodConfig config;
    config.viewportHeightPx = 1080.0;
    config.fovRadians = degreesToRadians(60.0);
    config.maxScreenSpaceErrorPx = 4.0;
    config.geometricErrorScale = 0.001;
    config.maxLevel = 14;
    const TerrainLodResult result = selector.selectTiles(scheme, camera, interest, config);

    ASSERT_FALSE(result.tiles.empty());
    const Stats stats = collect(result);
    // 近地（1.5 km）+ 4px + scale=1e-3：误差预算 ~6 m → z13 瓦（4.9 km 瓦×1e-3）。
    EXPECT_GE(stats.maxLevel, 12);
    EXPECT_LE(stats.maxLevel, config.maxLevel);
    EXPECT_GE(stats.perLevel.at(stats.maxLevel), 1);
    EXPECT_FALSE(hasAncestorPair(result));
}

TEST(TerrainLodSelector, CoverageContainsInterestCenter) {
    const WebMercatorTileScheme scheme;
    const TerrainLodSelector selector;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);

    for (const double altitude : {100000.0, 20000.0, 5000.0}) {
        const Rectangle interest = interestAround(ground, 0.5);
        const Vec3 camera = cameraAbove(ground, altitude);
        TerrainLodConfig config;
        config.maxScreenSpaceErrorPx = 8.0;
        config.maxLevel = 12;
        const TerrainLodResult result = selector.selectTiles(scheme, camera, interest, config);
        ASSERT_FALSE(result.tiles.empty());

        // 兴趣中心点必须被某个选中瓦覆盖。
        bool covered = false;
        for (const TileKey& k : result.tiles) {
            const Rectangle r = scheme.tileRectangleRadians(k);
            if (ground.longitude() >= r.west() && ground.longitude() <= r.east() &&
                ground.latitude() >= r.south() && ground.latitude() <= r.north()) {
                covered = true;
                break;
            }
        }
        EXPECT_TRUE(covered) << "altitude " << altitude;
    }
}

TEST(TerrainLodSelector, MaxLevelRespectedAndNoRunaway) {
    const WebMercatorTileScheme scheme;
    const TerrainLodSelector selector;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);
    // 极限近景 + 极小阈值：应停在 maxLevel。
    const Rectangle interest = interestAround(ground, 0.002);
    const Vec3 camera = cameraAbove(ground, 300.0);

    TerrainLodConfig config;
    config.maxScreenSpaceErrorPx = 0.5;
    config.maxLevel = 10;
    const TerrainLodResult result = selector.selectTiles(scheme, camera, interest, config);
    ASSERT_FALSE(result.tiles.empty());
    const Stats stats = collect(result);
    EXPECT_EQ(stats.maxLevel, 10);
    for (const TileKey& k : result.tiles) {
        EXPECT_LE(k.z(), config.maxLevel);
    }
    // 近景兴趣窗应该细化到 maxLevel 的小瓦（10 层瓦约 0.07°）。
    EXPECT_GE(stats.perLevel.at(10), 1);
}

TEST(TerrainLodSelector, DisjointInterestReturnsEmpty) {
    const WebMercatorTileScheme scheme;
    const TerrainLodSelector selector;
    // 兴趣矩形完全在世界外（纬度越出 Web Mercator 上限）。
    const Rectangle bad = Rectangle::fromDegrees(0.0, 89.0, 1.0, 89.5);
    const Vec3 camera = cameraAbove(Cartographic::fromDegrees(0.0, 0.0), 10000.0);
    TerrainLodConfig config;
    config.maxLevel = 8;
    const TerrainLodResult result = selector.selectTiles(scheme, camera, bad, config);
    EXPECT_TRUE(result.tiles.empty());
}

TEST(TerrainLodSelector, DeterministicForSameInputs) {
    const WebMercatorTileScheme scheme;
    const TerrainLodSelector selector;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);
    const Rectangle interest = interestAround(ground, 1.0);
    const Vec3 camera = cameraAbove(ground, 50000.0);
    TerrainLodConfig config;
    config.maxScreenSpaceErrorPx = 16.0;
    config.maxLevel = 14;

    const TerrainLodResult r1 = selector.selectTiles(scheme, camera, interest, config);
    const TerrainLodResult r2 = selector.selectTiles(scheme, camera, interest, config);
    ASSERT_EQ(r1.tiles.size(), r2.tiles.size());
    for (size_t i = 0; i < r1.tiles.size(); ++i) {
        EXPECT_EQ(r1.tiles[i], r2.tiles[i]);
    }
}
