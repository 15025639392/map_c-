#include "earth_engine/content/TerrainPicking.h"

#include <cmath>
#include <limits>

#include "earth_engine/core/math/RayTriangle.h"

namespace earth_engine {

std::optional<TerrainPickHit> pickTerrainFrame(
    const Vec3& origin, const Vec3& direction,
    const std::vector<TerrainFrameAssembler::Frame>& frames) {
    double bestT = std::numeric_limits<double>::infinity();
    TerrainPickHit best;
    for (size_t fi = 0; fi < frames.size(); ++fi) {
        const TerrainFrameAssembler::Frame& frame = frames[fi];
        const std::vector<Vec3>& positions = frame.mesh.positionsEcef;
        const std::vector<uint32_t>& indices = frame.mesh.indices;
        for (size_t tIdx = 0; tIdx + 2 < indices.size(); tIdx += 3) {
            const Vec3& v0 = positions[indices[tIdx]];
            const Vec3& v1 = positions[indices[tIdx + 1]];
            const Vec3& v2 = positions[indices[tIdx + 2]];
            double t = 0.0;
            double u = 0.0;
            double v = 0.0;
            if (!rayTriangleIntersection(origin, direction, v0, v1, v2, t, u, v)) {
                continue;
            }
            if (t < bestT) {
                bestT = t;
                best.hit = true;
                best.t = t;
                best.point = origin + direction * t;
                best.key = frame.key;
                best.frameIndex = fi;
                best.faceNormal = (v1 - v0).cross(v2 - v0).normalized();
                if (best.faceNormal.dot(best.point) < 0.0 && best.faceNormal.magnitudeSquared() > 0.0) {
                    best.faceNormal = -best.faceNormal; // 保证外向（对地心朝外）
                }
            }
        }
    }
    if (!best.hit) {
        return std::nullopt;
    }
    return best;
}

} // namespace earth_engine
