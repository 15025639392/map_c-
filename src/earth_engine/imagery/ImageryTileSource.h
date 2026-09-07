#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "ImageryTileAvailability.h"
#include "earth_engine/providers/ITileBytesSource.h"
#include "earth_engine/renderer/IRenderDevice.h"
#include "earth_engine/tiling/TileKey.h"

namespace earth_engine {

/// 影像瓦源（S4 装配）：把 退化决议 × 字节源 × PNG→纹理数据 组合成一条可测链路。
///
/// fetchTexture(request)：沿父链决议（ImageryTileResolution）→ 以**决议键**从字节源
/// 取瓦（URL 模板 {z}/{x}/{y} 用决议键格式化）→ PngToRgba8 解码为 RGBA8 纹理数据。
///
/// 语义：
/// - 请求瓦自身可用 → 取自身；否则取最近可用祖先（影像北极星退化链）——取到的
///   一定是**真实数据**（对应决议键），全链不可用 → nullopt（明确空）；
/// - 返回 {实际键, 纹理数据}，调用方据实际键管理生命周期/去重；
/// - hasData：源可用性谓词（真影像源常按 zoom 范围或 availability 表）；
/// - 字节源/缓存：外部组合（如包一层 TileCacheBytesSource 去重）。
class ImageryTileSource {
public:
    struct Result {
        TileKey resolvedKey;
        render::Texture2DData texture;
    };

    /// @param keepAlpha true 时按 RGBA 解码保留 alpha（透明叠加层如路网注记）。
    ImageryTileSource(const ITileBytesSource& bytesSource, std::string urlTemplate,
                      std::function<bool(const TileKey&)> hasData, bool keepAlpha = false);

    std::optional<Result> fetchTexture(const TileKey& request) const;

private:
    const ITileBytesSource& bytesSource_;
    std::string urlTemplate_;
    std::function<bool(const TileKey&)> hasData_;
    bool keepAlpha_ = false;
};

} // namespace earth_engine
