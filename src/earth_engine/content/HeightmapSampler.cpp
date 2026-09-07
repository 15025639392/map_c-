#include "earth_engine/content/HeightmapSampler.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

namespace {
// OpenGlobus RgbTerrain.checkNoDataValue 的规则：> 50000 一律视为 no-data
// （真实地形高度远达不到；只有解码垃圾/哨兵才有机会超）。仅哨兵路径生效。
constexpr double kNoDataHeightThreshold = 50000.0;
} // namespace

HeightmapSampler::HeightmapSampler(const double* heights, int width, int height,
                                   const double* noDataValues, int noDataCount)
    : heights_(heights),
      width_(width),
      height_(height),
      noDataValues_(noDataValues),
      noDataCount_(noDataCount) {}

double HeightmapSampler::clampToIndex(double v, int maxIndex) {
    return std::clamp(v, 0.0, static_cast<double>(maxIndex));
}

double HeightmapSampler::sampleNearest(double col, double row) const {
    const int c = std::clamp(static_cast<int>(std::llround(col)), 0, width_ - 1);
    const int r = std::clamp(static_cast<int>(std::llround(row)), 0, height_ - 1);
    return heights_[r * width_ + c];
}

bool HeightmapSampler::isNoData(double height) const {
    if (height > kNoDataHeightThreshold) {
        return true;
    }
    for (int i = 0; i < noDataCount_; ++i) {
        if (height == noDataValues_[i]) {
            return true;
        }
    }
    return false;
}

double HeightmapSampler::sampleBilinear(double col, double row) const {
    const double c = clampToIndex(col, width_ - 1);
    const double r = clampToIndex(row, height_ - 1);

    const int c0 = static_cast<int>(std::floor(c));
    const int r0 = static_cast<int>(std::floor(r));
    const int c1 = std::min(c0 + 1, width_ - 1);
    const int r1 = std::min(r0 + 1, height_ - 1);

    const double fx = c - static_cast<double>(c0);
    const double fy = r - static_cast<double>(r0);

    const double h00 = heights_[r0 * width_ + c0];
    const double h10 = heights_[r0 * width_ + c1];
    const double h01 = heights_[r1 * width_ + c0];
    const double h11 = heights_[r1 * width_ + c1];

    if (noDataCount_ <= 0) {
        // 无哨兵：保持既有双线性路径（结果逐位不变，普通用法零扰动）。
        const double top = h00 + (h10 - h00) * fx;
        const double bottom = h01 + (h11 - h01) * fx;
        return top + (bottom - top) * fy;
    }

    // 哨兵路径（镜像 gis-md DecodedHeightmap::sampleBilinearUnclamped）：
    // 原始混入会把哨兵（如 -10000）与有效角合成一个中值 —— 落在 isNoData
    // 判定之下、实际却非真实地形的假斜坡/尖刺。只对有效角加权并归一化；
    // 四角全哨兵 → 回传左上角值（= 哨兵，"该处无数据"信号原样上抛）。
    const double w00 = (1.0 - fx) * (1.0 - fy);
    const double w10 = fx * (1.0 - fy);
    const double w01 = (1.0 - fx) * fy;
    const double w11 = fx * fy;

    double weightedSum = 0.0;
    double weightTotal = 0.0;
    const auto accumulate = [&](double h, double w) {
        if (!isNoData(h)) {
            weightedSum += h * w;
            weightTotal += w;
        }
    };
    accumulate(h00, w00);
    accumulate(h10, w10);
    accumulate(h01, w01);
    accumulate(h11, w11);

    if (weightTotal <= 0.0) {
        return h00;
    }
    return weightedSum / weightTotal;
}

} // namespace earth_engine
