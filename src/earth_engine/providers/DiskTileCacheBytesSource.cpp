#include "earth_engine/providers/DiskTileCacheBytesSource.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace earth_engine {

namespace {

// URL → 十六进制文件名（64bit 哈希 ×2 盐，足够测试/单机缓存区分）。
std::string urlToHex(const std::string& url) {
    const std::size_t h1 = std::hash<std::string>{}(url);
    const std::size_t h2 = std::hash<std::string>{}(url + "#mapc");
    std::ostringstream oss;
    oss << std::hex << h1 << h2;
    return oss.str();
}

bool readFileToBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool writeBytesToFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

} // namespace

DiskTileCacheBytesSource::DiskTileCacheBytesSource(const ITileBytesSource& inner,
                                                   std::string cacheDirectory)
    : inner_(inner), cacheDirectory_(std::move(cacheDirectory)) {}

std::string DiskTileCacheBytesSource::cachePathForUrl(const std::string& url) const {
    return cacheDirectory_ + "/" + urlToHex(url) + ".bin";
}

std::optional<std::vector<uint8_t>> DiskTileCacheBytesSource::requestTileBytes(
    const TileKey& key, const std::string& url) const {
    const std::string path = cachePathForUrl(url);
    std::vector<uint8_t> cached;
    if (readFileToBytes(path, cached)) {
        ++diskHitCount_;
        return cached;
    }
    std::optional<std::vector<uint8_t>> bytes = inner_.requestTileBytes(key, url);
    if (!bytes) {
        ++passthroughCount_;
        return std::nullopt;
    }
    if (writeBytesToFile(path, *bytes)) {
        ++diskWriteCount_;
    } else {
        // 写盘失败（如只读目录/IO）降级为透传：缓存不该影响取数正确性。
        ++passthroughCount_;
    }
    return bytes;
}

} // namespace earth_engine
