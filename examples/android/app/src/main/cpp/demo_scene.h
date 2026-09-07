#pragma once

#include <earth_engine/core/math/Vec3.h>

namespace demoscene {

/// A2/A3 场景：相机 → core 管线（选择/解码/网格）→ GLES 渲染。
/// 相机初始 = station 预设（debug.mapc.station 1/2/3），Java 手势可改写。
class TerrainScene {
public:
    void initializeGl();
    void resize(int widthPx, int heightPx);
    void setCamera(double lonDeg, double latDeg, double altMeters, double pitchDeg,
                   double headingDeg);
    void drawFrame();

private:
    void ensureGeometry();   // 相机→选择→装配，并把网格上传 GPU
    void destroyGlObjects();

    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vboPos_ = 0;
    unsigned int vboNor_ = 0;
    unsigned int vboHei_ = 0;
    unsigned int ebo_ = 0;
    unsigned int indexCount_ = 0;
    bool geometryReady_ = false;
    int width_ = 1080;
    int height_ = 2400;
    int station_ = 2; // debug.mapc.station: 1=M-near 2=M-mid 3=M-graze
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
    double lastKey_[5] = {0, 0, 0, 0, 0};
};

} // namespace demoscene
