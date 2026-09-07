#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/content/HeightmapTile.h"
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

TEST(TerrainCrossLevel, SharedGridPointsCoincideBetweenLevels) {
    // T-V5 跨级半边：粗瓦 A(z) 与东邻 B(z) 的北/南半区子瓦（z+1）共享同一物理边界。
    // 两侧都贴边界采样 + 同 fn → **偶数行共享网格点 ECEF 逐点重合**（±1e-6 m）；
    // 子瓦奇数行落在粗瓦网格点之间（T 顶点）——不同细分的自然结果，需 stage-6
    // remap 域处理，本测试只钉"共享网格点一致"这一机制前提。
    const WebMercatorTileScheme scheme;
    const auto keyA = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(keyA.has_value());
    const TileKey A = keyA.value();
    const TileKey B(A.z(), A.x() + 1, A.y());
    ASSERT_TRUE(B.isValid());
    const auto bChildren = B.children();
    const TileKey childNW = bChildren[0]; // 北半区
    const TileKey childSW = bChildren[2]; // 南半区

    TerrainLodResult selection;
    selection.tiles = {A, childNW, childSW};
    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const Ellipsoid& e = Ellipsoid::WGS84();
    const auto frames = assembler.assemble(scheme, selection, source, e, 17, 16);
    ASSERT_EQ(frames.size(), 3u);
    const TerrainMeshData* meshA = nullptr;
    const TerrainMeshData* meshNW = nullptr;
    const TerrainMeshData* meshSW = nullptr;
    for (const auto& f : frames) {
        if (f.key == A) {
            meshA = &f.mesh;
        } else if (f.key == childNW) {
            meshNW = &f.mesh;
        } else if (f.key == childSW) {
            meshSW = &f.mesh;
        }
    }
    ASSERT_TRUE(meshA && meshNW && meshSW);

    auto expectEqual = [](const Vec3& p, const Vec3& q, int aRow, int cRow, const char* half) {
        EXPECT_NEAR(p.x(), q.x(), 1.0e-6) << half << " A row " << aRow << " vs child row " << cRow;
        EXPECT_NEAR(p.y(), q.y(), 1.0e-6) << half << " A row " << aRow << " vs child row " << cRow;
        EXPECT_NEAR(p.z(), q.z(), 1.0e-6) << half << " A row " << aRow << " vs child row " << cRow;
    };

    // 北半区：A 东边行 a ↔ NW 西边行 2a（a=0..8）。
    for (int a = 0; a <= 8; ++a) {
        const int c = 2 * a;
        expectEqual(meshA->positionsEcef[static_cast<size_t>(a * 17 + 16)],
                    meshNW->positionsEcef[static_cast<size_t>(c * 17 + 0)], a, c, "NW");
    }
    // 南半区：A 东边行 8+c/2 ↔ SW 西边行 c（c=0,2,...,16）。
    for (int c = 0; c <= 16; c += 2) {
        const int a = 8 + c / 2;
        expectEqual(meshA->positionsEcef[static_cast<size_t>(a * 17 + 16)],
                    meshSW->positionsEcef[static_cast<size_t>(c * 17 + 0)], a, c, "SW");
    }
}

TEST(TerrainCrossLevel, ParentAndChildCoverageConsistent) {
    // 父瓦 A 的中心 = 四个子瓦（z+1）的角点：验证子瓦外角与父网格中心顶点一致。
    const WebMercatorTileScheme scheme;
    const auto keyA = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(keyA.has_value());
    const TileKey A = keyA.value();
    const auto children = A.children();

    // A 的网格节点正好落在子瓦角点（同 fn、同尺寸栅格）。
    TerrainLodResult selection;
    selection.tiles = {A};
    const FunctionalTerrainSource source(hillFn);
    const TerrainFrameAssembler assembler;
    const Ellipsoid& e = Ellipsoid::WGS84();
    const auto framesA = assembler.assemble(scheme, selection, source, e, 17, 16);
    ASSERT_EQ(framesA.size(), 1u);
    const Vec3& parentCenter = framesA[0].mesh.positionsEcef[static_cast<size_t>(8 * 17 + 8)];

    // 子瓦 NW 的东南角 = A 中心；SE 的西北角等——取 NW 的 (col16,row16) 与 SW 的 (16,0)。
    selection.tiles = {children[0], children[3]}; // NW, SE
    const auto framesC = assembler.assemble(scheme, selection, source, e, 17, 16);
    ASSERT_EQ(framesC.size(), 2u);
    const TerrainMeshData* mNW = nullptr;
    const TerrainMeshData* mSE = nullptr;
    for (const auto& f : framesC) {
        if (f.key == children[0]) {
            mNW = &f.mesh;
        }
        if (f.key == children[3]) {
            mSE = &f.mesh;
        }
    }
    ASSERT_TRUE(mNW && mSE);
    // NW 东南角 (16,16)。
    EXPECT_NEAR(mNW->positionsEcef[static_cast<size_t>(16 * 17 + 16)].x(), parentCenter.x(), 1.0e-6);
    EXPECT_NEAR(mNW->positionsEcef[static_cast<size_t>(16 * 17 + 16)].y(), parentCenter.y(), 1.0e-6);
    EXPECT_NEAR(mNW->positionsEcef[static_cast<size_t>(16 * 17 + 16)].z(), parentCenter.z(), 1.0e-6);
    // SE 西北角 (0,0)。
    EXPECT_NEAR(mSE->positionsEcef[static_cast<size_t>(0 * 17 + 0)].x(), parentCenter.x(), 1.0e-6);
    EXPECT_NEAR(mSE->positionsEcef[static_cast<size_t>(0 * 17 + 0)].y(), parentCenter.y(), 1.0e-6);
    EXPECT_NEAR(mSE->positionsEcef[static_cast<size_t>(0 * 17 + 0)].z(), parentCenter.z(), 1.0e-6);
}
