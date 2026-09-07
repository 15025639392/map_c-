#pragma once

#include <optional>
#include <utility>

#include "HeightmapSampler.h"
#include "../core/geodesy/Cartographic.h"
#include "../core/math/Rectangle.h"
#include "../core/math/Vec2.h"
#include "../tiling/TileKey.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// 一块已解码高度图的内容封装（"瓦片 → 内容" 的装配产物）。
///
/// 语义（与 gis-md 查高服务一致的地基）：
/// - 网格点落在**瓦片边界**上：col 0/col w-1 = 西/东边，row 0/row h-1 = 北/南边
///   （DEM post 采样惯例，65×65 高度图贴 64×64 瓦片）；
/// - 行序：row 0 = 北（与 HeightmapCodec / XYZ 顶行一致）；
/// - 采样映射走 **Web Mercator 米**：col 在 mercator x 线性、row 在 mercator y 线性
///   （高纬不按经纬度线性，直接 lon/lat 双线性会错——T-P11 教训）。
/// 本对象只持有数据指针（由调用方/缓存层管理生命周期），不拷贝高度。
class HeightmapTile {
public:
    HeightmapTile(const WebMercatorTileScheme& scheme, const TileKey& key,
                  const double* heights, int width, int height);

    const TileKey& key() const { return key_; }
    int width() const { return width_; }
    int height() const { return height_; }

    /// 瓦片覆盖的经纬矩形（弧度；由 scheme 提供）。
    Rectangle coverageRadians() const;

    /// 瓦片是否覆盖该经纬点（mercator 米边界比较，含边）。
    bool contains(const Cartographic& cartographic) const;

    /// 经纬 → 像素坐标（网格单位，可为小数；越出瓦片返回 nullopt）。
    std::optional<Vec2> cartographicToPixel(const Cartographic& cartographic) const;

    /// 像素坐标 → 经纬（网格单位；col∈[0,w-1] 映射西→东，row∈[0,h-1] 映射北→南；
    /// 越界按贴边处理）。
    Cartographic pixelToCartographic(double col, double row) const;

    /// 经纬查高（米；越出瓦片返回 nullopt）。
    std::optional<double> sampleHeightAt(const Cartographic& cartographic) const;

    /// 网格最小/最大高度。
    std::pair<double, double> minMaxHeight() const;

private:
    const WebMercatorTileScheme* scheme_;
    TileKey key_;
    const double* heights_;
    int width_;
    int height_;
};

} // namespace earth_engine
