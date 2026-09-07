// 高程基准改正接入路径（engine-targets §6）：
// HeightDatumCorrectingDataSource 把解码栅格逐样本叠加 undulation
// （椭球高 = 正高 + undulation）。默认恒等改正器走零拷贝快路径（口径不变）；
// 真实 EGM96 网格数据文件为数据侧待办（本沙箱外网不可达，见 NEXT-STEPS）。
#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/content/HeightDatumCorrectingDataSource.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/geodesy/HeightDatumCorrector.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

/// 程序化高度源（同其他装饰器测试的 fixture 形态）。
class FunctionalTerrainSource : public ITerrainDataSource {
public:
    explicit FunctionalTerrainSource(std::function<double(const Cartographic&)> fn,
                                     std::vector<double> noData = {})
        : fn_(std::move(fn)), noData_(std::move(noData)) {}

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override {
        TerrainGrid grid;
        grid.width = gridSize;
        grid.height = gridSize;
        grid.noDataValues = noData_;
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
    std::vector<double> noData_;
};

Cartographic probeGeo(const WebMercatorTileScheme& scheme, const TileKey& key, int col, int row,
                      int gridSize) {
    std::vector<double> zeros(static_cast<size_t>(gridSize) * gridSize, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), gridSize, gridSize);
    return probe.pixelToCartographic(col, row);
}

constexpr int kGrid = 17;

} // namespace

TEST(HeightDatumCorrectingSource, IdentityFastPathBitwiseUnchanged) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    const IdentityHeightDatumCorrector identity;
    FunctionalTerrainSource inner(hillFn);
    const HeightDatumCorrectingDataSource corrected(inner, identity);
    const auto grid = corrected.requestHeights(scheme, key, kGrid);
    ASSERT_TRUE(grid.has_value());
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            EXPECT_DOUBLE_EQ(grid->heights[static_cast<size_t>(row) * kGrid + col],
                             hillFn(probeGeo(scheme, key, col, row, kGrid)));
        }
    }
}

TEST(HeightDatumCorrectingSource, ConstantUndulationShiftsEverySample) {
    // 大范围常数 undulation N=13：每个非哨兵样本恰 +13（含 min/max）。
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300); // 覆盖 106.xE 29.xN 一带（在 80..120E/10..40N 内）
    const double values[20] = {13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
                               13, 13, 13, 13, 13, 13, 13, 13, 13, 13};
    const GridHeightDatumCorrector grid(80.0, 10.0, 10.0, 5, 4, values);
    FunctionalTerrainSource inner(hillFn);
    const HeightDatumCorrectingDataSource corrected(inner, grid);
    const auto out = corrected.requestHeights(scheme, key, kGrid);
    ASSERT_TRUE(out.has_value());
    double minOut = 1e30, maxOut = -1e30;
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            const double expected =
                hillFn(probeGeo(scheme, key, col, row, kGrid)) + 13.0;
            EXPECT_DOUBLE_EQ(out->heights[static_cast<size_t>(row) * kGrid + col], expected);
            minOut = std::min(minOut, out->heights[static_cast<size_t>(row) * kGrid + col]);
            maxOut = std::max(maxOut, out->heights[static_cast<size_t>(row) * kGrid + col]);
        }
    }
    // frame 侧 min/max（来自瓦 min/max）也会整体抬升 13——抽样断言。
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();
    TerrainLodResult selection;
    selection.tiles = {key};
    const TerrainFrameAssembler assembler;
    const auto frames = assembler.assemble(scheme, selection, corrected, ellipsoid, kGrid, 8);
    ASSERT_EQ(frames.size(), 1u);
    const auto bareFrames =
        assembler.assemble(scheme, selection, inner, ellipsoid, kGrid, 8);
    ASSERT_EQ(bareFrames.size(), 1u);
    EXPECT_NEAR(frames.front().minHeight - bareFrames.front().minHeight, 13.0, 1e-9);
    EXPECT_NEAR(frames.front().maxHeight - bareFrames.front().maxHeight, 13.0, 1e-9);
}

TEST(HeightDatumCorrectingSource, NoDataSamplesNotCorrected) {
    // 带哨兵样本：命中 noDataValues 的原样保留（不叠加），其余照常。
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    const double values[20] = {13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
                               13, 13, 13, 13, 13, 13, 13, 13, 13, 13};
    const GridHeightDatumCorrector grid(80.0, 10.0, 10.0, 5, 4, values);
    // 源：半数样本 = -10000 哨兵（数据空洞）。
    FunctionalTerrainSource inner(
        [](const Cartographic& c) {
            const int idx = static_cast<int>(c.longitude() * 1000) % 2;
            return idx == 0 ? -10000.0 : hillFn(c);
        },
        std::vector<double>{-10000.0});
    const HeightDatumCorrectingDataSource corrected(inner, grid);
    const auto out = corrected.requestHeights(scheme, key, kGrid);
    ASSERT_TRUE(out.has_value());
    ASSERT_FALSE(out->noDataValues.empty());
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            const double v = out->heights[static_cast<size_t>(row) * kGrid + col];
            if (v == -10000.0) {
                continue; // 哨兵原样
            }
            const double expected =
                hillFn(probeGeo(scheme, key, col, row, kGrid)) + 13.0;
            EXPECT_DOUBLE_EQ(v, expected);
        }
    }
}

TEST(HeightDatumCorrectingSource, BilinearMatchesDirectCorrectorQuery) {
    // 非恒定 undulation 网格：逐样本 = 源高 + corrector(经纬)（探针路径自洽）。
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    // west=100, south=24, cell=2, 5×5：v(i,j) = 10 + i + 2j（i=lon 下标, j=lat 下标）。
    constexpr int kLon = 5, kLat = 5;
    std::vector<double> values(static_cast<size_t>(kLon) * kLat);
    for (int j = 0; j < kLat; ++j) {
        for (int i = 0; i < kLon; ++i) {
            values[static_cast<size_t>(j) * kLon + i] = 10.0 + i + 2.0 * j;
        }
    }
    const GridHeightDatumCorrector grid(100.0, 24.0, 2.0, kLon, kLat, values.data());
    FunctionalTerrainSource inner(hillFn);
    const HeightDatumCorrectingDataSource corrected(inner, grid);
    const auto out = corrected.requestHeights(scheme, key, kGrid);
    ASSERT_TRUE(out.has_value());
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            const Cartographic geo = probeGeo(scheme, key, col, row, kGrid);
            const double expected = hillFn(geo) + grid.undulationMeters(geo);
            EXPECT_NEAR(out->heights[static_cast<size_t>(row) * kGrid + col], expected, 1e-9);
        }
    }
}
