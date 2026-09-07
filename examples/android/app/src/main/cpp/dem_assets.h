#pragma once

#include <optional>
#include <string>
#include <vector>

#include <android/asset_manager.h>

#include <earth_engine/content/TerrainDataSource.h>
#include <earth_engine/content/TerrainTileMesh.h>
#include <earth_engine/providers/ITileBytesSource.h>
#include <earth_engine/providers/TerrainRgbPngTileSource.h>
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

/// 网络字节源：Java HttpURLConnection（经 JNI）拉取 URL（https 由系统栈处理）。
class NasaHttpBytesSource : public earth_engine::ITileBytesSource {
public:
    std::optional<std::vector<uint8_t>> requestTileBytes(
        const earth_engine::TileKey&, const std::string& url) const override;
};

/// NASA Terrain-RGB 514 带环源（Mapbox 标准）的组合体：字节源 + TerrainRgbPngTileSource
/// 环模式（cells=512 → PNG 514²，borderInset 0.5）+ zoom 范围 z6–12。
/// 端点：https://mapoverlay.xinzhi.space/3dterrain/nasa/tiles/{z}/{x}/{y}.png
struct NasaRingDemSource {
    NasaHttpBytesSource bytes;
    earth_engine::TerrainRgbPngTileSource source;

    NasaRingDemSource()
        : source(bytes, kNasaUrlTemplate,
                 /*cellRegisteredRing=*/true, /*minZoom=*/6, /*maxZoom=*/12) {}

    static constexpr const char* kNasaUrlTemplate =
        "https://mapoverlay.xinzhi.space/3dterrain/nasa/tiles/{z}/{x}/{y}.png";
};

/// 高度 → 分带层级（覆盖 assets 存储的 z10..13）。
int bandLevelForAltitudeMeters(double altitudeMeters);

/// 兴趣矩形在 level 层覆盖的全部瓦键（XYZ 顶行原点）。
std::vector<earth_engine::TileKey> bandKeysForRectangle(
    const earth_engine::WebMercatorTileScheme& scheme,
    const earth_engine::Rectangle& footprintRadians, int level);

/// 按分带构建 ECEF 网格帧（缺瓦跳过）。requestCells = 期望栅格 cell 数
/// （顶点栅格源如 assets：任意值；NASA 环源：512 → PNG 514²）；环栅格经
/// HeightmapTile 透传 borderInset/noDataValues，网格采样走内缩映射。
struct DemFrame {
    earth_engine::TileKey key;
    earth_engine::TerrainMeshData mesh;
};
std::vector<DemFrame> buildDemFrames(const earth_engine::WebMercatorTileScheme& scheme,
                                     const earth_engine::ITerrainDataSource& source,
                                     const earth_engine::Rectangle& footprintRadians,
                                     const earth_engine::Ellipsoid& ellipsoid, int level,
                                     int nodesPerEdge, int requestCells = 256);

} // namespace demoscene
