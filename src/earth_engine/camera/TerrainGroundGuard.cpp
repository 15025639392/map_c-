#include "earth_engine/camera/TerrainGroundGuard.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace earth_engine {

TerrainGroundGuard::TerrainGroundGuard(double minClearanceMeters)
    : minClearanceMeters_(std::max(0.0, minClearanceMeters)) {}

std::optional<GroundClearanceResult> TerrainGroundGuard::enforceClearance(
    double lonRad, double latRad, double heightMeters, GroundHeightFn ground) const {
    if (!std::isfinite(lonRad) || !std::isfinite(latRad) || !std::isfinite(heightMeters) ||
        ground == nullptr) {
        return std::nullopt; // 脏输入不吞不产
    }
    GroundClearanceResult result;
    const std::optional<double> groundH = ground(Cartographic(lonRad, latRad, 0.0));
    if (!groundH.has_value() || !std::isfinite(*groundH)) {
        result.heightMeters = heightMeters; // 无地表约束：不动（回落属渲染策略）
        result.clamped = false;
        return result;
    }
    const double floorMeters = *groundH + minClearanceMeters_;
    if (heightMeters < floorMeters) {
        result.heightMeters = floorMeters;
        result.clamped = true;
    } else {
        result.heightMeters = heightMeters;
        result.clamped = false;
    }
    return result;
}

} // namespace earth_engine
