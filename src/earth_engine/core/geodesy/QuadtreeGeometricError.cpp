#include "earth_engine/core/geodesy/QuadtreeGeometricError.h"

namespace earth_engine {
namespace QuadtreeGeometricError {

double screenSpaceError(double geometricErrorMeters, double distanceMeters,
                        double viewportHeightPx, double fovRadians) {
    if (viewportHeightPx <= 0.0) {
        return 0.0;
    }
    if (geometricErrorMeters < 0.0 || distanceMeters <= 0.0 || fovRadians <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    if (geometricErrorMeters == 0.0) {
        return 0.0;
    }
    const double metersPerPixel =
        2.0 * distanceMeters * std::tan(fovRadians * 0.5) / viewportHeightPx;
    if (!(metersPerPixel > 0.0)) {
        return std::numeric_limits<double>::infinity();
    }
    return geometricErrorMeters / metersPerPixel;
}

} // namespace QuadtreeGeometricError
} // namespace earth_engine
