// TileCacheBytesSource：瓦片字节缓存（S2 资源调度第一步）。
// 命中免重复 IO；失败不入缓存；FIFO 容量淘汰；计数供记账/断言。
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/providers/ITileBytesSource.h"
#include "earth_engine/providers/TileCacheBytesSource.h"
#include "earth_engine/tiling/TileKey.h"

using namespace earth_engine;

namespace {

// 可计数、可注入失败的假字节源。
class CountingBytesSource : public ITileBytesSource {
public:
    explicit CountingBytesSource(bool fail = false) : fail_(fail) {}

    std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey&,
                                                         const std::string& url) const override {
        ++calls_;
        lastUrl_ = url;
        if (fail_) {
            return std::nullopt;
        }
        std::vector<uint8_t> body;
        for (char ch : url) {
            body.push_back(static_cast<uint8_t>(ch));
        }
        return body;
    }

    mutable int calls_ = 0;
    mutable std::string lastUrl_;
    bool fail_ = false;
};

const TileKey kKey(9, 200, 100);

} // namespace

TEST(TileCacheBytesSource, FirstMissFetchesInnerOnce) {
    CountingBytesSource inner;
    const TileCacheBytesSource cache(inner);
    const auto body = cache.requestTileBytes(kKey, "http://t/9/200/100.png");
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(inner.calls_, 1);
    EXPECT_EQ(cache.missCount(), 1u);
    EXPECT_EQ(cache.hitCount(), 0u);
    EXPECT_EQ(cache.entryCount(), 1u);
}

TEST(TileCacheBytesSource, SecondHitSkipsInner) {
    CountingBytesSource inner;
    const TileCacheBytesSource cache(inner);
    const std::string url = "http://t/9/200/100.png";
    const auto first = cache.requestTileBytes(kKey, url);
    ASSERT_TRUE(first.has_value());
    const auto second = cache.requestTileBytes(kKey, url);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(inner.calls_, 1); // 二次命中不调内层
    EXPECT_EQ(cache.hitCount(), 1u);
    EXPECT_EQ(*first, *second); // 内容一致（副本）
}

TEST(TileCacheBytesSource, EvictsOldestAtCapacity) {
    CountingBytesSource inner;
    const TileCacheBytesSource cache(inner, /*maxEntries=*/1);
    const auto a = cache.requestTileBytes(kKey, "http://t/a.png");
    const auto b = cache.requestTileBytes(kKey, "http://t/b.png");
    ASSERT_TRUE(a && b);
    EXPECT_EQ(cache.entryCount(), 1u);
    // b 命中了缓存；a 已被淘汰 → 再取 a 应重调内层（calls_ 从 2 到 3）。
    const auto a2 = cache.requestTileBytes(kKey, "http://t/a.png");
    ASSERT_TRUE(a2.has_value());
    EXPECT_EQ(inner.calls_, 3);
    EXPECT_EQ(*a, *a2); // FIFO 淘汰后可重建
}

TEST(TileCacheBytesSource, FailureIsNotCachedAndPassedThrough) {
    CountingBytesSource inner(/*fail=*/true);
    const TileCacheBytesSource cache(inner);
    const std::string url = "http://t/fail.png";
    EXPECT_FALSE(cache.requestTileBytes(kKey, url).has_value());
    EXPECT_EQ(inner.calls_, 1);
    EXPECT_EQ(cache.entryCount(), 0u); // 失败不入缓存
    EXPECT_EQ(cache.missCount(), 1u);
    EXPECT_EQ(cache.hitCount(), 0u);
    // 失败后源恢复 → 下次可取（不被失败毒化）。
    inner.fail_ = false;
    EXPECT_TRUE(cache.requestTileBytes(kKey, url).has_value());
    EXPECT_EQ(inner.calls_, 2);
    EXPECT_EQ(cache.entryCount(), 1u);
}
