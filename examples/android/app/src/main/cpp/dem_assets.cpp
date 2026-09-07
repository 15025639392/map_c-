#include "dem_assets.h"

#include <android/log.h>
#include <cmath>

#include "net_fetch.h"

#include <earth_engine/content/HeightmapCodec.h>
#include <earth_engine/content/HeightmapTile.h>
#include <earth_engine/providers/StbPngDecoder.h>

#define LOG_TAG "map_cplus"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace earth_engine;

namespace demoscene {

namespace {

bool readAssetBytes(AAssetManager* manager, const std::string& path, std::vector<uint8_t>& out) {
    AAsset* asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        return false;
    }
    const off_t length = AAsset_getLength(asset);
    if (length <= 0) {
        AAsset_close(asset);
        return false;
    }
    out.resize(static_cast<size_t>(length));
    const int read = AAsset_read(asset, out.data(), static_cast<size_t>(length));
    AAsset_close(asset);
    return read == length;
}

} // namespace

DemAssetSource::DemAssetSource(AAssetManager* manager) : manager_(manager) {}

std::optional<TerrainGrid> DemAssetSource::requestHeights(
    const WebMercatorTileScheme& scheme, const TileKey& key, int) const {
    if (manager_ == nullptr) {
        return std::nullopt;
    }
    const std::string path = "dem/" + std::to_string(key.z()) + "/" + std::to_string(key.x()) +
                             "/" + std::to_string(key.y()) + ".png";
    std::vector<uint8_t> bytes;
    if (!readAssetBytes(manager_, path, bytes)) {
        return std::nullopt; // 无该瓦资产（分带窗口外）
    }
    const std::optional<RgbImage> image = decodePngToRgb(bytes.data(), bytes.size());
    if (!image) {
        return std::nullopt;
    }
    TerrainGrid grid;
    grid.width = image->width;
    grid.height = image->height;
    grid.heights.resize(static_cast<size_t>(grid.width) * grid.height);
    if (!HeightmapCodec::decodeTerrarium(image->rgb.data(), static_cast<size_t>(grid.width),
                                         static_cast<size_t>(grid.height), image->strideBytes(),
                                         grid.heights.data())) {
        return std::nullopt;
    }
    return grid;
}

// NASA 网络字节源（native → Java HttpURLConnection）。
std::optional<std::vector<uint8_t>> NasaHttpBytesSource::requestTileBytes(
    const TileKey&, const std::string& url) const {
    return httpGetBytes(url);
}

int bandLevelForAltitudeMeters(double altitudeMeters) {
    if (altitudeMeters >= 50000.0) {
        return 10;
    }
    if (altitudeMeters >= 20000.0) {
        return 11;
    }
    if (altitudeMeters >= 8000.0) {
        return 12;
    }
    return 13;
}

std::vector<TileKey> bandKeysForRectangle(const WebMercatorTileScheme& scheme,
                                          const Rectangle& footprintRadians, int level) {
    const int n = scheme.tilesPerSide(level);
    if (n <= 0) {
        return {};
    }
    const double span = 2.0 * scheme.worldHalfExtentMeters();
    const double cell = span / static_cast<double>(n);

    const Cartographic sw(footprintRadians.west(), footprintRadians.south(), 0.0);
    const Cartographic ne(footprintRadians.east(), footprintRadians.north(), 0.0);
    const Vec2 pSw = scheme.projectToMeters(sw);
    const Vec2 pNe = scheme.projectToMeters(ne);
    const double half = scheme.worldHalfExtentMeters();

    const int x0 = static_cast<int>((pSw.x() + half) / cell);
    const int x1 = static_cast<int>((pNe.x() + half) / cell);
    // y（顶行原点）：y = (half − my)/cell；北边 my 大 → y 小。
    const int y0 = static_cast<int>((half - pNe.y()) / cell);
    const int y1 = static_cast<int>((half - pSw.y()) / cell);

    std::vector<TileKey> keys;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (x >= 0 && x < n && y >= 0 && y < n) {
                keys.emplace_back(level, x, y);
            }
        }
    }
    return keys;
}

std::vector<DemFrame> buildDemFrames(const WebMercatorTileScheme& scheme,
                                     const ITerrainDataSource& source,
                                     const Rectangle& footprintRadians,
                                     const Ellipsoid& ellipsoid, int level, int nodesPerEdge,
                                     int requestCells) {
    std::vector<DemFrame> frames;
    const TerrainTileMeshBuilder builder;
    for (const TileKey& key : bandKeysForRectangle(scheme, footprintRadians, level)) {
        const std::optional<TerrainGrid> grid = source.requestHeights(scheme, key, requestCells);
        if (!grid || grid->empty()) {
            continue;
        }
        // 透传环栅格语义（borderInset/noData）：网格采样按内缩映射读环内邻瓦回填
        // → 相邻瓦共享边一致（与 host test_nasa_ring_source 同一机制）。
        const HeightmapTile tile(scheme, key, grid->heights.data(), grid->width, grid->height,
                                 grid->noDataValues.data(),
                                 static_cast<int>(grid->noDataValues.size()),
                                 grid->borderInset);
        DemFrame frame;
        frame.key = key;
        frame.mesh = builder.build(tile, ellipsoid, nodesPerEdge);
        frames.push_back(std::move(frame));
    }
    return frames;
}

} // namespace demoscene
