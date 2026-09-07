#include <earth_engine/core/math/MathUtils.h>
#include "earth_engine/camera/TerrainCameraPipeline.h"

namespace earth_engine {

std::vector<TerrainFrameAssembler::Frame> assembleTerrainFrameForCamera(
    const WebMercatorTileScheme& scheme, const CameraView& camera, const Ellipsoid& ellipsoid,
    const ITerrainDataSource& source, const TerrainCameraPipelineConfig& config) {
    std::optional<Rectangle> footprint = camera.groundFootprintRadians(ellipsoid);
    if (!footprint) {
        // 低俯仰/掠视时上角射线可能出太空：回退到相机正下区域（与 Android demo 同语义）。
        const Cartographic cam = ellipsoid.cartesianToCartographic(camera.position());
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
