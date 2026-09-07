#include <earth_engine/core/math/MathUtils.h>
#include "earth_engine/camera/TerrainCameraPipeline.h"

namespace earth_engine {

std::vector<TerrainFrameAssembler::Frame> assembleTerrainFrameForCamera(
    const WebMercatorTileScheme& scheme, const CameraView& camera, const Ellipsoid& ellipsoid,
    const ITerrainDataSource& source, const TerrainCameraPipelineConfig& config) {
    std::optional<Rectangle> footprint = camera.groundFootprintRadians(ellipsoid);
    if (!footprint) {
        // 仅在视线朝下（朝地球）时回退到相机正下区域：低俯仰/掠视上角射线
        // 可能出太空；真正"往太空看"（fwd·n>0）保持空结果。
        const Cartographic cam = ellipsoid.cartesianToCartographic(camera.position());
        const Vec3 n = ellipsoid.geodeticSurfaceNormal(cam);
        if (camera.forward().dot(n) >= -0.1) {
            return {};
        }
        footprint = Rectangle::fromDegrees(radiansToDegrees(cam.longitude()) - 0.35,
                                           radiansToDegrees(cam.latitude()) - 0.25,
                                           radiansToDegrees(cam.longitude()) + 0.35,
                                           radiansToDegrees(cam.latitude()) + 0.25);
    }
    const TerrainLodSelector selector;
    TerrainLodResult selection =
        selector.selectTiles(scheme, camera.position(), footprint.value(), config.lod);
    if (selection.tiles.empty()) {
        // 兜底：放宽阈值全视野重选。
        TerrainLodConfig wide = config.lod;
        wide.maxScreenSpaceErrorPx = 32.0;
        const Rectangle world = scheme.tileRectangleRadians(TileKey(0, 0, 0));
        selection = selector.selectTiles(scheme, camera.position(), world, wide, nullptr);
    }
    const TerrainFrameAssembler assembler;
    return assembler.assemble(scheme, selection, source, ellipsoid, config.gridSize,
                              config.nodesPerEdge);
}

} // namespace earth_engine
