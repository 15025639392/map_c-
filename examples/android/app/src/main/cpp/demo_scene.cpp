#include "demo_scene.h"

#include "dem_assets.h"
#include "gles3_device.h"

#include <GLES3/gl3.h>

#include <android/log.h>
#include <sys/system_properties.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <vector>

#include <earth_engine/camera/CameraView.h>
#include <earth_engine/camera/Frustum.h>
#include <earth_engine/core/geodesy/Transforms.h>
#include <earth_engine/content/HeightmapTile.h>
#include <earth_engine/imagery/ImageryTileSource.h>
#include <earth_engine/renderer/HeightTextureCodec.h>
#include <earth_engine/content/TerrainDataSource.h>
#include <earth_engine/content/TerrainFrameAssembler.h>
#include <earth_engine/core/geodesy/Ellipsoid.h>
#include <earth_engine/core/math/Mat4.h>
#include <earth_engine/core/math/MathUtils.h>
#include <earth_engine/tiling/TerrainLodSelector.h>
#include <earth_engine/providers/TileCacheBytesSource.h>
#include <earth_engine/tiling/WebMercatorTileScheme.h>

#define LOG_TAG "map_cplus"
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace earth_engine;

namespace demoscene {

namespace {

// ---------------------------------------------------------------------------
// 合成高度源（与 host 单测同一语义：fn 在每瓦像素格点求值）
// ---------------------------------------------------------------------------
double hillFn(const Cartographic& c) {
    // 大起伏合成地形（数千米级，含两个波长），用于观感演示/机位调参。
    return 1500.0 +
           3200.0 * std::sin(c.longitude() * 55.0) * std::cos(c.latitude() * 42.0) +
           900.0 * std::sin(c.longitude() * 220.0) * std::cos(c.latitude() * 130.0);
}

class FunctionalTerrainSource : public ITerrainDataSource {
public:
    std::optional<TerrainGrid> requestHeights(const WebMercatorTileScheme& scheme,
                                              const TileKey& key,
                                              int gridSize) const override {
        TerrainGrid grid;
        grid.width = gridSize;
        grid.height = gridSize;
        grid.heights.resize(static_cast<size_t>(gridSize * gridSize));
        std::vector<double> zeros(static_cast<size_t>(gridSize * gridSize), 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), gridSize, gridSize);
        for (int row = 0; row < gridSize; ++row) {
            for (int col = 0; col < gridSize; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                grid.heights[static_cast<size_t>(row * gridSize + col)] = hillFn(c);
            }
        }
        return grid;
    }
};

// ---------------------------------------------------------------------------
// Shader 工具
// ---------------------------------------------------------------------------
// 相机位姿基（与 CameraView 相同的施密特正交化，这里直接算 Vec3）。
struct Basis {
    Vec3 right;
    Vec3 up;
    Vec3 fwd;
};
Basis makeBasis(const Vec3& pos, const Vec3& target, const Vec3& upHint) {
    Basis b;
    b.fwd = (target - pos).normalized();
    if (b.fwd.magnitudeSquared() <= 0.0) {
        b.fwd = Vec3(0.0, 0.0, -1.0);
    }
    Vec3 r = b.fwd.cross(upHint).normalized();
    if (r.magnitudeSquared() <= 0.0) {
        r = Vec3(0.0, 1.0, 0.0);
        if (std::fabs(r.dot(b.fwd)) > 0.99) {
            r = Vec3(1.0, 0.0, 0.0);
        }
        r = r.cross(b.fwd).normalized();
    }
    b.right = r;
    b.up = r.cross(b.fwd).normalized();
    return b;
}

// 透视矩阵（列主序，GL 约定）。
Mat4 perspectiveMatrix(double fovYRad, double aspect, double zNear, double zFar) {
    const double f = 1.0 / std::tan(fovYRad * 0.5);
    const double nf = 1.0 / (zNear - zFar);
    return Mat4(f / aspect, 0.0, 0.0, 0.0,   // col0
                0.0, f, 0.0, 0.0,            // col1
                0.0, 0.0, (zFar + zNear) * nf, -1.0,  // col2
                0.0, 0.0, 2.0 * zFar * zNear * nf, 0.0);  // col3
}

// 视图（无平移；几何已做 RTC：顶点 = ECEF − camera）。
Mat4 viewFromBasis(const Basis& b) {
    // 相机坐标 = (R·p, U·p, −F·p)。
    return Mat4(Vec3(b.right.x(), b.up.x(), -b.fwd.x()),
                Vec3(b.right.y(), b.up.y(), -b.fwd.y()),
                Vec3(b.right.z(), b.up.z(), -b.fwd.z()),
                Vec3(0.0, 0.0, 0.0));
}

const char* kVs =
    "#version 300 es\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNor;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat3 uViewRot;\n"
    "layout(location=2) in float aHei;\n"
    "layout(location=3) in vec2 aUv;\n"
    "layout(location=4) in vec3 aDisp;\n"
    "uniform int uDispMode; // 1 = 顶点位移属性通道：p = aPos + aDisp（基准椭球面 + 位移）\n"
    "out vec3 vNor;\n"
    "out float vHeight;\n"
    "out vec2 vUv;\n"
    "void main() {\n"
    "  vec3 p = (uDispMode == 1) ? (aPos + aDisp) : aPos;\n"
    "  vNor = uViewRot * aNor;\n"
    "  vHeight = aHei;\n"
    "  vUv = aUv;\n"
    "  gl_Position = uMvp * vec4(p, 1.0);\n"
    "}\n";

const char* kFs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vNor;\n"
    "out vec4 fragColor;\n"
    "uniform vec3 uLightDir;\n"
    "uniform vec3 uBaseColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uLabel;\n"
    "uniform int uImageryMode; // 1 = 卫星影像作 albedo（高德）；0 = 高度着色\n"
    "uniform int uHasLabel;    // 1 = 有路网注记叠加层（单位 1）\n"
    "in float vHeight;\n"
    "in vec2 vUv;\n"
    "void main() {\n"
    "  vec3 n = normalize(vNor);\n"
    "  vec3 l = normalize(uLightDir);\n"
    "  float d = max(dot(n, l), 0.0);\n"
    "  vec3 albedo;\n"
    "  if (uImageryMode == 1) {\n"
    "    albedo = texture(uTex, vUv).rgb;\n"
    "    if (uHasLabel == 1) {\n"
    "      vec4 lbl = texture(uLabel, vUv);\n"
    "      albedo = mix(albedo, lbl.rgb, lbl.a);\n"
    "    }\n"
    "  } else if (uImageryMode == 2) {\n"
    "    vec3 enc = texture(uTex, vUv).rgb;\n"
    "    float h = -10000.0 + dot(enc, vec3(65536.0, 256.0, 1.0)) * 0.1;\n"
    "    float t = clamp((h - (-1000.0)) / 6000.0, 0.0, 1.0);\n"
    "    vec3 low  = vec3(0.35, 0.48, 0.25);\n"
    "    vec3 mid  = vec3(0.48, 0.43, 0.30);\n"
    "    vec3 high = vec3(0.66, 0.52, 0.34);\n"
    "    albedo = t < 0.5 ? mix(low, mid, t * 2.0) : mix(mid, high, (t - 0.5) * 2.0);\n"
    "  } else {\n"
    "    float t = clamp((vHeight - (-1000.0)) / 6000.0, 0.0, 1.0);\n"
    "    vec3 low  = vec3(0.35, 0.48, 0.25);\n"
    "    vec3 mid  = vec3(0.48, 0.43, 0.30);\n"
    "    vec3 high = vec3(0.66, 0.52, 0.34);\n"
    "    albedo = t < 0.5 ? mix(low, mid, t * 2.0) : mix(mid, high, (t - 0.5) * 2.0);\n"
    "  }\n"
    "  vec3 c = albedo * (0.42 + 0.9 * d * d);\n"
    "  fragColor = vec4(c, 1.0);\n"
    "}\n";

} // namespace

