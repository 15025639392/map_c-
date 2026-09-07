// DiskTileCacheBytesSource：磁盘瓦片缓存（S2 冷启层）——落盘 URL 哈希文件，
// 命中免网络；写盘失败降级透传；无淘汰（目录归调用方清理）。
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

#include "earth_engine/providers/DiskTileCacheBytesSource.h"
#include "earth_engine/providers/ITileBytesSource.h"
#include "earth_engine/tiling/TileKey.h"

using namespace earth_engine;

namespace {

// 计数假源（可注入失败）。
class CountingBytesSource : public ITileBytesSource {
public:
    explicit CountingBytesSource(bool fail = false) : fail_(fail) {}
    std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey&,
                                                         const std::string& url) const override {
        ++calls_;
        if (fail_) {
            return std::nullopt;
        }
        return std::vector<uint8_t>(url.begin(), url.end());
    }
    mutable int calls_ = 0;
    bool fail_ = false;
};

const TileKey kKey(9, 200, 100);

// 唯一测试目录（build/ctest 运行目录下）。
std::string makeTempCacheDir() {
    const std::string dir = "./.cache_test_disk_tile_" +
                            std::to_string(static_cast<unsigned long>(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace

TEST(DiskTileCache, FirstMissWritesAndSecondHitSkipsInner) {
    const std::string dir = makeTempCacheDir();
    CountingBytesSource inner;
    const DiskTileCacheBytesSource cache(inner, dir);
    const std::string url = "http://t/9/200/100.png";
    const auto first = cache.requestTileBytes(kKey, url);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(inner.calls_, 1);
    EXPECT_EQ(cache.diskWriteCount(), 1u);
    // 文件确实落盘。
    EXPECT_TRUE(std::filesystem::exists(cache.cachePathForUrl(url)));

    // 新实例（模拟冷启）命中磁盘，不调内层。
    CountingBytesSource inner2;
    const DiskTileCacheBytesSource cache2(inner2, dir);
    const auto second = cache2.requestTileBytes(kKey, url);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(inner2.calls_, 0); // 纯磁盘命中
    EXPECT_EQ(cache2.diskHitCount(), 1u);
    EXPECT_EQ(*first, *second);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(DiskTileCache, FailureIsNotCachedAndPassedThrough) {
    const std::string dir = makeTempCacheDir();
    CountingBytesSource inner(/*fail=*/true);
    const DiskTileCacheBytesSource cache(inner, dir);
    const std::string url = "http://t/fail.png";
    EXPECT_FALSE(cache.requestTileBytes(kKey, url).has_value());
    EXPECT_EQ(cache.passthroughCount(), 1u);
    EXPECT_EQ(cache.diskWriteCount(), 0u);
    EXPECT_FALSE(std::filesystem::exists(cache.cachePathForUrl(url)));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(DiskTileCache, UnwritableDirectoryDegradesToPassthrough) {
    // 缓存目录不可写（指到文件而非目录）→ 写失败也应正常返回数据（缓存不拖累取数）。
    const std::string bogus = "./.cache_test_disk_tile_bogus_file";
    {
        std::ofstream touch(bogus, std::ios::trunc);
        touch << "x";
    }
    CountingBytesSource inner;
    const DiskTileCacheBytesSource cache(inner, bogus); // 目录参数实为文件
    const auto body = cache.requestTileBytes(kKey, "http://t/a.png");
    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(inner.calls_, 1);
    EXPECT_EQ(cache.diskWriteCount(), 0u);
    std::error_code ec;
    std::filesystem::remove_all(bogus, ec);
}
