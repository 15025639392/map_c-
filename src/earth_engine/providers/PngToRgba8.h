#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "earth_engine/renderer/IRenderDevice.h"

namespace earth_engine {

/// PNG 瓦片字节 → RGBA8 纹理数据（S4 影像瓦→GPU 纹理的 host 数据腿）。
///
/// 语义：
/// - 输入为 PNG/JPEG 编码的瓦片字节（stb 支持格式）；
/// - decode()：RGB 语义（alpha 恒 255）——普通影像瓦；
/// - decodeKeepAlpha()：保留源 alpha（PNG alpha 或 JPEG→255）——透明叠加层
///   （路网注记等）用；
/// - 输出 render::Texture2DData（RGBA8，可直接 createTexture2D 上传）；
/// - 解码失败/非图像/尺寸超上限（默认 ≤4096²）→ nullopt（不吞不产脏数据）；
/// - 行序：解码输出首行 = 图像顶行（影像瓦通常顶=北；UV 映射时按需翻转 v）。
class PngToRgba8 {
public:
    /// @param maxDimensionPx 单边尺寸上限（防超大瓦打爆内存；0=默认 4096）。
    static std::optional<render::Texture2DData> decode(const uint8_t* pngBytes, size_t size,
                                                       int maxDimensionPx = 4096);
    /// 保留 alpha（透明层用）。
    static std::optional<render::Texture2DData> decodeKeepAlpha(const uint8_t* bytes,
                                                                size_t size,
                                                                int maxDimensionPx = 4096);
};

} // namespace earth_engine
