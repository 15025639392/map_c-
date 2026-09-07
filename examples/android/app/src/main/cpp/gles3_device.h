#pragma once

#include <unordered_map>

#include <GLES3/gl3.h>

#include <earth_engine/renderer/IRenderDevice.h>

namespace demoscene {

/// IRenderDevice 的 GLES3 实现（demo 目标内，用 NDK GLES3 头；host core 不依赖）。
/// 语义对齐 host 测试口径（render_device_interface）：
/// - 网格 = 1 VAO + pos/nor/hei 三个 VBO + EBO；attribute 0/1/2（aPos/aNor/aHei，
///   与 demo shader 布局一致）；heights 空时 attribute 2 用常量 0（顶点着色器只读
///   aHei 也可正常，但为防 shader 需要，绑 0 缓冲由实现决定：demo 总有高度）。
/// - 程序：编译/链接着色器；失败返回 0 并打日志；uniform location 懒缓存（名字→loc）；
/// - 绘制：需当前绑定 program + 存活网格（否则记录错误并跳过，不崩溃）。
class Gles3RenderDevice final : public earth_engine::render::IRenderDevice {
public:
    ~Gles3RenderDevice() override;

    earth_engine::render::DrawStats stats() const override { return stats_; }

    uint32_t uploadMesh(const earth_engine::render::MeshUploadData& mesh) override;
    void releaseMesh(uint32_t handle) override;
    uint32_t createTexture2D(const earth_engine::render::Texture2DData& data) override;
    void releaseTexture(uint32_t handle) override;
    void bindTexture2D(uint32_t unit, uint32_t handle) override;
    uint32_t createProgram(const earth_engine::render::ProgramSource& source) override;
    void releaseProgram(uint32_t handle) override;
    void useProgram(uint32_t handle) override;
    void setUniformMat4(const char* name, const float* mat4x4) override;
    void setUniformMat3(const char* name, const float* mat3x3) override;
    void setUniformVec3(const char* name, float x, float y, float z) override;
    void setUniformInt(const char* name, int value) override;
    void setViewport(int widthPx, int heightPx) override;
    void clearColor(float r, float g, float b, float a) override;
    void drawMesh(uint32_t handle) override;

private:
    struct Mesh {
        GLuint vao = 0;
        GLuint vboPos = 0;
        GLuint vboNor = 0;
        GLuint vboHei = 0;
        GLuint vboUv = 0;
        GLuint vboDisp = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
    };
    struct Texture {
        GLuint id = 0;
    };
    struct Program {
        GLuint id = 0;
        std::unordered_map<std::string, GLint> uniformLocs;
    };

    GLint uniformLocation(Program& prog, const char* name);

    earth_engine::render::DrawStats stats_;

    uint32_t nextMesh_ = 1;
    uint32_t nextTexture_ = 1;
    uint32_t nextProgram_ = 1;
    std::unordered_map<uint32_t, Mesh> meshes_;
    std::unordered_map<uint32_t, Texture> textures_;
    std::unordered_map<uint32_t, Program> programs_;
    uint32_t boundHandle_ = 0; // 当前绑定程序（句柄）
};

} // namespace demoscene
