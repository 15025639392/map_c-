#pragma once

#include <android/asset_manager.h>

#include <memory>

#include <earth_engine/core/math/Vec3.h>
#include <earth_engine/renderer/IRenderDevice.h>

namespace demoscene {

/// A2/A3 场景：相机 → core 管线（选择/解码/网格）→ IRenderDevice(GLES3) 渲染。
/// 相机初始 = station 预设（debug.mapc.station 1..5），Java 手势可改写。
class TerrainScene {
public:
    void initializeGl();
    void resize(int widthPx, int heightPx);
    void setCamera(double lonDeg, double latDeg, double altMeters, double pitchDeg,
                   double headingDeg);
    void setAssetManager(AAssetManager* manager);
    void drawFrame();

private:
    void ensureGeometry();   // 相机→选择→装配，并把网格上传 GPU
    void destroyGlObjects();

    std::unique_ptr<earth_engine::render::IRenderDevice> device_;
    uint32_t programHandle_ = 0;
    uint32_t textureHandle_ = 0; // 合成"影像瓦"棋盘（纹理管线验证）
    // 每瓦一个设备网格句柄（DrawList-lite：逐瓦上传/绘制/账本到瓦级）。
    std::vector<uint32_t> meshHandles_;
    // NASA 模式的每瓦真实纹理（与 meshHandles_ 对齐；0 = 缺失回退棋盘）。
    std::vector<uint32_t> tileTextures_;
    bool geometryReady_ = false;
    int width_ = 1080;
    int height_ = 2400;
    int station_ = 2; // debug.mapc.station: 1=M-near 2=M-mid 3=M-graze 4=M-high 5=M-coarse
    long frameCount_ = 0;
    long tilesDrawn_ = 0;
    int totalVertices_ = 0;

    earth_engine::Vec3 cameraPosCache_;
    earth_engine::Vec3 cameraTargetCache_;
    earth_engine::Vec3 cameraUpCache_;

    // 活动相机（station 预设为初值；手势改）
    double camLonDeg_ = 106.44;
    double camLatDeg_ = 29.70;
    double camAltMeters_ = 15000.0;
    double camPitchDeg_ = 45.0;    // 相对地平线向下角（0=掠视 90=正下）
    double camHeadingDeg_ = 200.0; // 0=北 顺时针
    bool cameraUserSet_ = false;
    bool useDem_ = false; // debug.mapc.dem=1 → DEM（assets 或 NASA 网络源）
    AAssetManager* assetManager_ = nullptr;
    double lastKey_[5] = {0, 0, 0, 0, 0};
};

} // namespace demoscene
