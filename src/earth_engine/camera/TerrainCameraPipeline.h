#pragma once

#include <vector>

#include "CameraView.h"
#include "../content/TerrainDataSource.h"
#include "../content/TerrainFrameAssembler.h"
#include "../tiling/TerrainLodSelector.h"
#include "../tiling/WebMercatorTileScheme.h"

namespace earth_engine {

/// 相机 → 地形帧的端到端参数。
struct TerrainCameraPipelineConfig {
    TerrainCameraPipelineConfig() { lod.maxScreenSpaceErrorPx = 8.0; }
    TerrainLodConfig lod;   // LOD 选择参数
    int gridSize = 17;      // 每瓦解码栅格（格点/边）
    int nodesPerEdge = 8;   // 每瓦几何网格段数
};

/// 一步管线：相机地表脚印 → LOD 选择 → 数据源解码 → ECEF 地形帧。
/// 脚印为空（看向太空）返回空 vector。
std::vector<TerrainFrameAssembler::Frame> assembleTerrainFrameForCamera(
    const WebMercatorTileScheme& scheme, const CameraView& camera, const Ellipsoid& ellipsoid,
    const ITerrainDataSource& source, const TerrainCameraPipelineConfig& config);

} // namespace earth_engine
