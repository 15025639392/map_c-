// 祖先回退数据源（调度-lite）：缺瓦沿父链上溯重采样 → 帧不出现空洞。
//
// 对应 stage6-merge-checkpoint「TerrainFrameCache ↔ 调度/帧收敛」行与本仓 roadmap
// 「缺失瓦跳过（祖先回退属调度阶段）」缺口——host 侧先落「父数据、子几何」占位
// 内容（重采样到子瓦栅格，下游无感知）。跨级无缝（T 顶点 remap）仍属 B4。
#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/content/AncestorFallbackDataSource.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

/// 程序化高度源：按瓦把函数采样到 gridSize² 栅格；可选「某层整层失败」。
class FunctionalTerrainSource : public ITerrainDataSource {
public:
    explicit FunctionalTerrainSource(std::function<double(const Cartographic&)> fn,
                                     int failAtZ = -1)
        : fn_(std::move(fn)), failAtZ_(failAtZ) {}

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override {
        if (failAtZ_ >= 0 && key.z() == failAtZ_) {
            return std::nullopt; // 模拟该层数据缺失/瞬时失败
        }
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
    int failAtZ_ = -1;
};

constexpr int kGrid = 33;

// 请求瓦像素 (col,row) 的地理坐标（与源内部同口径，用于期望值）。
Cartographic probeCarto(const WebMercatorTileScheme& scheme, const TileKey& key, int col,
                        int row) {
    std::vector<double> zeros(static_cast<size_t>(kGrid) * kGrid, 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), kGrid, kGrid);
    return probe.pixelToCartographic(col, row);
}

} // namespace

TEST(AncestorFallback, DirectHitPassesThrough) {
    const WebMercatorTileScheme scheme;
    const TileKey key(10, 500, 300);
    FunctionalTerrainSource inner(hillFn);
    const AncestorFallbackDataSource fallback(inner);
    const auto grid = fallback.requestHeights(scheme, key, kGrid);
    ASSERT_TRUE(grid.has_value());
    // 直接命中 = 源原值（逐位一致）。
    for (int row = 0; row < kGrid; ++row) {
        for (int col = 0; col < kGrid; ++col) {
            EXPECT_DOUBLE_EQ(grid->heights[static_cast<size_t>(row) * kGrid + col],
                             hillFn(probeCarto(scheme, key, col, row)));
        }
    }
}

TEST(AncestorFallback, ChildFilledFromAncestorGrid) {
    // 子层（z11）整层缺失 → 回退到父层（z10）栅格重采样。
    const WebMercatorTileScheme scheme;
    const TileKey child(11, 1000, 600);
    const TileKey parent(10, 500, 300);
    ASSERT_EQ(child.parent().value(), parent);
    FunctionalTerrainSource inner(hillFn, /*failAtZ=*/11);
    const AncestorFallbackDataSource fallback(inner);
    const auto grid = fallback.requestHeights(scheme, child, kGrid);
    ASSERT_TRUE(grid.has_value());
    EXPECT_EQ(grid->width, kGrid);
    EXPECT_EQ(grid->height, kGrid);
    // 重采样值 ≈ 世界函数在子瓦像素处的值（父栅格双线性；函数平滑 → 亚米级误差）。
    for (int row = 0; row < kGrid; row += 4) {
        for (int col = 0; col < kGrid; col += 4) {
            EXPECT_NEAR(grid->heights[static_cast<size_t>(row) * kGrid + col],
                        hillFn(probeCarto(scheme, child, col, row)), 1.0)
                << "col " << col << " row " << row;
        }
    }
}

TEST(AncestorFallback, NoDataAnywhereYieldsNullopt) {
    const WebMercatorTileScheme scheme;
    FunctionalTerrainSource inner(hillFn, /*failAtZ=*/10); // z10 失败 → z9 链上只有 z9 成功?…
    // failAtZ=10：请求 z11 → 父 z10 也失败 → 上溯到 z9 成功 → 仍应返回（更深回退）。
    const AncestorFallbackDataSource fallback(inner);
    const auto grid = fallback.requestHeights(scheme, TileKey(11, 1000, 600), kGrid);
    ASSERT_TRUE(grid.has_value()); // 深一层回退同样生效

    // 全部失败：无任何祖先可用 → nullopt（不冒充数据）。
    class AlwaysEmptySource : public ITerrainDataSource {
    public:
        std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme&, const TileKey&,
                                                  int) const override {
            return std::nullopt;
        }
    };
    AlwaysEmptySource empty;
    const AncestorFallbackDataSource emptyFallback(empty);
    EXPECT_FALSE(emptyFallback.requestHeights(scheme, TileKey(11, 1000, 600), kGrid).has_value());
}

TEST(AncestorFallback, RespectsMaxFallbackLevels) {
    const WebMercatorTileScheme scheme;
    // z13 请求；数据只在 z9 存在 → 回退 4 层（13→9）刚好够；限制 3 层则失败。
    FunctionalTerrainSource inner(hillFn, -1);
    class OnlyZ9Source : public FunctionalTerrainSource {
    public:
        OnlyZ9Source() : FunctionalTerrainSource(&onlyZ9fn) {}
        std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                                  const TileKey& key, int gridSize) const override {
            if (key.z() != 9) {
                return std::nullopt;
            }
            return FunctionalTerrainSource::requestHeights(scheme, key, gridSize);
        }
        static double onlyZ9fn(const Cartographic& c) { return hillFn(c); }
    };
    OnlyZ9Source onlyZ9;
    const AncestorFallbackDataSource with4(onlyZ9, 4);
    EXPECT_TRUE(with4.requestHeights(scheme, TileKey(13, 4000, 2400), kGrid).has_value());
    const AncestorFallbackDataSource with3(onlyZ9, 3); // 13→12→11→10：到不了 z9
    EXPECT_FALSE(with3.requestHeights(scheme, TileKey(13, 4000, 2400), kGrid).has_value());
}

TEST(AncestorFallback, AssemblerFrameWithoutHole) {
    // 端到端：选择含一个子层瓦（其源缺失）→ 无回退时空帧缺瓦；有回退时帧齐且
    // 网格来自祖先数据（父数据、子几何）。
    const WebMercatorTileScheme scheme;
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();
    const TileKey child(11, 1000, 600);
    FunctionalTerrainSource inner(hillFn, /*failAtZ=*/11);
    TerrainLodResult selection;
    selection.tiles = {child};

    const TerrainFrameAssembler assembler;
    const auto bare = assembler.assemble(scheme, selection, inner, ellipsoid, kGrid, 8);
    EXPECT_TRUE(bare.empty()); // 无回退：缺瓦跳过 → 帧空洞

    const AncestorFallbackDataSource fallback(inner);
    const auto frames = assembler.assemble(scheme, selection, fallback, ellipsoid, kGrid, 8);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames.front().key, child); // 仍是子瓦键（几何在子瓦）
    EXPECT_FALSE(frames.front().mesh.positionsEcef.empty());
}
