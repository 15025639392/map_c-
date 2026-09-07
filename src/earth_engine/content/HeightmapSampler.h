#pragma once

#include <cstddef>

namespace earth_engine {

/// 规则网格高度采样器（内容模块）。
/// 数据布局：heights[row * width + col]，**row 0 = 北（顶行）**，与高度图缓冲一致。
/// 采样坐标 (col, row) 以像素网格为单位；小数坐标 = 双线性插值。
/// 越界处理 = CLAMP_TO_EDGE（贴边不外推）——跨瓦采样前由上层保证只在本瓦内查询。
///
/// **no-data 哨兵（并入 gis-md 解码语义，B1）**：可选携带本瓦哨兵表
/// （TerrainGrid::noDataValues）。提供时双线性对哨兵角做**仅有效角加权再归一化**
/// （镜像 gis-md DecodedHeightmap::sampleBilinearUnclamped），四角全哨兵时回传哨兵值；
/// 不提供（空）时保持纯双线性路径（行为逐位不变）——哨兵语义只属于已注册的
/// 解码栅格，不污染普通用法。
class HeightmapSampler {
public:
    /// 只持有指针，不拥有数据；heights/noDataValues 生命周期须覆盖本对象。
    /// @param noDataValues 可选 no-data 哨兵表（可为 nullptr，配合 count=0）。
    HeightmapSampler(const double* heights, int width, int height,
                     const double* noDataValues = nullptr, int noDataCount = 0);

    int width() const { return width_; }
    int height() const { return height_; }
    /// 是否携带哨兵表。
    bool hasNoData() const { return noDataCount_ > 0; }

    /// 最近邻采样（坐标四舍五入后钳制）。
    double sampleNearest(double col, double row) const;

    /// 双线性采样：像素中心对齐格点 (col,row)=(i,j) 处为 heights[j*w+i]。
    /// 边缘/角落在采样窗越过边界时钳到边界像素（等价于纹理 CLAMP_TO_EDGE）。
    /// 携带哨兵表时：只对有效（非哨兵）角加权并归一化；四角全哨兵 → 回传
    /// 左上角值（= 哨兵，调用方/上层据此判"该处无数据"）。
    double sampleBilinear(double col, double row) const;

    /// 坐标钳制辅助（[0, maxIndex]）。
    static double clampToIndex(double v, int maxIndex);

    /// 该高度是否为本采样器哨兵表中的 no-data（`> 50000` 或精确命中哨兵值；
    /// 镜像 gis-md isNoData 规则；仅当携带哨兵表时有意义）。
    bool isNoData(double height) const;

private:
    const double* heights_;
    int width_;
    int height_;
    const double* noDataValues_ = nullptr;
    int noDataCount_ = 0;
};

} // namespace earth_engine
