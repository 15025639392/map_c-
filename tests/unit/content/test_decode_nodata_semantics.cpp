// no-data 哨兵语义（并入 gis-md 解码 worker 语义，B1/A4 merge）——
// Terrain-RGB 隐式注册 + min/max 排除 + 采样仅有效角归一化。
//
// case 转写自 gis-md：
//   scaffold/tests/unit/content/test_heightmap_terrain.cpp   （decode 契约 + 隐式哨兵）
//   scaffold/tests/unit/tiling/test_decoded_heightmap_sampler.cpp （nodata 环/哨兵传播）
// 数值口径与本仓顶点栅格形态（borderInset=0）一致；Mapbox 514 重叠环/半像素
// 内缩属 B2（cell-registered 源语义），不在此文件。
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapSampler.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/Vec3.h"
#include "earth_engine/providers/ITileBytesSource.h"
#include "earth_engine/providers/TerrainRgbTileSource.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

// 内存 RGB fixture：heightAt 返回 nullopt 的像素编成黑（Terrain-RGB RGB(0,0,0)
// = 数据空洞底值 -10000）；其余按 Terrain-RGB 编码。
class RgbRampFixture : public ITileBytesSource {
public:
    RgbRampFixture(std::function<std::optional<double>(int col, int row)> heightAt, int size)
        : heightAt_(std::move(heightAt)), size_(size) {}

    std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey&,
                                                         const std::string&) const override {
        std::vector<uint8_t> bytes(static_cast<size_t>(size_) * size_ * 3);
        for (int row = 0; row < size_; ++row) {
            for (int col = 0; col < size_; ++col) {
                const size_t px = (static_cast<size_t>(row) * size_ + col) * 3;
                const std::optional<double> h = heightAt_(col, row);
                if (!h) {
                    bytes[px] = 0; // 数据空洞 → RGB(0,0,0)
                    bytes[px + 1] = 0;
                    bytes[px + 2] = 0;
                    continue;
                }
                uint8_t r, g, b;
                HeightmapCodec::encodeTerrainRgbPixel(*h, r, g, b);
                bytes[px] = r;
                bytes[px + 1] = g;
                bytes[px + 2] = b;
            }
        }
        return bytes;
    }

private:
    std::function<std::optional<double>(int, int)> heightAt_;
    int size_;
};

// 5×5 线性场 100 + 10·col + 2·row，中间像素 (2,2) 挖空（黑）。
std::optional<double> rampWithHole(int col, int row) {
    if (col == 2 && row == 2) {
        return std::nullopt;
    }
    return 100.0 + 10.0 * static_cast<double>(col) + 2.0 * static_cast<double>(row);
}

} // namespace

// ---- 解码契约：黑像素解码 == -10000 底值（哨兵的编码侧恒等式） ---------------

