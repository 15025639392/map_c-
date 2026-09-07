#include "earth_engine/content/HeightmapSampler.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

HeightmapSampler::HeightmapSampler(const double* heights, int width, int height)
    : heights_(heights), width_(width), height_(height) {}

double HeightmapSampler::clampToIndex(double v, int maxIndex) {
    return std::clamp(v, 0.0, static_cast<double>(maxIndex));
}

double HeightmapSampler::sampleNearest(double col, double row) const {
    const int c = std::clamp(static_cast<int>(std::llround(col)), 0, width_ - 1);
    const int r = std::clamp(static_cast<int>(std::llround(row)), 0, height_ - 1);
    return heights_[r * width_ + c];
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

    const double top = h00 + (h10 - h00) * fx;
    const double bottom = h01 + (h11 - h01) * fx;
    return top + (bottom - top) * fy;
}

} // namespace earth_engine
