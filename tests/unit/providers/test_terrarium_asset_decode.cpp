// 真实资产 host 解码回归（Terrarium PNG 真实字节路径）：
// 内置 DEM（缙云山 z10–13，examples/android assets，git 跟踪 559 瓦）此前只在
// Android app 内解码——host 侧 PNG 测试全部是 Terrain-RGB 合成 fixture。本套件
// 用仓库内**真实 PNG 字节**走 DemAssetSource 同款链（StbPngDecoder → Terrarium），
// 并把实测的同级共享边差距（a4-merge-plan §7：z13 均值 ~1.8m / z12 ~3.9–4.7m）
// 锁成可重复回归（T-V5 在真数据上未闭合的账本行）。
#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/camera/CameraView.h"
#include "earth_engine/camera/TerrainCameraPipeline.h"
#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/content/SeamAudit.h"
#include "earth_engine/content/TerrainDataSource.h"
#include "earth_engine/content/TerrainFrameAssembler.h"
#include "earth_engine/content/TerrainPicking.h"
#include "earth_engine/content/TerrainTileMesh.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/providers/StbPngDecoder.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

// 资产相对仓库根路径。
const std::string kAssetsRoot = std::string(MAPC_REPO_ROOT) +
                                "/examples/android/app/src/main/assets/dem/";

std::optional<std::vector<uint8_t>> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                std::istreambuf_iterator<char>());
}

// DemAssetSource 同款：PNG 字节 → Terrarium 高度栅格。
std::optional<std::vector<double>> decodeTerrariumTile(const std::string& path, int& outSize) {
    const auto bytes = readFile(path);
    if (!bytes) {
        return std::nullopt;
    }
    const std::optional<RgbImage> image = decodePngToRgb(bytes->data(), bytes->size());
    if (!image || image->width != image->height) {
        return std::nullopt;
    }
    outSize = image->width;
    std::vector<double> h(static_cast<size_t>(image->width) * image->height);
    if (!HeightmapCodec::decodeTerrarium(image->rgb.data(), image->width, image->height,
                                         image->strideBytes(), h.data())) {
        return std::nullopt;
    }
    return h;
}

// 解码栅格 → 装配帧（顶点栅格模型，节点 33/边）。
TerrainFrameAssembler::Frame makeFrame(const WebMercatorTileScheme& scheme, const TileKey& key,
                                       const std::vector<double>& h, int size) {
    const HeightmapTile tile(scheme, key, h.data(), size, size);
    TerrainFrameAssembler::Frame frame;
    frame.key = key;
    frame.mesh = TerrainTileMeshBuilder().build(tile, Ellipsoid::WGS84(), 32);
    return frame;
}

// 真实资产数据源（Android DemAssetSource 的 host 形态）：瓦 PNG → Terrarium 栅格。
// 资产栅格固定 256×256（与 dem_assets 一致）；请求其他尺寸返回 nullopt。
class RealDemAssetSource : public ITerrainDataSource {
public:
    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme&,
                                              const TileKey& key, int gridSize) const override {
        if (gridSize != 256) {
            return std::nullopt;
        }
        const std::string path = kAssetsRoot + key.toString() + ".png";
        int size = 0;
        const auto h = decodeTerrariumTile(path, size);
        if (!h) {
            return std::nullopt;
        }
        TerrainGrid grid;
        grid.width = size;
        grid.height = size;
        grid.heights = *h;
        return grid;
    }
};

bool tileExists(const std::string& rel) { return readFile(kAssetsRoot + rel).has_value(); }

} // namespace

// ---- 真实 Terrarium PNG 解码（host 覆盖 Android demo 同款路径） ------------

