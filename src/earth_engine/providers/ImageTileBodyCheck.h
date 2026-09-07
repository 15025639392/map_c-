#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace earth_engine {

/// 瓦片响应体魔数白名单（PNG / JPEG / WebP）。
///
/// 语义转写自 gis-md `providers/ImageTileBodyCheck.h`（commit bf25c639）——网络硬化
/// 差值表项：CDN/OSS 边缘节点实测会间歇返回 **HTTP 200 + NoSuchKey XML 错误体**
/// （过期负缓存）；本仓当前无 HttpCache（接真实网络源/缓存时该检查防止毒体入链），
/// 现先把检查落在地形链路的**网络字节入口**（TerrainRgbPngTileSource），把垃圾体
/// 挡在 PNG 解码之前。
///
/// 名单外合法图格式会被拒（接入新格式数据源前需先扩这里）。
inline bool looksLikeImageTileBody(const uint8_t* data, size_t size) {
    // 12 字节下限同时是 WebP 头（RIFF????WEBP）的最短读取长度；真实图像瓦片不会短于它。
    if (data == nullptr || size < 12) {
        return false;
    }
    static constexpr uint8_t kPng[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (std::memcmp(data, kPng, sizeof(kPng)) == 0) {
        return true;
    }
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return true; // JPEG SOI 前缀
    }
    if (std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0) {
        return true;
    }
    return false;
}

inline bool looksLikeImageTileBody(const std::vector<uint8_t>& body) {
    return looksLikeImageTileBody(body.data(), body.size());
}

} // namespace earth_engine
