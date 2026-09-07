#pragma once

#include <cstddef>
#include <cstdint>

namespace earth_engine {

/// 高度图像素编解码（纯函数，无图像 IO——IO 由 Provider 层负责）。
/// 本仓采用与 gis-md 相同的行序约定：**缓冲首行 = 瓦片北边**（与 XYZ 顶行原点一致）。
///
/// 两种主流 Web 高度编码（均为 PNG RGB 8bit）：
/// - **Terrain-RGB（Mapbox）**：h = -10000 + (r·65536 + g·256 + b) · 0.1（米）。
///   范围 [-10000, +1667721.5] m，量化步长 0.1 m。NASA/大量 XYZ 高程源用它。
/// - **Terrarium（Mapzen）**：h = (r·256 + g + b/256) - 32768（米）。
///   范围 [-32768, +32767.996] m，量化步长 ~0.0039 m。OpenTopoData 等用它。
class HeightmapCodec {
public:
    /// Terrain-RGB 的 nodata 底值（米）：RGB(0,0,0) 的解码结果。数据空洞与
    /// 缺邻居的重叠环常编成它。解码栅格把它注册进 noData 哨兵后，min/max 与
    /// 采样须排除，否则 -10000 会被当合法高度混进边缘双线性，造成 km 级假深沟
    /// 与假悬崖法线（瓦界光照条带）。
    /// **哨兵单一来源**：镜像 gis-md `HeightmapTerrainProvider::
    /// kTerrainRgbNoDataFloorMeters` 的纪律——注册点与下游判据共用此常量，
    /// 勿各自写字面量。
    static constexpr double kTerrainRgbNoDataFloorMeters = -10000.0;

    // ---- 单像素 ----
    static double decodeTerrainRgbPixel(uint8_t r, uint8_t g, uint8_t b);
    static double decodeTerrariumPixel(uint8_t r, uint8_t g, uint8_t b);

    /// Terrain-RGB 编码（0.1 m 量化取整；越界钳制）。r/g/b 输出。
    static void encodeTerrainRgbPixel(double heightMeters, uint8_t& outR, uint8_t& outG,
                                      uint8_t& outB);

    // ---- 整张缓冲 ----
    /// 解码 Terrain-RGB 行缓冲（像素 RGB 打包，PNG 解码后需先做颜色平面转 RGB）。
    /// pixels: width*height 像素、每行 strideBytes 字节（>= width*3，行尾可带 padding）。
    /// outHeights: width*height 双精度高度（米），行序 = 图像行序（首行=北）。
    /// 返回 false = 参数非法（pixels/outHeights 为空、stride < width*3 等）。
    static bool decodeTerrainRgb(const uint8_t* pixels, size_t width, size_t height,
                                 size_t strideBytes, double* outHeights);

    /// Terrarium 整张行解码（同 decodeTerrainRgb 的布局约定）。
    static bool decodeTerrarium(const uint8_t* pixels, size_t width, size_t height,
                                size_t strideBytes, double* outHeights);
};

} // namespace earth_engine
