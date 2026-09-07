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
/// 可选携带本瓦 no-data 哨兵表（TerrainGrid::noDataValues，默认空）：
/// min/max 与采样把哨兵当"无数据"排除（采样仅有效角归一化），
/// 防止 -10000 类底值污染包围体或把边缘混出假深沟——并入 gis-md 解码语义（B1）。
class HeightmapTile {
public:
    HeightmapTile(const WebMercatorTileScheme& scheme, const TileKey& key,
                  const double* heights, int width, int height,
                  const double* noDataValues = nullptr, int noDataCount = 0);

    const TileKey& key() const { return key_; }
    int width() const { return width_; }
    int height() const { return height_; }
    /// 原始高度栅格（row-major，row0=北）；调用方管理生命周期。
    const double* heights() const { return heights_; }
    /// 本瓦 no-data 哨兵表（可能为 nullptr / count 0 = 无哨兵）。
    const double* noDataValues() const { return noDataValues_; }
    int noDataCount() const { return noDataCount_; }

    /// 瓦片覆盖的经纬矩形（弧度；由 scheme 提供）。
    Rectangle coverageRadians() const;

    /// 瓦片是否覆盖该经纬点（mercator 米边界比较，含边）。
    bool contains(const Cartographic& cartographic) const;

    /// 经纬 → 像素坐标（网格单位，可为小数；越出瓦片返回 nullopt）。
    std::optional<Vec2> cartographicToPixel(const Cartographic& cartographic) const;

    /// 像素坐标 → 经纬（网格单位；col∈[0,w-1] 映射西→东，row∈[0,h-1] 映射北→南；
    /// 越界按贴边处理）。
    Cartographic pixelToCartographic(double col, double row) const;

    /// 经纬查高（米；越出瓦片返回 nullopt）。携带哨兵表时哨兵角被排除
    /// （仅有效角归一化；四角全哨兵 → 回传哨兵值）。
    std::optional<double> sampleHeightAt(const Cartographic& cartographic) const;

    /// 网格最小/最大高度。携带哨兵表时**排除 no-data 样本**（全哨兵 → {0,0}，
    /// 镜像 gis-md assignHeights）；无哨兵时全量扫描（结果不变）。
    std::pair<double, double> minMaxHeight() const;

private:
    /// 该高度是否为哨兵 no-data（>50000 或精确命中哨兵表；仅哨兵路径使用）。
    bool isNoDataValue(double height) const;

    const WebMercatorTileScheme* scheme_;
    TileKey key_;
    const double* heights_;
    int width_;
    int height_;
    const double* noDataValues_ = nullptr;
    int noDataCount_ = 0;
};

} // namespace earth_engine
