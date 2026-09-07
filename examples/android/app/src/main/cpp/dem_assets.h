#pragma once

#include <optional>
#include <string>
#include <vector>

#include <android/asset_manager.h>

#include <earth_engine/content/TerrainDataSource.h>
#include <earth_engine/content/TerrainTileMesh.h>
#include <earth_engine/tiling/TileKey.h>
#include <earth_engine/tiling/WebMercatorTileScheme.h>

namespace demoscene {

/// 从 APK assets 读取 terrarium PNG（assets/dem/z/x/y.png）的 DEM 数据源。
/// PNG → RgbImage → Terrarium 解码 → 自然尺寸 TerrainGrid（忽略 gridSize 参数）。
class DemAssetSource : public earth_engine::ITerrainDataSource {
public:
    explicit DemAssetSource(AAssetManager* manager);

    std::optional<earth_engine::TerrainGrid> requestHeights(
        const earth_engine::WebMercatorTileScheme& scheme,
        const earth_engine::TileKey& key, int /*gridSize*/) const override;

    bool hasManager() const { return manager_ != nullptr; }

private:
    AAssetManager* manager_;
};

/// 高度 → 分带层级（覆盖 assets 存储的 z10..13）。
int bandLevelForAltitudeMeters(double altitudeMeters);

/// 兴趣矩形在 level 层覆盖的全部瓦键（XYZ 顶行原点）。
std::vector<earth_engine::TileKey> bandKeysForRectangle(
    const earth_engine::WebMercatorTileScheme& scheme,
    const earth_engine::Rectangle& footprintRadians, int level);

/// 按分带构建 ECEF 网格帧（缺瓦跳过）。
struct DemFrame {
    earth_engine::TileKey key;
    earth_engine::TerrainMeshData mesh;
};
std::vector<DemFrame> buildDemFrames(const earth_engine::WebMercatorTileScheme& scheme,
                                     const earth_engine::ITerrainDataSource& source,
                                     const earth_engine::Rectangle& footprintRadians,
                                     const earth_engine::Ellipsoid& ellipsoid, int level,
                                     int nodesPerEdge);

} // namespace demoscene