TEST(TerrariumAssetDecode, RealTileDecodesPlausibleHeights) {
    int size = 0;
    const auto h = decodeTerrariumTile(kAssetsRoot + "13/6518/3387.png", size);
    if (!h) {
        GTEST_SKIP() << "真实资产缺失（仓库根：" << kAssetsRoot << "）";
    }
    ASSERT_EQ(size, 256);
    double mn = 1e30, mx = -1e30;
    for (const double v : *h) {
        ASSERT_TRUE(std::isfinite(v));
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    // 机位中心瓦：缙云山脚 100–700m 量级（实测 159–456）；黑/nodata 应极少。
    EXPECT_GT(mn, 50.0);
    EXPECT_LT(mx, 800.0);
    int nearNoData = 0;
    for (const double v : *h) {
        if (v <= -32760.0) {
            ++nearNoData;
        }
    }
    EXPECT_EQ(nearNoData, 0);
}

// ---- 真实资产同级共享边回归（T-V5 账本：真数据未闭合，数值锁档） -----------

TEST(TerrariumAssetSeam, RealZ13EdgeGapMatchesMeasurement) {
    const WebMercatorTileScheme scheme;
    const std::string relW = "13/6518/3387.png", relE = "13/6519/3387.png",
                      relS = "13/6518/3388.png";
    if (!(tileExists(relW) && tileExists(relE) && tileExists(relS))) {
        GTEST_SKIP() << "真实资产缺失";
    }
    int s1 = 0, s2 = 0, s3 = 0;
    const auto hw = decodeTerrariumTile(kAssetsRoot + relW, s1);
    const auto he = decodeTerrariumTile(kAssetsRoot + relE, s2);
    const auto hs = decodeTerrariumTile(kAssetsRoot + relS, s3);
    ASSERT_TRUE(hw && he && hs);
    std::vector<TerrainFrameAssembler::Frame> frames;
    frames.push_back(makeFrame(scheme, TileKey(13, 6518, 3387), *hw, s1));
    frames.push_back(makeFrame(scheme, TileKey(13, 6519, 3387), *he, s2));
    frames.push_back(makeFrame(scheme, TileKey(13, 6518, 3388), *hs, s3));
    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    EXPECT_EQ(audit.edgePairsFound, 2);
    ASSERT_GT(audit.comparedNodePairs, 0);
    // 实测口径（像元边界差均值 ~1.8m）：网格同级边同样未闭合且量级相符。
    EXPECT_GT(audit.meanMeters, 0.2);
    EXPECT_LT(audit.meanMeters, 12.0);
    EXPECT_GT(audit.maxMeters, 0.5);
    EXPECT_LT(audit.maxMeters, 60.0);
    EXPECT_GT(audit.nodePairsOverOneMeter, 0); // >1m 的节点对确实存在
}

TEST(TerrariumAssetSeam, RealZ12EdgeGapCoarserLarger) {
    const WebMercatorTileScheme scheme;
    const std::string relW = "12/3259/1693.png", relE = "12/3260/1693.png";
    if (!(tileExists(relW) && tileExists(relE))) {
        GTEST_SKIP() << "真实资产缺失";
    }
    int s1 = 0, s2 = 0;
    const auto hw = decodeTerrariumTile(kAssetsRoot + relW, s1);
    const auto he = decodeTerrariumTile(kAssetsRoot + relE, s2);
    ASSERT_TRUE(hw && he);
    std::vector<TerrainFrameAssembler::Frame> frames;
    frames.push_back(makeFrame(scheme, TileKey(12, 3259, 1693), *hw, s1));
    frames.push_back(makeFrame(scheme, TileKey(12, 3260, 1693), *he, s2));
    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    ASSERT_GT(audit.comparedNodePairs, 0);
    // z12 像元更粗 → 边差均值更大（实测 3.9–4.7m vs z13 1.8m）。
    EXPECT_GT(audit.meanMeters, 1.0);
    EXPECT_LT(audit.meanMeters, 25.0);
}

// ---- 相机管线级：真实 DEM 帧（M-near 型正下机位，模拟器 demo 的 host 替身）----

TEST(TerrariumAssetPipeline, CameraFrameOverRealDemPicksPlausibleHeight) {
    const WebMercatorTileScheme scheme;
    const Ellipsoid& ellipsoid = Ellipsoid::WGS84();
    if (!tileExists("13/6518/3387.png")) {
        GTEST_SKIP() << "真实资产缺失";
    }
    // M-near 型：缙云山中心上空 3km 正下（高度带 → z13）。
    const Cartographic center = Cartographic::fromDegrees(106.44, 29.70, 0.0);
    const Vec3 pos = ellipsoid.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), 3000.0));
    const Vec3 up = ellipsoid.geodeticSurfaceNormal(center);
    const CameraView camera(pos, ellipsoid.cartographicToCartesian(center), up,
                            degreesToRadians(60.0), 4.0 / 3.0);

    TerrainCameraPipelineConfig config;
    config.lod.maxScreenSpaceErrorPx = 8.0;
    config.lod.maxLevel = 13; // 资产到 z13（与 demo 高度带一致）
    config.gridSize = 256;    // 资产栅格固定 256²
    config.nodesPerEdge = 16;

    const RealDemAssetSource source;
    const auto frames = assembleTerrainFrameForCamera(scheme, camera, ellipsoid, source, config);
    ASSERT_FALSE(frames.empty());
    // 正下点被覆盖（中心瓦 z13 在资产内）。
    const auto hit = pickTerrainFrame(camera.rayThroughNdc(0.0, 0.0).origin(),
                                      camera.rayThroughNdc(0.0, 0.0).direction(), frames);
    ASSERT_TRUE(hit.has_value());
    const Cartographic hitCarto = ellipsoid.cartesianToCartographic(hit->point);
    // 缙云山脚 100–700m 量级（机位中心实测瓦 159–456）。
    EXPECT_GT(hitCarto.height(), 50.0);
    EXPECT_LT(hitCarto.height(), 900.0);

    // 帧内同级共享边如出现：T-V5 账本 —— 真实数据未闭合（mean > 0.2m 档）。
    const SeamAuditResult audit = auditSameLevelSharedEdges(frames);
    if (audit.comparedNodePairs > 0) {
        EXPECT_GT(audit.meanMeters, 0.0);
    }
}
