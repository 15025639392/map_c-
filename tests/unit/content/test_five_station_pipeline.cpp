#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/TerrainCameraPipeline.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/core/geodesy/Transforms.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 900.0 + 1200.0 * std::sin(c.longitude() * 55.0) * std::cos(c.latitude() * 42.0) +
           400.0 * std::sin(c.longitude() * 220.0);
}

class FunctionalTerrainSource : public ITerrainDataSource {
public:
    explicit FunctionalTerrainSource(std::function<double(const Cartographic&)> fn)
        : fn_(std::move(fn)) {}
    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key, int gridSize) const override {
        TerrainGrid grid;
        grid.width = gridSize;
        grid.height = gridSize;
        grid.heights.resize(static_cast<size_t>(gridSize * gridSize));
        std::vector<double> zeros(static_cast<size_t>(gridSize * gridSize), 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), gridSize, gridSize);
        for (int row = 0; row < gridSize; ++row) {
            for (int col = 0; col < gridSize; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                grid.heights[static_cast<size_t>(row * gridSize + col)] = fn_(c);
            }
        }
        return grid;
    }

private:
    std::function<double(const Cartographic&)> fn_;
};

/// 按判据文档机位参数构造相机（与 demo 同语义：pitch=相对地平线向下角，hdg 0=北）。
CameraView stationCamera(double lonDeg, double latDeg, double altMeters, double pitchDeg,
                         double headingDeg) {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(lonDeg, latDeg, 0.0);
    const Vec3 pos = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), altMeters));
    const double hdg = headingDeg * kRadiansPerDegree;
    const double pit = pitchDeg * kRadiansPerDegree;
    const Vec3 dirEnu(std::sin(hdg) * std::cos(pit), std::cos(hdg) * std::cos(pit),
                      -std::sin(pit));
    const Mat4 enu =
        Transforms::eastNorthUpToFixedFrame(center, e);
    const Vec3 target = enu.transformPoint(dirEnu * 200000.0);
    const Vec3 up = e.geodeticSurfaceNormal(center);
    return CameraView(pos, target, up, degreesToRadians(60.0), 1.0);
}

} // namespace

// 判据固定机位 host 回归：五机位（合成源）都必须能出帧。
// 模拟器真 DEM 版截图见 docs/assets/station1..5.png；本测试守卫"机位可出帧"。
TEST(FiveStationPipeline, AllStationsAssembleFrames) {
    struct Station {
        const char* name;
        double altMeters;
        double pitchDeg; // 视线相对地平线向下
    };
    const Station stations[] = {
        {"M-near", 3000.0, 60.0},
        {"M-mid", 15000.0, 45.0},
        {"M-graze", 8000.0, 10.0},
        {"M-high", 60000.0, 30.0},
        {"M-coarse", 250000.0, 20.0},
    };
    const WebMercatorTileScheme scheme;
    const FunctionalTerrainSource source(hillFn);
    const Ellipsoid& e = Ellipsoid::WGS84();

    for (const Station& s : stations) {
        const CameraView camera = stationCamera(106.44, 29.70, s.altMeters, s.pitchDeg, 20.0);
        TerrainCameraPipelineConfig config;
        config.lod.maxScreenSpaceErrorPx = s.altMeters >= 200000.0 ? 12.0 : 4.0;
        config.lod.geometricErrorScale = 0.001;
        config.lod.maxLevel = 16;
        config.gridSize = 17;
        config.nodesPerEdge = 16;
        const auto frames = assembleTerrainFrameForCamera(scheme, camera, e, source, config);
        EXPECT_FALSE(frames.empty()) << s.name << " 应能出帧";
        if (!frames.empty()) {
            size_t verts = 0;
            size_t tris = 0;
            for (const auto& f : frames) {
                verts += f.mesh.positionsEcef.size();
                tris += f.mesh.indices.size() / 3;
            }
            EXPECT_GT(verts, 0u) << s.name;
            EXPECT_GT(tris, 0u) << s.name;
        }
    }
}
