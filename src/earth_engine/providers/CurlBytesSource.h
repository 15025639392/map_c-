#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ITileBytesSource.h"

namespace earth_engine {

/// HTTP(S) 瓦片字节源（libcurl）。HTTPS/重定向由 curl 处理；失败/非 200 → nullopt。
class CurlBytesSource : public ITileBytesSource {
public:
    /// timeoutMs：整请求超时（连接 + 传输）。
    explicit CurlBytesSource(long timeoutMs = 10000);

    std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey& key, const std::string& url) const override;

private:
    long timeoutMs_;
};

} // namespace earth_engine