// ---------------------------------------------------------------------------
// TerrainScene
// ---------------------------------------------------------------------------
void TerrainScene::destroyGlObjects() {
    if (device_) {
        if (programHandle_ != 0) {
            device_->releaseProgram(programHandle_);
        }
        for (uint32_t h : meshHandles_) {
            device_->releaseMesh(h);
        }
        if (textureHandle_ != 0) {
            device_->releaseTexture(textureHandle_);
        }
        for (uint32_t th : tileTextures_) {
            if (th != 0u) {
                device_->releaseTexture(th);
            }
        }
        for (uint32_t th : labelTextures_) {
            if (th != 0u) {
                device_->releaseTexture(th);
            }
        }
    }
    programHandle_ = 0;
    textureHandle_ = 0;
    tileTextures_.clear();
    labelTextures_.clear();
    meshHandles_.clear();
    device_.reset();
    geometryReady_ = false;
}

void TerrainScene::initializeGl() {
    destroyGlObjects();
    char buf[PROP_VALUE_MAX] = {0};
    if (__system_property_get("debug.mapc.station", buf) > 0 && buf[0] != '\0') {
        station_ = std::atoi(buf);
        if (station_ < 1 || station_ > 5) {
            station_ = 2;
        }
    }
    ALOG("station=%d", station_);
    char demProp[PROP_VALUE_MAX] = {0};
    const int propLen = __system_property_get("debug.mapc.dem", demProp);
    if (propLen <= 0) {
        useDem_ = (assetManager_ != nullptr); // 默认：有 DEM 资产即真地形
    } else {
        useDem_ = (demProp[0] == '1');
    }
    char imgProp[PROP_VALUE_MAX] = {0};
    char lblProp[PROP_VALUE_MAX] = {0};
    useImg_ = (__system_property_get("debug.mapc.img", imgProp) <= 0) || (imgProp[0] == '1');
    useLbl_ = (__system_property_get("debug.mapc.lbl", lblProp) <= 0) || (lblProp[0] == '1');
    char hgtProp[PROP_VALUE_MAX] = {0};
    char dispProp[PROP_VALUE_MAX] = {0};
    useHgt_ = (__system_property_get("debug.mapc.hgt", hgtProp) > 0) && (hgtProp[0] == '1');
    useDisp_ = (__system_property_get("debug.mapc.disp", dispProp) > 0) && (dispProp[0] == '1');
    ALOG("dem mode=%d (assetMgr=%d) img=%d lbl=%d hgt=%d disp=%d", useDem_ ? 1 : 0,
         assetManager_ != nullptr ? 1 : 0, useImg_ ? 1 : 0, useLbl_ ? 1 : 0,
         useHgt_ ? 1 : 0, useDisp_ ? 1 : 0);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    device_ = std::make_unique<Gles3RenderDevice>();
    device_->clearColor(0.42f, 0.55f, 0.74f, 1.0f); // 天蓝
    render::ProgramSource src;
    src.vertexShader = kVs;
    src.fragmentShader = kFs;
    programHandle_ = device_->createProgram(src);
    if (programHandle_ == 0) {
        ALOGE("createProgram failed");
    }
    // 合成 256² 棋盘"影像瓦"（纹理管线验证；真实影像源接入后替换）。
    {
        constexpr int kTex = 256;
        constexpr int kCell = 32;
        render::Texture2DData tex;
        tex.width = kTex;
        tex.height = kTex;
        tex.rgba8.resize(static_cast<size_t>(kTex) * kTex * 4);
        for (int y = 0; y < kTex; ++y) {
            for (int x = 0; x < kTex; ++x) {
                const int idx = (y / kCell + x / kCell) & 1;
                const uint8_t g = idx == 0 ? 255 : 200;
                const size_t off = (static_cast<size_t>(y) * kTex + x) * 4;
                tex.rgba8[off] = g;
                tex.rgba8[off + 1] = g;
                tex.rgba8[off + 2] = g;
                tex.rgba8[off + 3] = 255;
            }
        }
        textureHandle_ = device_->createTexture2D(tex);
        ALOG("checker texture handle=%u", textureHandle_);
    }
}

