#pragma once

#include <cstddef>

namespace earth_engine {

/// 规则网格高度采样器（内容模块）。
/// 数据布局：heights[row * width + col]，**row 0 = 北（顶行）**，与高度图缓冲一致。
/// 采样坐标 (col, row) 以像素网格为单位；小数坐标 = 双线性插值。
/// 越界处理 = CLAMP_TO_EDGE（贴边不外推）——跨瓦采样前由上层保证只在本瓦内查询。
class HeightmapSampler {
public:
    /// 只持有指针，不拥有数据；heights 生命周期须覆盖本对象。
    HeightmapSampler(const double* heights, int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    /// 最近邻采样（坐标四舍五入后钳制）。
    double sampleNearest(double col, double row) const;

    /// 双线性采样：像素中心对齐格点 (col,row)=(i,j) 处为 heights[j*w+i]。
    /// 边缘/角落在采样窗越过边界时钳到边界像素（等价于纹理 CLAMP_TO_EDGE）。
    double sampleBilinear(double col, double row) const;

    /// 坐标钳制辅助（[0, maxIndex]）。
    static double clampToIndex(double v, int maxIndex);

private:
    const double* heights_;
    int width_;
    int height_;
};

} // namespace earth_engine
