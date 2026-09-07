#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/tiling/TerrainLodSelector.h"

using namespace earth_engine;

namespace {

// 光滑地理高度函数（所有瓦共享同一全局函数 → 跨瓦无缝）。
double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

/// 程序化高度数据源：按瓦把函数采样到 gridSize² 栅格（复用 tile 的像素↔地理映射）。
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

/// 每第 Nth 个键失败的源（测试缺失瓦跳过）。
class FlakyTerrainSource : public FunctionalTerrainSource {
public:
    FlakyTerrainSource(std::function<double(const Cartographic&)> fn, int everyNth)
        : FunctionalTerrainSource(std::move(fn)), everyNth_(everyNth) {}

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override {
        counter_ = (counter_ + 1) % everyNth_;
        if (counter_ == 0) {
            return std::nullopt;
        }
        return FunctionalTerrainSource::requestHeights(scheme, key, gridSize);
    }

private:
    mutable int counter_ = 0;
    int everyNth_;
};

/// 若 b 是 a 的正东/正南邻居，逐边比对共享边 ECEF（单向验证，避免反向重复）。
void expectSharedEdgeCoincident(const TerrainFrameAssembler::Frame& a,
                                const TerrainFrameAssembler::Frame& b) {
    const TileKey& ka = a.key;
    const TileKey& kb = b.key;
    if (ka.z() != kb.z() || ka.y() != kb.y() || kb.x() != ka.x() + 1) {
        return; // 不是"a 在西、b 在东"的同层邻居
    }
    const int stride = a.mesh.nodesPerEdge + 1;
    ASSERT_EQ(b.mesh.nodesPerEdge, a.mesh.nodesPerEdge);
    for (int t = 0; t < stride; ++t) {
        // a 东边 = 行 t 的最后一列；b 西边 = 行 t 的第 0 列。
        const size_t iA = static_cast<size_t>(t * stride + (stride - 1));
        const size_t iB = static_cast<size_t>(t * stride + 0);
        EXPECT_NEAR(a.mesh.positionsEcef[iA].x(), b.mesh.positionsEcef[iB].x(), 1.0e-6);
        EXPECT_NEAR(a.mesh.positionsEcef[iA].y(), b.mesh.positionsEcef[iB].y(), 1.0e-6);
        EXPECT_NEAR(a.mesh.positionsEcef[iA].z(), b.mesh.positionsEcef[iB].z(), 1.0e-6);
    }
}

/// a 在北、b 在南（b.y == a.y+1）→ 比对 a 南边 / b 北边。
void expectSouthNeighborCoincident(const TerrainFrameAssembler::Frame& a,
                                   const TerrainFrameAssembler::Frame& b) {
    const TileKey& ka = a.key;
    const TileKey& kb = b.key;
    if (ka.z() != kb.z() || ka.x() != kb.x() || kb.y() != ka.y() + 1) {
        return;
    }
    const int stride = a.mesh.nodesPerEdge + 1;
    ASSERT_EQ(b.mesh.nodesPerEdge, a.mesh.nodesPerEdge);
    for (int t = 0; t < stride; ++t) {
        const size_t iA = static_cast<size_t>((stride - 1) * stride + t); // a 南边
        const size_t iB = static_cast<size_t>(0 * stride + t);            // b 北边
        EXPECT_NEAR(a.mesh.positionsEcef[iA].x(), b.mesh.positionsEcef[iB].x(), 1.0e-6);
        EXPECT_NEAR(a.mesh.positionsEcef[iA].y(), b.mesh.positionsEcef[iB].y(), 1.0e-6);
        EXPECT_NEAR(a.mesh.positionsEcef[iA].z(), b.mesh.positionsEcef[iB].z(), 1.0e-6);
    }
}

} // namespace

