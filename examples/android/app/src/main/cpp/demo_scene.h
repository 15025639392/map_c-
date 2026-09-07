#pragma once

#include <earth_engine/core/math/Vec3.h>

namespace demoscene {

/// A2 场景：固定相机（重庆 M-mid 型）→ core 管线（选择/解码/网格）→ GLES 渲染。
class TerrainScene {
public:
    void initializeGl();
    void resize(int widthPx, int heightPx);
    void drawFrame();

private:
    void ensureGeometry();   // 相机→选择→装配，并把首帧网格上传 GPU
    void destroyGlObjects();

    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vboPos_ = 0;
    unsigned int vboNor_ = 0;
    unsigned int ebo_ = 0;
    unsigned int indexCount_ = 0;
    bool geometryReady_ = false;
    int width_ = 1080;
    int height_ = 2400;
    long frameCount_ = 0;
    long tilesDrawn_ = 0;
    int totalVertices_ = 0;

    earth_engine::Vec3 cameraPosCache_;
    earth_engine::Vec3 cameraTargetCache_;
    earth_engine::Vec3 cameraUpCache_;
};

} // namespace demoscene
