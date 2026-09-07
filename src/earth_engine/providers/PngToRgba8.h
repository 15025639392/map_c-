#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "earth_engine/renderer/IRenderDevice.h"

namespace earth_engine {

/// PNG 瓦片字节 → RGBA8 纹理数据（S4 影像瓦→GPU 纹理的 host 数据腿）。
///
/// 语义：
/// - 输入为 PNG 编码的 RGB/RGBA 瓦片字节（如真实影像瓦；本仓地形源字节同形态）；
/// - 输出 render::Texture2DData（RGBA8，alpha=255；可直接 createTexture2D 上传）；
/// - 解码失败/非 PNG/尺寸超上限（默认 ≤4096²）→ nullopt（不吞不产脏数据）；
/// - 行序：stb 输出首行 = 图像顶行（影像瓦通常顶=北；UV 映射时按需翻转 v）。
class PngToRgba8 {
public:
    /// @param maxDimensionPx 单边尺寸上限（防超大瓦打爆内存；0=默认 4096）。
    static std::optional<render::Texture2DData> decode(const uint8_t* pngBytes, size_t size,
                                                       int maxDimensionPx = 4096);
};

} // namespace earth_engine