TEST(DecodeNoDataSemantics, BlackPixelDecodesToImplicitFloor) {
    const uint8_t black[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 2×2×3
    EXPECT_DOUBLE_EQ(HeightmapCodec::decodeTerrainRgbPixel(0, 0, 0),
                     HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    double out[4] = {0.0, 0.0, 0.0, 0.0};
    ASSERT_TRUE(HeightmapCodec::decodeTerrainRgb(black, 2, 2, 6, out));
    for (int i = 0; i < 4; ++i) {
        EXPECT_DOUBLE_EQ(out[i], HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    }
}

// ---- 解码源隐式注册（镜像 gis-md decodeTile：RGB(0,0,0) 注册进哨兵表） ---------

TEST(DecodeNoDataSemantics, TerrainRgbSourceRegistersImplicitSentinel) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(key.has_value());
    constexpr int kSize = 5;
    // 顶行（row 0 = 北）整行黑 = 空洞带；其余 100..148 线性场。
    RgbRampFixture fixture(
        [](int col, int row) -> std::optional<double> {
            if (row == 0) {
                return std::nullopt;
            }
            return rampWithHole(col, row);
        },
        kSize);
    const TerrainRgbTileSource source(fixture, "https://t/{z}/{x}/{y}.png");
    const auto grid = source.requestHeights(scheme, key.value(), kSize);
    ASSERT_TRUE(grid.has_value());
    ASSERT_EQ(grid->noDataValues.size(), 1u);
    EXPECT_DOUBLE_EQ(grid->noDataValues[0], HeightmapCodec::kTerrainRgbNoDataFloorMeters);
    // 空洞行样本值即底值。
    for (int col = 0; col < kSize; ++col) {
        EXPECT_DOUBLE_EQ(grid->heights[static_cast<size_t>(col)], -10000.0) << "col " << col;
    }
}

// ---- HeightmapTile：min/max 排除哨兵；无哨兵表时全量扫描（旧行为不变） --------

TEST(DecodeNoDataSemantics, TileMinMaxExcludesSentinelsOnlyWhenRegistered) {
    const WebMercatorTileScheme scheme;
    const TileKey key(9, 200, 100);
    // 5×5：外圈 = -10000（空洞），内 3×3 = 200..240 线性场。
    constexpr int kSize = 5;
    std::vector<double> heights;
    heights.reserve(static_cast<size_t>(kSize) * kSize);
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const bool ring = row == 0 || row == kSize - 1 || col == 0 || col == kSize - 1;
            heights.push_back(ring ? -10000.0
                                   : 200.0 + 10.0 * static_cast<double>(col) +
                                         5.0 * static_cast<double>(row));
        }
    }
    const std::vector<double> sentinels{HeightmapCodec::kTerrainRgbNoDataFloorMeters};

    // 注册哨兵 → min/max 只看有效内场。
    const HeightmapTile tileWith(scheme, key, heights.data(), kSize, kSize, sentinels.data(),
                                 static_cast<int>(sentinels.size()));
    const auto [minEx, maxEx] = tileWith.minMaxHeight();
    EXPECT_NEAR(minEx, 200.0 + 10.0 + 5.0, 1e-9); // (1,1)：200+10+5
    EXPECT_NEAR(maxEx, 200.0 + 30.0 + 15.0, 1e-9); // (3,3)：200+30+15

    // 未注册 → 全量扫描（-10000 混入；旧语义，供对照/兼容）。
    const HeightmapTile tilePlain(scheme, key, heights.data(), kSize, kSize);
    const auto [minPlain, maxPlain] = tilePlain.minMaxHeight();
    EXPECT_DOUBLE_EQ(minPlain, -10000.0);
    EXPECT_NEAR(maxPlain, 245.0, 1e-9);

    // 全哨兵瓦：无有效高度 → {0,0}（镜像 gis-md assignHeights）。
    const std::vector<double> allSentinels(kSize * kSize, -10000.0);
    const HeightmapTile tileAll(scheme, key, allSentinels.data(), kSize, kSize,
                                sentinels.data(), static_cast<int>(sentinels.size()));
    EXPECT_EQ(tileAll.minMaxHeight(), (std::pair<double, double>{0.0, 0.0}));
}

// ---- 采样：哨兵角被排除并归一化（转写 gis-md sampleBilinear 语义） ------------

TEST(DecodeNoDataSemantics, SamplerRenormalizesNoDataCorner) {
    // 2×2：左上角 = -10000 空洞。中心点 (0.5,0.5) 原始双线性会混出 -2485 的
    // 假值；排除左上角后仅剩三有效角 → (10+20+30)·0.25 / 0.75 = 20。
    const double heights[4] = {-10000.0, 10.0, 20.0, 30.0};
    const double sentinel[1] = {-10000.0};
    const HeightmapSampler sampler(heights, 2, 2, sentinel, 1);
    EXPECT_DOUBLE_EQ(sampler.sampleBilinear(0.5, 0.5), 20.0);
    EXPECT_TRUE(sampler.isNoData(-10000.0));
    EXPECT_TRUE(sampler.isNoData(1.0e6)); // >50000 规则
    EXPECT_FALSE(sampler.isNoData(20.0));
}

TEST(DecodeNoDataSemantics, SamplerPropagatesSentinelWhenAllCornersNoData) {
    // 四角全空洞：无有效数据 → 回传哨兵值（"此处无数据"信号上抛，isNoData 可判）。
    const double heights[4] = {-10000.0, -10000.0, -10000.0, -10000.0};
    const double sentinel[1] = {-10000.0};
    const HeightmapSampler sampler(heights, 2, 2, sentinel, 1);
    EXPECT_DOUBLE_EQ(sampler.sampleBilinear(0.5, 0.5), -10000.0);
    EXPECT_TRUE(sampler.isNoData(sampler.sampleBilinear(0.5, 0.5)));
}

