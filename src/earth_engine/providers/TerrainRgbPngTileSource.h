#pragma once

#include <string>

#include "ITileBytesSource.h"
#include "StbPngDecoder.h"
#include "TileUrlFormatter.h"
#include "../content/HeightmapCodec.h"
#include "../content/TerrainDataSource.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// Terrain-RGB **PNG** 高度源：字节源 → PNG 解码 → Terrain-RGB 解码 → TerrainGrid。
/// 真实源（NASA Terrain-RGB 等）的瓦片字节就是 PNG——本类是 providers 栈的"真值"形态；
/// 纯 RGB 行变体见 TerrainRgbTileSource（供已解码管线/测试）。
///
/// **cell-registered + 1px 裙边环模式（Mapbox Terrain-RGB 514 标准，B2）**：
/// 请求 gridSize（cell 数，如 512）时，期望瓦片 PNG 为 (gridSize+2)²（512 cell +
/// 每侧 1px 邻瓦回填环），解码栅格 width/height = gridSize+2、borderInset = 0.5 →
/// 采样内缩后半像元 → 相邻瓦共享边读到同一批世界样本（SeamAudit ≈0）。
/// 应用实例：`https://mapoverlay.xinzhi.space/3dterrain/nasa/tiles/{z}/{x}/{y}.png`
/// （实测 514×514 Terrain-RGB，覆盖 z6–12；z13 404）。zoom 范围（默认不限制）越界
/// 请求返回 nullopt（源覆盖外不冒充数据；配合 AncestorFallbackDataSource 由近祖
/// 顶住）。
class TerrainRgbPngTileSource : public ITerrainDataSource {
public:
    /// @param cellRegisteredRing true = 514 环模式（期望 PNG (gridSize+2)²，输出
    ///   borderInset=0.5 的 ring 栅格）；false = 顶点栅格模式（期望 PNG gridSize²，
    ///   行为与旧版一致）。
    /// @param minZoom/maxZoom 源覆盖层级（含边界；默认不限制）。
    TerrainRgbPngTileSource(const ITileBytesSource& bytesSource, std::string urlTemplate,
                            bool cellRegisteredRing = false, int minZoom = 0,
                            int maxZoom = 30);

    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& /*scheme*/,
                                              const TileKey& key, int gridSize) const override;

private:
    const ITileBytesSource& bytesSource_;
    std::string urlTemplate_;
    bool cellRegisteredRing_ = false;
    int minZoom_ = 0;
    int maxZoom_ = 30;
};

} // namespace earth_engine
