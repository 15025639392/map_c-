#include "earth_engine/camera/TerrainCameraPipeline.h"

namespace earth_engine {

std::vector<TerrainFrameAssembler::Frame> assembleTerrainFrameForCamera(
    const WebMercatorTileScheme& scheme, const CameraView& camera, const Ellipsoid& ellipsoid,
    const ITerrainDataSource& source, const TerrainCameraPipelineConfig& config) {
    const std::optional<Rectangle> footprint = camera.groundFootprintRadians(ellipsoid);
    if (!footprint) {
        return {};
    }
    const TerrainLodSelector selector;
    const TerrainLodResult selection =
        selector.selectTiles(scheme, camera.position(), footprint.value(), config.lod);
    if (selection.tiles.empty()) {
        return {};
    }
    const TerrainFrameAssembler assembler;
    return assembler.assemble(scheme, selection, source, ellipsoid, config.gridSize,
                              config.nodesPerEdge);
}

} // namespace earth_engine
