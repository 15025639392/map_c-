#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace earth_engine::render {

/// CPU 侧三角形网格上传描述：位置/法线（ECEF，三轴 float 数组，长度相同）+
/// 可选逐顶点高度通道（数量 = 顶点数；空 = 无高度通道，实现按 0 处理）+
/// 三角形索引（3 的倍数）。
struct MeshUploadData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> heights; // 可选：长度 == positions/3 或为空
    std::vector<uint32_t> indices;

    bool valid() const {
        return positions.size() % 3 == 0 && normals.size() == positions.size() &&
               indices.size() % 3 == 0 && !indices.empty() &&
               (heights.empty() || heights.size() == positions.size() / 3);
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
/// 2D 纹理上传描述（RGBA8，逐行，首行=顶；宽高 ≥1）。
struct Texture2DData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba8;

    bool valid() const {
        return width > 0 && height > 0 &&
               rgba8.size() == static_cast<size_t>(width) * height * 4;
    }
};

/// 渲染统计（H2 性能记账第一笔账：北极星"每字节/每三角形有账"）。
struct DrawStats {
    uint64_t drawCalls = 0;      // 累计 drawMesh 成功次数
    uint64_t trianglesDrawn = 0; // 累计绘制三角形数
    uint64_t uploadedBytes = 0;  // 累计上传字节（position+normal+height+index+texture）
    uint32_t liveMeshes = 0;     // 当前驻留网格数（资源账）
    uint32_t liveTextures = 0;   // 当前驻留纹理数（资源账）
};

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    /// 当前累计渲染/资源统计（实现持续维护；测试与性能账本用）。
    virtual DrawStats stats() const = 0;

    // -- 网格 --
    virtual uint32_t uploadMesh(const MeshUploadData& mesh) = 0;
    virtual void releaseMesh(uint32_t handle) = 0;

    // -- 2D 纹理（RGBA8；影像瓦 / 高度纹理的通道） --
    virtual uint32_t createTexture2D(const Texture2DData& data) = 0;
    virtual void releaseTexture(uint32_t handle) = 0;
    /// 绑定纹理到采样单元（unit 0..15）；0 = 解绑该单元。
    virtual void bindTexture2D(uint32_t unit, uint32_t handle) = 0;

    // -- 着色器程序 --
    virtual uint32_t createProgram(const ProgramSource& source) = 0;
    virtual void releaseProgram(uint32_t handle) = 0;
    virtual void useProgram(uint32_t handle) = 0;

    // -- uniform / 视口 / 清屏 --
    /// mat4 列主序 16 float（与引擎 Mat4 布局一致）。
    virtual void setUniformMat4(const char* name, const float* mat4x4) = 0;
    /// mat3（列主序 9 float，法线旋转等）。
    virtual void setUniformMat3(const char* name, const float* mat3x3) = 0;
    virtual void setUniformVec3(const char* name, float x, float y, float z) = 0;
    /// 整数 uniform（纹理 sampler 等）。
    virtual void setUniformInt(const char* name, int value) = 0;
    virtual void setViewport(int widthPx, int heightPx) = 0;
    virtual void clearColor(float r, float g, float b, float a) = 0;

    // -- 绘制 --
    virtual void drawMesh(uint32_t handle) = 0;
};

} // namespace earth_engine::render
