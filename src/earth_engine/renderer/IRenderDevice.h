#pragma once

#include <cstdint>
#include <vector>

namespace earth_engine::render {

/// CPU 侧三角形网格上传描述：位置/法线（ECEF，三轴 float 数组，长度相同）+
/// 三角形索引（3 的倍数）。
struct MeshUploadData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<uint32_t> indices;

    bool valid() const {
        return positions.size() % 3 == 0 && normals.size() == positions.size() &&
               indices.size() % 3 == 0 && !indices.empty();
    }
};

/// 渲染抽象（S1 第一步，roadmap 阶段 2「RenderDevice 接口层，先 host 空实现+用例」）。
///
/// 目的：把 demo 的 GL 直写（VAO/VBO/EBO/shader）收口到最小设备接口后面，
/// 使引擎核心不绑定具体图形 API；后续以 GLES3/Metal 实现本接口即解锁观感判据
/// 的可测化（T-V*）。本文件**不含任何图形头**——接口只依赖引擎自有类型。
///
/// 最小面（按当前 demo 需求裁剪，随 S1 演进扩充：纹理/帧缓冲/管线状态后加）：
/// - MeshUploadData：CPU 侧三角形网格上传描述（位置/法线 ECEF float 三轴数组 +
///   索引；是否顶点色/高度通道等后续加）；
/// - IRenderDevice：设备抽象——上传网格得 opaque 句柄、释放、按句柄绘制、
///   视口设置。句柄语义（分配/复用/销毁后失效）由实现定义，接口只保证：
///   未 upload 的句柄 draw 为未定义行为（实现应防呆/可断言）。
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    /// 上传 CPU 网格 → 返回设备句柄（>0）。
    virtual uint32_t uploadMesh(const MeshUploadData& mesh) = 0;
    /// 释放句柄（此后不可 draw）。重复释放由实现防御。
    virtual void releaseMesh(uint32_t handle) = 0;
    /// 绘制已上传网格（drawIndexed）。
    virtual void drawMesh(uint32_t handle) = 0;
    /// 设置视口（像素）。
    virtual void setViewport(int widthPx, int heightPx) = 0;
};

} // namespace earth_engine::render
