#include "demo_scene.h"

#include "dem_assets.h"

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
#include <earth_engine/content/TerrainDataSource.h>
#include <earth_engine/content/TerrainFrameAssembler.h>
#include <earth_engine/core/geodesy/Ellipsoid.h>
#include <earth_engine/core/math/Mat4.h>
#include <earth_engine/core/math/MathUtils.h>
#include <earth_engine/tiling/TerrainLodSelector.h>
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
unsigned int compileShader(GLenum type, const char* src, const char* tag) {
    const unsigned int sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    int ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        ALOGE("shader compile failed (%s): %s", tag, log);
    }
    return sh;
}

unsigned int buildProgram(const char* vsSrc, const char* fsSrc) {
    const unsigned int vs = compileShader(GL_VERTEX_SHADER, vsSrc, "vert");
    const unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fsSrc, "frag");
    const unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        ALOGE("program link failed: %s", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

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
    "out vec3 vNor;\n"
    "out float vHeight;\n"
    "void main() {\n"
    "  vNor = uViewRot * aNor;\n"
    "  vHeight = aHei;\n"
    "  gl_Position = uMvp * vec4(aPos, 1.0);\n"
    "}\n";

const char* kFs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec3 vNor;\n"
    "out vec4 fragColor;\n"
    "uniform vec3 uLightDir;\n"
    "uniform vec3 uBaseColor;\n"
    "in float vHeight;\n"
    "void main() {\n"
    "  vec3 n = normalize(vNor);\n"
    "  vec3 l = normalize(uLightDir);\n"
    "  float d = max(dot(n, l), 0.0);\n"
    "  float t = clamp((vHeight - (-1000.0)) / 6000.0, 0.0, 1.0);\n"
    "  vec3 low  = vec3(0.35, 0.48, 0.25);\n"
    "  vec3 mid  = vec3(0.48, 0.43, 0.30);\n"
    "  vec3 high = vec3(0.66, 0.52, 0.34);\n"
    "  vec3 tint = t < 0.5 ? mix(low, mid, t * 2.0) : mix(mid, high, (t - 0.5) * 2.0);\n"
    "  vec3 c = tint * (0.42 + 0.9 * d * d);\n"
    "  fragColor = vec4(c, 1.0);\n"
    "}\n";

} // namespace

// ---------------------------------------------------------------------------
// TerrainScene
// ---------------------------------------------------------------------------
void TerrainScene::destroyGlObjects() {
    if (program_) glDeleteProgram(program_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vboPos_) glDeleteBuffers(1, &vboPos_);
    if (vboNor_) glDeleteBuffers(1, &vboNor_);
    if (vboHei_) glDeleteBuffers(1, &vboHei_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    program_ = vao_ = vboPos_ = vboNor_ = ebo_ = 0;
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
    ALOG("dem mode=%d (assetMgr=%d)", useDem_ ? 1 : 0, assetManager_ != nullptr ? 1 : 0);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.42f, 0.55f, 0.74f, 1.0f); // 天蓝
    program_ = buildProgram(kVs, kFs);
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vboPos_);
    glGenBuffers(1, &vboNor_);
    glGenBuffers(1, &vboHei_);
    glGenBuffers(1, &ebo_);
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
    if (!geometryReady_ || program_ == 0 || indexCount_ == 0) {
        if (frameCount_ % 120 == 1) {
            ALOG("draw frame=%ld (no geometry yet)", static_cast<long>(frameCount_));
        }
        return;
    }
    // 方形视口（aspect=1 相机），居中。
    const int side = std::min(width_, height_);
    glViewport((width_ - side) / 2, (height_ - side) / 2, side, side);

    glUseProgram(program_);
    glBindVertexArray(vao_);

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
    glUniformMatrix4fv(glGetUniformLocation(program_, "uMvp"), 1, GL_FALSE, m);
    // 视图旋转 3x3（列主序）。
    float vr[9];
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            vr[c * 3 + r] = static_cast<float>(view.at(c, r));
        }
    }
    glUniformMatrix3fv(glGetUniformLocation(program_, "uViewRot"), 1, GL_FALSE, vr);
    glUniform3f(glGetUniformLocation(program_, "uLightDir"), 0.25f, 0.55f, 0.80f);
    glUniform3f(glGetUniformLocation(program_, "uBaseColor"), 0.55f, 0.45f, 0.30f);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    if (frameCount_ == 1) {
        const GLenum err = glGetError();
        const int umvp = glGetUniformLocation(program_, "uMvp");
        const int uview = glGetUniformLocation(program_, "uViewRot");
        const int ulight = glGetUniformLocation(program_, "uLightDir");
        const int ucolor = glGetUniformLocation(program_, "uBaseColor");
        ALOG("draw debug: glErr=0x%x uniform locs mvp=%d view=%d light=%d color=%d", err,
             umvp, uview, ulight, ucolor);
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
            NasaRingDemSource demSource; // 字节源 + 引擎环模式源（组合体）
            int level = bandLevelForAltitudeMeters(camAltMeters_);
            if (level > 12) {
                level = 12;
            }
            if (level < 6) {
                level = 6;
            }
            const int demNodes = (level >= 12) ? 65 : 33;
            frames = buildDemFrames(scheme, demSource.source, bandRect, e, level, demNodes,
                                    /*requestCells=*/512);
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
    }
    if (frames.empty()) {
        ALOGE("ensureGeometry: no frames assembled");
        return;
    }
    tilesDrawn_ = static_cast<long>(frames.size());

    // 汇总全部顶点（RTC 到相机），打包 pos/normal/index。
    std::vector<float> pos;
    std::vector<float> nor;
    std::vector<float> hei;
    std::vector<uint32_t> idx;
    size_t base = 0;
    for (const auto& frame : frames) {
        const std::vector<Vec3>& positions = frame.mesh.positionsEcef;
        const std::vector<Vec3>& normals = frame.mesh.normals;
        for (size_t i = 0; i < positions.size(); ++i) {
            const Vec3 rel = positions[i] - cameraPosCache_;
            pos.push_back(static_cast<float>(rel.x()));
            pos.push_back(static_cast<float>(rel.y()));
            pos.push_back(static_cast<float>(rel.z()));
            nor.push_back(static_cast<float>(normals[i].x()));
            nor.push_back(static_cast<float>(normals[i].y()));
            nor.push_back(static_cast<float>(normals[i].z()));
            const Cartographic c = e.cartesianToCartographic(positions[i]);
            hei.push_back(static_cast<float>(c.height()));
        }
        for (uint32_t index : frame.mesh.indices) {
            idx.push_back(index + static_cast<uint32_t>(base));
        }
        base += positions.size();
    }
    totalVertices_ = static_cast<int>(pos.size() / 3);
    indexCount_ = static_cast<unsigned int>(idx.size());
    if (pos.empty() || idx.empty()) {
        ALOGE("ensureGeometry: empty buffers");
        return;
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vboPos_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(pos.size() * sizeof(float)),
                 pos.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vboNor_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(nor.size() * sizeof(float)),
                 nor.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vboHei_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(hei.size() * sizeof(float)),
                 hei.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(uint32_t)),
                 idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    geometryReady_ = true;
    ALOG("geometry ready: tiles=%ld vertices=%d triangles=%u", static_cast<long>(tilesDrawn_),
         totalVertices_, indexCount_ / 3);
}

} // namespace demoscene
