#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ITileBytesSource.h"

namespace earth_engine {

/// 瓦片字节缓存（装饰 ITileBytesSource）：以 URL 为键缓存最近取回的瓦片字节，
/// 命中免网络/解码前重复 IO。S2 资源调度第一步——真实 NASA 514 源 M-coarse
/// 首帧 323 瓦 ≈40s 的直击点：缓存后重复机位/换代不重复下载。
///
/// 语义：
/// - 键 = URL（与 provider 层"一瓦一 URL"对应，键含 {z}/{x}/{y} 已格式化）；
/// - 命中返回字节副本并计 miss 不增；未命中调内层，成功则入缓存并计 miss；
/// - 内层失败（nullopt）**不入缓存**、原样透传（失败不可缓存，防毒体/瞬时错误固化）；
/// - 容量 = maxEntries 瓦；满时淘汰最早插入的一条（FIFO，简单先行）；
/// - 非线程安全（本仓当前同步单线程调用；接入并发/异步前需加锁），头注释声明。
/// - 计数（hit/miss/entry）供性能记账与 host 断言。
class TileCacheBytesSource final : public ITileBytesSource {
public:
    explicit TileCacheBytesSource(const ITileBytesSource& inner, size_t maxEntries = 64);

    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey& key, const std::string& url) const override;

    uint64_t hitCount() const { return hitCount_; }
    uint64_t missCount() const { return missCount_; }
    size_t entryCount() const { return entries_.size(); }

private:
    const ITileBytesSource& inner_;
    size_t maxEntries_;
    // FIFO 插入序（值 = 条目计数键）与条目表。
    mutable std::vector<std::string> order_;
    mutable std::unordered_map<std::string, std::vector<uint8_t>> entries_;
    mutable uint64_t hitCount_ = 0;
    mutable uint64_t missCount_ = 0;
};

} // namespace earth_engine
