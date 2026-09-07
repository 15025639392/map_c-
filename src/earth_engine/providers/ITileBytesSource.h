#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../tiling/TileKey.h"

namespace earth_engine {

/// 瓦片字节源抽象：给键 + URL 模板，返回瓦片原始字节。
/// 语义与实现无关——将来 HTTP/curl 实现只需满足该接口；
/// 当前 host 测试用内存 fixture 实现（离线全绿）。
class ITileBytesSource {
public:
    virtual ~ITileBytesSource() = default;

    /// 请求瓦片字节；不可用返回 nullopt。
    virtual std::optional<std::vector<uint8_t>> requestTileBytes(
        const TileKey& key, const std::string& urlTemplate) const = 0;
};

} // namespace earth_engine