TEST(TerrainFrameAssembler, AssemblesSelectionToMeshes) {
    const WebMercatorTileScheme scheme;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);
    const TerrainLodSelector selector;
    const Vec3 camera = Ellipsoid::WGS84().cartographicToCartesian(
        Cartographic(ground.longitude(), ground.latitude(), 50000.0));
    TerrainLodConfig config;
    config.maxScreenSpaceErrorPx = 16.0;
    config.geometricErrorScale = 0.001;
    config.maxLevel = 12;
    const Rectangle interest(ground.longitude() - degreesToRadians(0.5),
                             ground.latitude() - degreesToRadians(0.5),
                             ground.longitude() + degreesToRadians(0.5),
                             ground.latitude() + degreesToRadians(0.5));
    const TerrainLodResult selection = selector.selectTiles(scheme, camera, interest, config);
    ASSERT_FALSE(selection.tiles.empty());

    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const auto frames = assembler.assemble(scheme, selection, source, Ellipsoid::WGS84(), 17, 8);

    EXPECT_EQ(frames.size(), selection.tiles.size());
    for (const auto& frame : frames) {
        // 键与选择一致（顺序一致）。
        EXPECT_TRUE(std::find(selection.tiles.begin(), selection.tiles.end(), frame.key) !=
                    selection.tiles.end());
        EXPECT_EQ(frame.mesh.nodesPerEdge, 8);
        EXPECT_FALSE(frame.mesh.positionsEcef.empty());
        // min/max 与网格高度范围一致（山丘函数在 [200,800]）。
        EXPECT_GT(frame.maxHeight, 200.0);
        EXPECT_LT(frame.minHeight, 800.0);
        EXPECT_LE(frame.minHeight, frame.maxHeight);
    }
}

TEST(TerrainFrameAssembler, NeighborFramesShareSeamlessEdges) {
    // 手工 3 瓦（同 z，A + 东/南邻居）装配后，跨帧共享边逐点重合。
    const WebMercatorTileScheme scheme;
    const auto keyA = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 8);
    ASSERT_TRUE(keyA.has_value());
    const TileKey keyEast(keyA->z(), keyA->x() + 1, keyA->y());
    const TileKey keySouth(keyA->z(), keyA->x(), keyA->y() + 1);
    ASSERT_TRUE(keyEast.isValid());
    ASSERT_TRUE(keySouth.isValid());

    TerrainLodResult selection;
    selection.tiles = {keyA.value(), keyEast, keySouth};

    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const auto frames = assembler.assemble(scheme, selection, source, Ellipsoid::WGS84(), 17, 16);
    ASSERT_EQ(frames.size(), 3u);
    // 配对验证（A-East、A-South）。
    for (const auto& a : frames) {
        for (const auto& b : frames) {
            expectSharedEdgeCoincident(a, b);
            expectSouthNeighborCoincident(a, b);
        }
    }
}

TEST(TerrainFrameAssembler, MissingSourceTileIsSkipped) {
    const WebMercatorTileScheme scheme;
    const auto keyA = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(keyA.has_value());
    TerrainLodResult selection;
    selection.tiles = {keyA.value(), TileKey(keyA->z(), keyA->x() + 1, keyA->y())};

    const FlakyTerrainSource source(hillFn, /*everyNth=*/2); // 第 2 个键失败
    const TerrainFrameAssembler assembler;
    const auto frames = assembler.assemble(scheme, selection, source, Ellipsoid::WGS84(), 9, 4);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].key, keyA.value());
}

TEST(TerrainFrameAssembler, Deterministic) {
    const WebMercatorTileScheme scheme;
    const Cartographic ground = Cartographic::fromDegrees(106.5, 29.7);
    const Vec3 camera = Ellipsoid::WGS84().cartographicToCartesian(
        Cartographic(ground.longitude(), ground.latitude(), 20000.0));
    TerrainLodConfig config;
    config.maxScreenSpaceErrorPx = 8.0;
    config.geometricErrorScale = 0.001;
    config.maxLevel = 11;
    const Rectangle interest(ground.longitude() - degreesToRadians(0.3),
                             ground.latitude() - degreesToRadians(0.3),
                             ground.longitude() + degreesToRadians(0.3),
                             ground.latitude() + degreesToRadians(0.3));
    const TerrainLodSelector selector;
    const TerrainLodResult selection = selector.selectTiles(scheme, camera, interest, config);
    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const auto f1 = assembler.assemble(scheme, selection, source, Ellipsoid::WGS84(), 17, 8);
    const auto f2 = assembler.assemble(scheme, selection, source, Ellipsoid::WGS84(), 17, 8);
    ASSERT_EQ(f1.size(), f2.size());
    for (size_t i = 0; i < f1.size(); ++i) {
        EXPECT_EQ(f1[i].key, f2[i].key);
        EXPECT_EQ(f1[i].mesh.indices.size(), f2[i].mesh.indices.size());
        EXPECT_EQ(f1[i].mesh.positionsEcef.size(), f2[i].mesh.positionsEcef.size());
        for (size_t v = 0; v < f1[i].mesh.positionsEcef.size(); ++v) {
            EXPECT_EQ(f1[i].mesh.positionsEcef[v], f2[i].mesh.positionsEcef[v]);
        }
    }
}