void TerrainScene::resize(int widthPx, int heightPx) {
    width_ = widthPx;
    height_ = heightPx;
}

void TerrainScene::drawFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ++frameCount_;
    if (!geometryReady_) {
        ensureGeometry();
    }
    if (!geometryReady_ || programHandle_ == 0 || meshHandles_.empty()) {
        if (frameCount_ % 120 == 1) {
            ALOG("draw frame=%ld (no geometry yet)", static_cast<long>(frameCount_));
        }
        return;
    }
    // 方形视口（aspect=1 相机），居中。
    const int side = std::min(width_, height_);
    glViewport((width_ - side) / 2, (height_ - side) / 2, side, side);

    device_->useProgram(programHandle_);
    device_->clearColor(0.42f, 0.55f, 0.74f, 1.0f);
    if (tileTextures_.empty() && textureHandle_ != 0) {
        device_->bindTexture2D(0, textureHandle_); // 高度着色模式仍绑安全纹理
        device_->setUniformInt("uTex", 0);
        device_->setUniformInt("uImageryMode", 0);
    }

    // MVP：透视 × 视图（几何已 RTC 到相机位置）。
    const double fovY = earth_engine::degreesToRadians(60.0);
    const Mat4 proj = perspectiveMatrix(fovY, 1.0, 50.0, 600000.0);
    const Vec3& cameraPos = cameraPosCache_;
    const Basis basis = makeBasis(cameraPos, cameraTargetCache_, cameraUpCache_);
    const Mat4 view = viewFromBasis(basis);
    const Mat4 mvp = proj * view;
    float m[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            m[c * 4 + r] = static_cast<float>(mvp.at(c, r));
        }
    }
    device_->setUniformMat4("uMvp", m);
    // 视图旋转 3x3（列主序）。
    float vr[9];
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            vr[c * 3 + r] = static_cast<float>(view.at(c, r));
        }
    }
    device_->setUniformMat3("uViewRot", vr);
    device_->setUniformVec3("uLightDir", 0.25f, 0.55f, 0.80f);
    device_->setUniformVec3("uBaseColor", 0.55f, 0.45f, 0.30f);
    // 位移通道模式：1 = GPU 顶点位移（p = aPos + aDisp）；0 = 常规烘焙位置。
    device_->setUniformInt("uDispMode", useDisp_ ? 1 : 0);
    for (size_t i = 0; i < meshHandles_.size(); ++i) { // DrawList-lite：逐瓦绘制
        if (useDisp_) {
            // disp 演示对照：片元走 vHeight 高度属性着色（与 baked 相同着色路径）。
            device_->setUniformInt("uImageryMode", 0);
        } else if (useHgt_ && !tileTextures_.empty() && tileTextures_[i] != 0u) {
            // 高度纹理模式（GPU 位移数据链演示：绝对编码 RGBA → shader 解码色带）。
            device_->bindTexture2D(0, tileTextures_[i]);
            device_->setUniformInt("uTex", 0);
            device_->setUniformInt("uImageryMode", 2);
        } else if (!tileTextures_.empty()) {
            // 影像模式：每瓦高德卫星纹理（缺失瓦回退高度着色）。
            const uint32_t th = tileTextures_[i];
            if (th != 0u) {
                device_->bindTexture2D(0, th);
                device_->setUniformInt("uTex", 0);
                device_->setUniformInt("uImageryMode", 1);
                const uint32_t lh = i < labelTextures_.size() ? labelTextures_[i] : 0u;
                if (lh != 0u) {
                    device_->bindTexture2D(1, lh);
                    device_->setUniformInt("uLabel", 1);
                    device_->setUniformInt("uHasLabel", 1);
                } else {
                    device_->bindTexture2D(1, 0u);
                    device_->setUniformInt("uHasLabel", 0);
                }
            } else {
                device_->bindTexture2D(1, 0u);
                device_->setUniformInt("uHasLabel", 0);
                device_->setUniformInt("uImageryMode", 0);
            }
        }
        device_->drawMesh(meshHandles_[i]);
    }
    if (frameCount_ == 1) {
        const GLenum err = glGetError();
        ALOG("draw debug: glErr=0x%x (render via IRenderDevice)", err);
    }
    if (frameCount_ % 300 == 1) { // 性能账本（H2 首笔账）：北极星"每字节/三角形有账"
        const auto st = device_->stats();
        ALOG("render ledger: draws=%llu triangles=%llu upBytes=%llu liveMeshes=%u",
             static_cast<unsigned long long>(st.drawCalls),
             static_cast<unsigned long long>(st.trianglesDrawn),
             static_cast<unsigned long long>(st.uploadedBytes),
             static_cast<unsigned int>(st.liveMeshes));
    }
}

