#include "earth_engine/providers/TileCacheBytesSource.h"

namespace earth_engine {

TileCacheBytesSource::TileCacheBytesSource(const ITileBytesSource& inner, size_t maxEntries)
    : inner_(inner), maxEntries_(maxEntries == 0 ? 1 : maxEntries) {}

std::optional<std::vector<uint8_t>> TileCacheBytesSource::requestTileBytes(
    const TileKey& key, const std::string& url) const {
    const auto it = entries_.find(url);
    if (it != entries_.end()) {
        ++hitCount_;
        return it->second; // 副本（vector 拷贝）
    }
    ++missCount_;
    std::optional<std::vector<uint8_t>> bytes = inner_.requestTileBytes(key, url);
    if (!bytes) {
        return std::nullopt; // 失败不入缓存
    }
    if (entries_.size() >= maxEntries_ && !entries_.empty()) {
        // FIFO 淘汰最早插入条目。
        const std::string oldest = order_.front();
        order_.erase(order_.begin());
        entries_.erase(oldest);
    }
    entries_.emplace(url, *bytes);
    order_.push_back(url);
    return bytes;
}

} // namespace earth_engine
