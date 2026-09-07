#include "earth_engine/core/geodesy/HeightDatumCorrector.h"

#include <algorithm>
#include <cmath>

#include "earth_engine/core/math/MathUtils.h"

namespace earth_engine {

GridHeightDatumCorrector::GridHeightDatumCorrector(double westDeg, double southDeg,
                                                   double cellDegrees, int lonCount,
                                                   int latCount, const double* gridMeters)
    : westDeg_(westDeg),
      southDeg_(southDeg),
      cellDegrees_(cellDegrees),
      lonCount_(lonCount),
      latCount_(latCount) {
    if (gridMeters != nullptr && lonCount_ > 0 && latCount_ > 0 && cellDegrees_ > 0.0) {
        values_.assign(gridMeters, gridMeters + static_cast<size_t>(lonCount_) * latCount_);
    }
}

double GridHeightDatumCorrector::undulationMeters(const Cartographic& cartographic) const {
    if (empty()) {
        return 0.0;
    }
    const double lonDeg = radiansToDegrees(cartographic.longitude());
    const double latDeg = radiansToDegrees(cartographic.latitude());
    const double eastDeg = westDeg_ + (lonCount_ - 1) * cellDegrees_;
    const double northDeg = southDeg_ + (latCount_ - 1) * cellDegrees_;

    // 网格外：钳制到边（不做外推）。
    const double lon = std::clamp(lonDeg, westDeg_, eastDeg);
    const double lat = std::clamp(latDeg, southDeg_, northDeg);

    const double fx = (lon - westDeg_) / cellDegrees_;
    const double fy = (lat - southDeg_) / cellDegrees_;
    const int i0 = static_cast<int>(std::floor(fx));
    const int j0 = static_cast<int>(std::floor(fy));
    const int i1 = std::min(i0 + 1, lonCount_ - 1);
    const int j1 = std::min(j0 + 1, latCount_ - 1);
    const double tx = fx - static_cast<double>(i0);
    const double ty = fy - static_cast<double>(j0);

    const auto at = [this](int i, int j) {
        return values_[static_cast<size_t>(j) * lonCount_ + static_cast<size_t>(i)];
    };
    const double top = at(i0, j0) * (1.0 - tx) + at(i1, j0) * tx;
    const double bottom = at(i0, j1) * (1.0 - tx) + at(i1, j1) * tx;
    return top * (1.0 - ty) + bottom * ty;
}

} // namespace earth_engine