void TerrainScene::setCamera(double lonDeg, double latDeg, double altMeters,
                             double pitchDeg, double headingDeg) {
    camLonDeg_ = lonDeg;
    camLatDeg_ = latDeg;
    camAltMeters_ = altMeters;
    camPitchDeg_ = pitchDeg;
    camHeadingDeg_ = headingDeg;
    cameraUserSet_ = true;
    geometryReady_ = false; // 强制重建几何
    ALOG("setCamera lon=%.4f lat=%.4f alt=%.0f pitch=%.1f hdg=%.1f", lonDeg, latDeg,
         altMeters, pitchDeg, headingDeg);
}

void TerrainScene::setAssetManager(AAssetManager* manager) { assetManager_ = manager; }

void TerrainScene::ensureGeometry() {
    const Ellipsoid& e = Ellipsoid::WGS84();
    const Cartographic center = Cartographic::fromDegrees(camLonDeg_, camLatDeg_, 0.0);
    cameraUpCache_ = e.geodeticSurfaceNormal(center);
    const double key[5] = {camLonDeg_, camLatDeg_, camAltMeters_, camPitchDeg_, camHeadingDeg_};
    bool sameCamera = true;
    for (int i = 0; i < 5; ++i) {
        if (std::fabs(key[i] - lastKey_[i]) > 1.0e-9) {
            sameCamera = false;
            break;
        }
    }
    if (sameCamera && geometryReady_) {
        return; // 相机未变：复用已上传几何
    }
    for (int i = 0; i < 5; ++i) {
        lastKey_[i] = key[i];
    }

    TerrainLodConfig lod;
    lod.viewportHeightPx = static_cast<double>(std::min(width_, height_));
    lod.fovRadians = degreesToRadians(60.0);
    lod.geometricErrorScale = 0.001;
    lod.maxLevel = 16;

    // 相机参数：station 预设为初值；之后可由 Java 手势（nativeSetCamera）改写。
    double altMeters = camAltMeters_;
    double pitchDeg = camPitchDeg_;
    double hdgDeg = camHeadingDeg_;
    if (!cameraUserSet_) {
        // 判据文档固定机位（docs/northstar/terrain.md）：camH/pitch/heading 直译。
        // pitch = 视线相对地平线向下角（文档 pitch −x → 此处 +x）；
        // heading = 视线方位 0=北 顺时针。
        switch (station_) {
        case 1: // M-near 3000m −60° 20°
            camAltMeters_ = altMeters = 3000.0;
            camPitchDeg_ = pitchDeg = 60.0;
            camHeadingDeg_ = hdgDeg = 20.0;
            lod.maxScreenSpaceErrorPx = 0.8;
            lod.maxLevel = 17;
            break;
        case 2: // M-mid 15000m −45° 20°
            camAltMeters_ = altMeters = 15000.0;
            camPitchDeg_ = pitchDeg = 45.0;
            camHeadingDeg_ = hdgDeg = 20.0;
            lod.maxScreenSpaceErrorPx = 3.0;
            break;
        case 3: // M-graze 8000m −10° 20°
            camAltMeters_ = altMeters = 8000.0;
            camPitchDeg_ = pitchDeg = 10.0;
            camHeadingDeg_ = hdgDeg = 20.0;
            lod.maxScreenSpaceErrorPx = 4.0;
            break;
        case 4: // M-high 60000m −30° 20°
            camAltMeters_ = altMeters = 60000.0;
            camPitchDeg_ = pitchDeg = 30.0;
            camHeadingDeg_ = hdgDeg = 20.0;
            lod.maxScreenSpaceErrorPx = 6.0;
            break;
        default: // M-coarse 250km −20° 20°
            camAltMeters_ = altMeters = 250000.0;
            camPitchDeg_ = pitchDeg = 20.0;
            camHeadingDeg_ = hdgDeg = 20.0;
            lod.maxScreenSpaceErrorPx = 12.0;
            break;
        }
    }
    cameraPosCache_ = e.cartographicToCartesian(
        Cartographic(center.longitude(), center.latitude(), altMeters));
    // 视线方向（ENU 局部）：hdg 0=北；pitch=相对地平线向下角。
    const double hdg = hdgDeg * degreesToRadians(1.0);
    const double pit = pitchDeg * degreesToRadians(1.0);
    const Vec3 dirEnu(std::sin(hdg) * std::cos(pit), std::cos(hdg) * std::cos(pit),
                      -std::sin(pit));
    const Mat4 enu = Transforms::eastNorthUpToFixedFrame(
        Cartographic(center.longitude(), center.latitude(), 0.0), e);
    cameraTargetCache_ = enu.transformPoint(dirEnu * 200000.0);

    const CameraView camera(cameraPosCache_, cameraTargetCache_, cameraUpCache_,
                            degreesToRadians(60.0), 1.0);
    const Frustum frustum = Frustum::fromCamera(camera, 100.0);

    const WebMercatorTileScheme scheme;
    const std::optional<Rectangle> footOpt = camera.groundFootprintRadians(e);

    std::vector<DemFrame> frames; // key + mesh 的统一帧集合
    if (useDem_) {
        // 数据源：debug.mapc.src —— ''|'nasa'（默认，用户指定 NASA 网络源）| 'asset'。
        char srcProp[PROP_VALUE_MAX] = {0};
        const bool nasa = (__system_property_get("debug.mapc.src", srcProp) <= 0) ||
                          (srcProp[0] != 'a'); // 未设或非 asset → nasa
        if (assetManager_ == nullptr && !nasa) {
            ALOGE("dem asset source needs asset manager");
            return;
        }
        Rectangle bandRect;
        if (footOpt) {
            bandRect = footOpt.value();
        } else {
            // 脚印空（如近掠视角上角射线出太空）：回退到相机正下区域。
            bandRect = Rectangle::fromDegrees(camLonDeg_ - 0.35, camLatDeg_ - 0.25,
                                              camLonDeg_ + 0.35, camLatDeg_ + 0.25);
        }
        if (nasa) {
            // NASA Terrain-RGB 514 带环源（z6–12；近景受源上限 z12 约束）。
            // 高度栅格与影像纹理共享同一瓦片缓存（S2 去重：同 URL 单次下载）。
            NasaHttpBytesSource rawBytes;
            TileCacheBytesSource cacheBytes(rawBytes, 512);
            TerrainRgbPngTileSource ringSource(cacheBytes, NasaRingDemSource::kNasaUrlTemplate,
                                               /*cellRegisteredRing=*/true, 6, 12);
            // 影像内容 = 高德卫星（JPEG 256，XYZ；固定子域先行）。
            NasaHttpBytesSource amapRaw;
            TileCacheBytesSource amapCache(amapRaw, 512);
            const char* amapUrl =
                "https://webst01.is.autonavi.com/appmaptile?style=6&x={x}&y={y}&z={z}";
            ImageryTileSource imagery(amapCache, amapUrl,
                                      [](const TileKey& k) { return k.z() >= 3 && k.z() <= 18; });
            const bool wantImg = useImg_;
            // 路网注记层：style=8 RGBA PNG（alpha 合成；同瓦同缓存）。
            TileCacheBytesSource lblCache(amapRaw, 512);
            const char* amapLabelUrl =
                "https://webst01.is.autonavi.com/appmaptile?style=8&x={x}&y={y}&z={z}";
            ImageryTileSource labelImagery(lblCache, amapLabelUrl,
                                           [](const TileKey& k) { return k.z() >= 3 && k.z() <= 18; },
                                           /*keepAlpha=*/true);
            int level = bandLevelForAltitudeMeters(camAltMeters_);
            if (level > 12) {
                level = 12;
            }
            if (level < 6) {
                level = 6;
            }
            const int demNodes = (level >= 12) ? 65 : 33;
            frames = buildDemFrames(scheme, ringSource, bandRect, e, level, demNodes,
                                    /*requestCells=*/512);
            // 每瓦真实影像纹理（字节走同一缓存 → 单次下载；S4→渲染全链）。
            tileTextures_.clear();
            tileTextures_.reserve(frames.size());
            labelTextures_.clear();
            labelTextures_.reserve(frames.size());
            for (const auto& f : frames) {
                const auto tr = imagery.fetchTexture(f.key);
                const uint32_t th =
                    tr.has_value() ? device_->createTexture2D(tr->texture) : 0u;
                if (th == 0u) {
                    ALOG("imagery texture miss tile %s", f.key.toString().c_str());
                }
                tileTextures_.push_back(wantImg ? th : 0u);
                // 路网注记（alpha 保留；缺失或关层 → 0）。
                const auto lr =
                    (useImg_ && useLbl_) ? labelImagery.fetchTexture(f.key)
                                         : std::optional<ImageryTileSource::Result>();
                labelTextures_.push_back(lr.has_value()
                                             ? device_->createTexture2D(lr->texture)
                                             : 0u);
                if (!wantImg && th != 0u) { // 关影像层仍释放已建纹理
                    device_->releaseTexture(th);
                }
            }
            if (useHgt_) {
                // 高度纹理：每瓦 grid（缓存命中）→ 裁 cell 512 → RGBA8 编码 → 上传。
                for (uint32_t th : tileTextures_) {
                    if (th != 0u) device_->releaseTexture(th);
                }
                for (uint32_t lh : labelTextures_) {
                    if (lh != 0u) device_->releaseTexture(lh);
                }
                tileTextures_.clear();
                labelTextures_.clear();
                tileTextures_.reserve(frames.size());
                for (const auto& f : frames) {
                    const auto g = ringSource.requestHeights(scheme, f.key, 512);
                    uint32_t th = 0u;
                    if (g && g->width >= 514 && g->height >= 514) {
                        std::vector<double> cells(static_cast<size_t>(512) * 512);
                        for (int r = 0; r < 512; ++r) {
                            for (int c = 0; c < 512; ++c) {
                                cells[static_cast<size_t>(r) * 512 + c] =
                                    g->heights[static_cast<size_t>(r + 1) * g->width + (c + 1)];
                            }
                        }
                        const auto enc =
                            render::HeightTextureCodec::encode(cells, 512, 512);
                        if (enc.texture.valid()) {
                            th = device_->createTexture2D(enc.texture);
                        }
                    }
                    tileTextures_.push_back(th);
                }
                labelTextures_.assign(tileTextures_.size(), 0u);
            }
            ALOG("dem nasa band level=%d tiles=%zu foot=%d", level, frames.size(),
                 footOpt.has_value() ? 1 : 0);
            if (frames.empty() && assetManager_ != nullptr) {
                ALOGE("nasa tiles empty (net?) — 离线回退: adb shell setprop debug.mapc.src asset");
            }
        } else {
            DemAssetSource demSource(assetManager_);
            const int level = bandLevelForAltitudeMeters(camAltMeters_);
            const int demNodes = (level >= 13) ? 65 : 33; // 近景(L13)网格加密
            frames = buildDemFrames(scheme, demSource, bandRect, e, level, demNodes, 256);
            tileTextures_.clear();
            labelTextures_.clear();
            ALOG("dem asset band level=%d tiles=%zu foot=%d", level, frames.size(),
                 footOpt.has_value() ? 1 : 0);
        }
    } else {
        const FunctionalTerrainSource source;
        TerrainLodResult selection;
        if (footOpt) {
            const TerrainLodSelector selector;
            selection =
                selector.selectTiles(scheme, cameraPosCache_, footOpt.value(), lod, &frustum);
        }
        if (selection.tiles.empty()) {
            // 兜底：放宽阈值重选。
            TerrainLodConfig wide = lod;
            wide.maxScreenSpaceErrorPx = 32.0;
            const TerrainLodSelector selector;
            const Rectangle fallback = scheme.tileRectangleRadians(TileKey(0, 0, 0));
            selection = selector.selectTiles(scheme, cameraPosCache_, fallback, wide, nullptr);
        }
        const TerrainFrameAssembler assembler;
        const auto assembled = assembler.assemble(scheme, selection, source, e, 17, 16);
        for (const auto& frame : assembled) {
            frames.push_back(DemFrame{frame.key, frame.mesh});
        }
        tileTextures_.clear(); // 合成源无影像纹理
        labelTextures_.clear();
    }
    if (frames.empty()) {
        ALOGE("ensureGeometry: no frames assembled");
        return;
    }
    tilesDrawn_ = static_cast<long>(frames.size());

    // DrawList-lite：每瓦单独 RTC + 上传（账本到瓦级；逐瓦绘制见 drawFrame）。
    meshHandles_.clear();
    totalVertices_ = 0;
    unsigned int totalTriangles = 0;
    for (const auto& frame : frames) {
        if (useDisp_) {
            // GPU 位移属性通道：基准椭球面顶点 + 位移向量(顶点-基准) 分开放，
            // 顶点位移在 GPU 完成（绕开 vertex 纹理采样驱动限制）。
            const std::vector<Vec3>& positions = frame.mesh.positionsEcef;
            const std::vector<Vec3>& normals = frame.mesh.normals;
            if (positions.empty() || frame.mesh.indices.empty()) {
                continue;
            }
            render::MeshUploadData md;
            md.positions.reserve(positions.size() * 3);
            md.normals.reserve(normals.size() * 3);
            md.heights.reserve(positions.size());
            md.uvs.reserve(positions.size() * 2);
            md.displacements.reserve(positions.size() * 3);
            const Vec2 tOrigin = scheme.tileOriginMeters(frame.key);
            const Vec2 tSize = scheme.tileSizeMeters(frame.key.z());
            for (size_t i = 0; i < positions.size(); ++i) {
                const Cartographic c = e.cartesianToCartographic(positions[i]);
                const Vec3 base = e.cartographicToCartesian(
                    Cartographic(c.longitude(), c.latitude(), 0.0));
                const Vec3 delta = positions[i] - base; // 位移向量（属性）
                const Vec3 relBase = base - cameraPosCache_;
                md.positions.push_back(static_cast<float>(relBase.x()));
                md.positions.push_back(static_cast<float>(relBase.y()));
                md.positions.push_back(static_cast<float>(relBase.z()));
                md.normals.push_back(static_cast<float>(normals[i].x()));
                md.normals.push_back(static_cast<float>(normals[i].y()));
                md.normals.push_back(static_cast<float>(normals[i].z()));
                md.heights.push_back(static_cast<float>(c.height()));
                md.displacements.push_back(static_cast<float>(delta.x()));
                md.displacements.push_back(static_cast<float>(delta.y()));
                md.displacements.push_back(static_cast<float>(delta.z()));
                const Vec2 meters = scheme.projectToMeters(c);
                const double u = std::clamp((meters.x() - tOrigin.x()) / tSize.x(), 0.0, 1.0);
                const double v = std::clamp(1.0 - (meters.y() - tOrigin.y()) / tSize.y(), 0.0, 1.0);
                md.uvs.push_back(static_cast<float>(u));
                md.uvs.push_back(static_cast<float>(v));
            }
            md.indices = frame.mesh.indices;
            const uint32_t hd = device_->uploadMesh(md);
            if (hd != 0u) {
                meshHandles_.push_back(hd);
                totalVertices_ += static_cast<int>(positions.size());
                totalTriangles += static_cast<unsigned int>(frame.mesh.indices.size() / 3);
            }
            continue;
        }
        const std::vector<Vec3>& positions = frame.mesh.positionsEcef;
        const std::vector<Vec3>& normals = frame.mesh.normals;
        if (positions.empty() || frame.mesh.indices.empty()) {
            continue;
        }
        render::MeshUploadData md;
        md.positions.reserve(positions.size() * 3);
        md.normals.reserve(normals.size() * 3);
        md.heights.reserve(positions.size());
        md.uvs.reserve(positions.size() * 2);
        // UV = mercator 瓦内归一（u 西→东，v 北=0→顶；与影像瓦行序对齐）。
        const Vec2 tOrigin = scheme.tileOriginMeters(frame.key);
        const Vec2 tSize = scheme.tileSizeMeters(frame.key.z());
        for (size_t i = 0; i < positions.size(); ++i) {
            const Vec3 rel = positions[i] - cameraPosCache_;
            md.positions.push_back(static_cast<float>(rel.x()));
            md.positions.push_back(static_cast<float>(rel.y()));
            md.positions.push_back(static_cast<float>(rel.z()));
            md.normals.push_back(static_cast<float>(normals[i].x()));
            md.normals.push_back(static_cast<float>(normals[i].y()));
            md.normals.push_back(static_cast<float>(normals[i].z()));
            const Cartographic c = e.cartesianToCartographic(positions[i]);
            md.heights.push_back(static_cast<float>(c.height()));
            const Vec2 meters = scheme.projectToMeters(c);
            const double u = std::clamp((meters.x() - tOrigin.x()) / tSize.x(), 0.0, 1.0);
            const double v =
                std::clamp(1.0 - (meters.y() - tOrigin.y()) / tSize.y(), 0.0, 1.0);
            md.uvs.push_back(static_cast<float>(u));
            md.uvs.push_back(static_cast<float>(v));
        }
        md.indices = frame.mesh.indices; // 每瓦索引本瓦 0 基，无需平移
        const uint32_t h = device_->uploadMesh(md);
        if (h == 0) {
            ALOGE("ensureGeometry: uploadMesh failed for tile %s",
                  frame.key.toString().c_str());
            continue;
        }
        meshHandles_.push_back(h);
        totalVertices_ += static_cast<int>(positions.size());
        totalTriangles += static_cast<unsigned int>(frame.mesh.indices.size() / 3);
    }
    if (meshHandles_.empty()) {
        ALOGE("ensureGeometry: no tiles uploaded");
        geometryReady_ = false;
        return;
    }
    geometryReady_ = true;
    ALOG("geometry ready: tiles=%zu vertices=%d triangles=%u",
         meshHandles_.size(), totalVertices_, totalTriangles);
}

} // namespace demoscene