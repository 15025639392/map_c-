// 影像可用性决议（影像北极星）：缺失时沿父链退化到更低分辨率的真实祖先数据，
// 绝不空洞、绝不用假数据（根瓦/0/合成数据）冒充。覆盖：
//   自身可用 → 自身；缺 1 层 / 缺多层 → 最近可用祖先键正确；全链不可用 → nullopt；
//   谓词由 false 变 true（数据到达）后对同一请求重评 → 决议自身。
#include <gtest/gtest.h>

#include <functional>
#include <optional>

#include "earth_engine/imagery/ImageryTileAvailability.h"
#include "earth_engine/tiling/TileKey.h"

namespace earth_engine {
namespace {

using std::nullopt;
using std::optional;

/// 测试 helper：按 z 范围判定可用性的源谓词。
/// @param minZ 可用最低层级（含），maxZ 可用最高层级（含）；minZ<=z<=maxZ → 可用。
std::function<bool(const TileKey&)> makeZRangeAvailable(int minZ, int maxZ) {
    return [minZ, maxZ](const TileKey& k) { return k.z() >= minZ && k.z() <= maxZ; };
}

TEST(ImageryTileDegrade, ResolvesSelfWhenOwnTileAvailable) {
    const auto hasData = makeZRangeAvailable(3, 6); // 请求层 4 在范围内
    const TileKey request(4, 5, 7);

    const ImageryTileResolution res = resolveImageryTile(hasData, request);

    ASSERT_TRUE(res.covered);
    ASSERT_TRUE(res.resolved.has_value());
    EXPECT_EQ(*res.resolved, request) << "自身可用时必须决议为自身";
    EXPECT_EQ(res.fallbackDepth, 0);
}

TEST(ImageryTileDegrade, FallsBackOneLevelToNearestAvailableAncestor) {
    // z=4 全缺，z=3 层可用 → 缺 1 层，决议为最近可用祖先（z=3）。
    const auto hasData = makeZRangeAvailable(3, 3);
    const TileKey request(4, 5, 7); // 父 = (3, 2, 3)

    const ImageryTileResolution res = resolveImageryTile(hasData, request);

    ASSERT_TRUE(res.covered);
    ASSERT_TRUE(res.resolved.has_value());
    EXPECT_EQ(*res.resolved, TileKey(3, 2, 3));
    EXPECT_EQ(res.fallbackDepth, 1);
}

TEST(ImageryTileDegrade, FallsBackAcrossMultipleLevelsToNearestAvailableAncestor) {
    // z=6..5 全缺、z=4 层可用 → 缺多层，跳过所有不可用中间层直达 z=4 祖先。
    const auto hasData = makeZRangeAvailable(4, 4);
    const TileKey request(6, 21, 13);
    // 祖先链：z=5 (10,6) → z=4 (5,3)。
    const optional<TileKey> expectAncestor = request.ancestor(2);

    const ImageryTileResolution res = resolveImageryTile(hasData, request);

    ASSERT_TRUE(res.covered);
    ASSERT_TRUE(res.resolved.has_value());
    ASSERT_TRUE(expectAncestor.has_value());
    EXPECT_EQ(*res.resolved, *expectAncestor);
    EXPECT_EQ(*res.resolved, TileKey(4, 5, 3));
    EXPECT_EQ(res.fallbackDepth, 2);
}

TEST(ImageryTileDegrade, ReturnsNulloptWhenWholeAncestorChainUnavailable) {
    // 全链（含根瓦）不可用 → 明确 nullopt，绝不返回根瓦/0 冒充"假数据"。
    const auto hasData = makeZRangeAvailable(9, 12); // 请求层 4 远低于可用层，且链上无可用
    const TileKey request(4, 5, 7);

    const ImageryTileResolution res = resolveImageryTile(hasData, request);

    EXPECT_FALSE(res.covered);
    EXPECT_FALSE(res.resolved.has_value()) << "全链不可用必须返回 nullopt，禁止假数据冒充";
    EXPECT_EQ(res.resolved, nullopt);
}

TEST(ImageryTileDegrade, NeverReportsFakeDataAtRootWhenOnlyDeepDataExists) {
    // 只有极高层数据（z=10..12），请求 z=0 根瓦：根瓦自身不可用且无父 → nullopt，
    // 即使"低层假说"允许也不得用其它瓦片冒充。
    const auto hasData = makeZRangeAvailable(10, 12);
    const TileKey rootRequest(0, 0, 0);

    const ImageryTileResolution res = resolveImageryTile(hasData, rootRequest);

    EXPECT_FALSE(res.covered);
    EXPECT_FALSE(res.resolved.has_value());
}

TEST(ImageryTileDegrade, ReEvaluateAfterPredicateTurnsTrueResolvesSelf) {
    // 数据到达：同一源谓词对象由 false 变 true（无状态纯函数重评）。
    bool dataAvailable = false;
    const std::function<bool(const TileKey&)> hasData = [&dataAvailable](const TileKey& k) {
        return dataAvailable && k.z() >= 4 && k.z() <= 6;
    };
    const TileKey request(5, 11, 9);

    const ImageryTileResolution before = resolveImageryTile(hasData, request);
    EXPECT_FALSE(before.covered);
    EXPECT_EQ(before.resolved, nullopt) << "数据未到时必须明确为空";

    dataAvailable = true; // 数据到达
    const ImageryTileResolution after = resolveImageryTile(hasData, request);

    ASSERT_TRUE(after.covered);
    ASSERT_TRUE(after.resolved.has_value());
    EXPECT_EQ(*after.resolved, request) << "数据到达后重评应决议为自身";
    EXPECT_EQ(after.fallbackDepth, 0);
}

} // namespace
} // namespace earth_engine