TEST(DecodeNoDataSemantics, SamplerRawBlendWithoutSentinelTableUnchanged) {
    // 未注册哨兵：采样路径保持纯双线性（结果与旧实现逐位一致）——哨兵语义
    // 只属于已注册的解码栅格，普通用法零扰动。
    const double heights[4] = {-10000.0, 10.0, 20.0, 30.0};
    const HeightmapSampler sampler(heights, 2, 2); // 无哨兵表
    EXPECT_DOUBLE_EQ(sampler.sampleBilinear(0.5, 0.5), (-10000.0 + 10.0 + 20.0 + 30.0) / 4.0);
}

TEST(DecodeNoDataSemantics, SamplerExcludesOversizeCornerAsNoData) {
    // 哨兵路径下 >50000 的角（解码垃圾）同样被排除（镜像 gis-md isNoData）。
    const double heights[4] = {1.0e6, 10.0, 20.0, 30.0};
    const double sentinel[1] = {-10000.0}; // 表非空即启用哨兵路径
    const HeightmapSampler sampler(heights, 2, 2, sentinel, 1);
    EXPECT_DOUBLE_EQ(sampler.sampleBilinear(0.5, 0.5), 20.0);
}

// ---- 帧级端到端：洞不污染 frame min/max、网格不向 -10000 沉 ------------------

TEST(DecodeNoDataSemantics, FrameMinMaxAndMeshExcludeHole) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 10);
    ASSERT_TRUE(key.has_value());
    constexpr int kSize = 5;
    RgbRampFixture fixture(rampWithHole, kSize);
    const TerrainRgbTileSource source(fixture, "https://t/{z}/{x}/{y}.png");

    TerrainLodResult selection;
    selection.tiles = {key.value()};
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();
    const TerrainFrameAssembler assembler;
    const auto frames = assembler.assemble(scheme, selection, source, ellipsoid, kSize, 5);
    ASSERT_EQ(frames.size(), 1u);
    const TerrainFrameAssembler::Frame& frame = frames.front();
    // frame min/max 来自瓦 min/max（已排除 -10000 空洞）：有效场 100..148。
    EXPECT_NEAR(frame.minHeight, 100.0, 0.01);
    EXPECT_NEAR(frame.maxHeight, 148.0, 0.01);
    // 网格没有任何节点沉向 -10000（采样哨兵角时仅有效角归一化）。
    ASSERT_FALSE(frame.mesh.positionsEcef.empty());
    for (const Vec3& p : frame.mesh.positionsEcef) {
        const Cartographic c = ellipsoid.cartesianToCartographic(p);
        EXPECT_GT(c.height(), 90.0) << "node sunk toward the -10000 hole";
        EXPECT_LT(c.height(), 160.0);
    }
}

// ---- 对照：同一源若未注册哨兵，frame min 会被 -10000 污染 --------------------
// （钉住"注册"是语义生效点；也防未来有人把注册挪走而不自知。）

TEST(DecodeNoDataSemantics, WithoutRegistrationFrameMinIsPoisoned) {
    const WebMercatorTileScheme scheme;
    const auto key = scheme.tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 10);
    ASSERT_TRUE(key.has_value());
    constexpr int kSize = 5;
    RgbRampFixture fixture(rampWithHole, kSize);

    // 手工复制"未注册"路径：直接解码栅格但不带哨兵表。
    const std::vector<uint8_t> bytes = *fixture.requestTileBytes(key.value(), "");
    std::vector<double> heights(static_cast<size_t>(kSize) * kSize);
    ASSERT_TRUE(HeightmapCodec::decodeTerrainRgb(bytes.data(), kSize, kSize, kSize * 3,
                                                 heights.data()));
    const TerrainGrid grid{heights, kSize, kSize, {}}; // 空哨兵表 = 未注册
    const HeightmapTile plainTile(scheme, key.value(), grid.heights.data(), grid.width,
                                  grid.height);
    const auto [minPlain, maxPlain] = plainTile.minMaxHeight();
    EXPECT_DOUBLE_EQ(minPlain, -10000.0); // 空洞把包围体 min 拉穿
    EXPECT_NEAR(maxPlain, 148.0, 0.01);
}
