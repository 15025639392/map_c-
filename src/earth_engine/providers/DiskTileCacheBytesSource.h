#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ITileBytesSource.h"

namespace earth_engine {

/// 磁盘瓦片字节缓存（装饰 ITileBytesSource）：把取回的瓦片字节落盘（缓存目录下
/// 以 URL 哈希命名），命中免网络——S2 的二次启动/冷启层（配合内存缓存
/// TileCacheBytesSource 使用：先内存后磁盘）。
///
/// 语义：
/// - 文件 = cacheDirectory/<sha-like hex of url>.bin（URL 哈希做文件名，不做目录级联）；
/// - 命中磁盘 → 返回并计 diskHit；未命中 → 调内层，成功则写盘（计 diskWrite）再返回，
///   失败（nullopt）不写盘、原样透传；
/// - 无淘汰（目录归属调用方/集成层清理；本类只负责读写），头注释声明；
/// - 非线程安全（同步单线程用；接入并发前加锁）；写盘失败（IO 异常/只读目录）
///   降级为纯透传（不因缓存失败影响取数），不计假命中。
class DiskTileCacheBytesSource final : public ITileBytesSource {
public:
    DiskTileCacheBytesSource(const ITileBytesSource& inner, std::string cacheDirectory);

    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey& key, const std::string& url) const override;

    uint64_t diskHitCount() const { return diskHitCount_; }
    uint64_t diskWriteCount() const { return diskWriteCount_; }
    uint64_t passthroughCount() const { return passthroughCount_; }

    /// 该 URL 的缓存文件名（测试/诊断用）。
    std::string cachePathForUrl(const std::string& url) const;

private:
    const ITileBytesSource& inner_;
    std::string cacheDirectory_;
    mutable uint64_t diskHitCount_ = 0;
    mutable uint64_t diskWriteCount_ = 0;
    mutable uint64_t passthroughCount_ = 0;
};

} // namespace earth_engine
