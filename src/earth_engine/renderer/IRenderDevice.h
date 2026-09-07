#pragma once

#include <cstdint>
#include <string>
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

/// 着色器程序源码（顶点/片元）。
struct ProgramSource {
    std::string vertexShader;
    std::string fragmentShader;

    bool valid() const { return !vertexShader.empty() && !fragmentShader.empty(); }
};

/// 渲染抽象（S1/L2；roadmap「RenderDevice 接口层，先 host 空实现+用例」）。
///
/// 目的：把 demo 的 GL 直写（VAO/VBO/EBO/program/uniform）收口到最小设备接口后，
/// 引擎核心不绑定具体图形 API；GLES3/Metal 实现本接口即解锁观感判据可测化（T-V*）。
/// 本文件**不含任何图形头**。接口语义（实现须遵守，host 替身钉口径）：
/// - 句柄：create*/upload* 返回 >0；失败/无效输入返回 0（空句柄）；release 防御幂等；
/// - 状态机：drawMesh 需要"已上传且未释放的网格句柄 + 当前已绑定的 program"，
///   否则为使用错误（实现应防御记录/断言，不崩溃）；
/// - uniform/视口/清屏为最近值语义（实现记录最后调用）。
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    // -- 网格 --
    virtual uint32_t uploadMesh(const MeshUploadData& mesh) = 0;
    virtual void releaseMesh(uint32_t handle) = 0;

    // -- 着色器程序 --
    virtual uint32_t createProgram(const ProgramSource& source) = 0;
    virtual void releaseProgram(uint32_t handle) = 0;
    virtual void useProgram(uint32_t handle) = 0;

    // -- uniform / 视口 / 清屏 --
    /// mat4 列主序 16 float（与引擎 Mat4 布局一致）。
    virtual void setUniformMat4(const char* name, const float* mat4x4) = 0;
    virtual void setUniformVec3(const char* name, float x, float y, float z) = 0;
    virtual void setViewport(int widthPx, int heightPx) = 0;
    virtual void clearColor(float r, float g, float b, float a) = 0;

    // -- 绘制 --
    virtual void drawMesh(uint32_t handle) = 0;
};

} // namespace earth_engine::render
