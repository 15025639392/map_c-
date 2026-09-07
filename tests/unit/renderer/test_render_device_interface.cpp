// IRenderDevice 最小面语义（S1/L2）：host 测试替身钉 网格/程序/绘制状态机 口径，
// 供后续 GLES3/Metal 实现对齐（释放防御、空句柄拒绝、缺程序绘制=使用错误）。
#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "earth_engine/renderer/IRenderDevice.h"

using namespace earth_engine::render;

namespace {

/// host 测试替身：记录调用；防御（无效输入 → 0；空/已释放句柄与缺程序绘制 →
/// badXxx 计数，不崩溃）。
class HostTraceRenderDevice final : public IRenderDevice {
public:
    uint32_t uploadMesh(const MeshUploadData& mesh) override {
        ++uploadCount_;
        if (!mesh.valid()) {
            return 0;
        }
        const uint32_t h = nextMesh_++;
        meshes_.insert(h);
        lastUploaded_ = h;
        return h;
    }
    void releaseMesh(uint32_t handle) override {
        ++releaseMeshCount_;
        meshes_.erase(handle);
    }
    uint32_t createProgram(const ProgramSource& source) override {
        ++createProgramCount_;
        if (!source.valid()) {
            return 0;
        }
        const uint32_t h = nextProgram_++;
        programs_.insert(h);
        lastCreatedProgram_ = h;
        return h;
    }
    void releaseProgram(uint32_t handle) override {
        ++releaseProgramCount_;
        programs_.erase(handle);
        if (boundProgram_ == handle) {
            boundProgram_ = 0;
        }
    }
    void useProgram(uint32_t handle) override {
        ++useProgramCount_;
        if (handle != 0 && programs_.count(handle) == 0) {
            ++badUseProgram_;
            boundProgram_ = 0;
            return;
        }
        boundProgram_ = handle;
    }
    void setUniformMat4(const char* name, const float* mat4x4) override {
        ++uniformMat4Count_;
        lastUniformMat4Name_ = name ? name : "";
        for (int i = 0; i < 16; ++i) {
            lastMat4_[i] = mat4x4[i];
        }
    }
    void setUniformVec3(const char* name, float x, float y, float z) override {
        ++uniformVec3Count_;
        lastUniformVec3Name_ = name ? name : "";
        lastVec3_[0] = x;
        lastVec3_[1] = y;
        lastVec3_[2] = z;
    }
    void setViewport(int w, int h) override {
        ++viewportCount_;
        lastViewportW_ = w;
        lastViewportH_ = h;
    }
    void clearColor(float r, float g, float b, float a) override {
        ++clearCount_;
        lastClear_[0] = r;
        lastClear_[1] = g;
        lastClear_[2] = b;
        lastClear_[3] = a;
    }
    void drawMesh(uint32_t handle) override {
        ++drawCount_;
        lastDrawn_ = handle;
        const bool meshOk = handle != 0 && meshes_.count(handle) == 1;
        const bool programOk = boundProgram_ != 0;
        if (!meshOk || !programOk) {
            ++badDraws_; // 使用错误防御：计数暴露，不崩溃
        }
    }

    size_t liveMeshCountForTest() const { return meshes_.size(); }
    size_t liveProgramCountForTest() const { return programs_.size(); }

    // 计数与最近值。
    int uploadCount_ = 0;
    int releaseMeshCount_ = 0;
    int createProgramCount_ = 0;
    int releaseProgramCount_ = 0;
    int useProgramCount_ = 0;
    int badUseProgram_ = 0;
    int uniformMat4Count_ = 0;
    int uniformVec3Count_ = 0;
    int viewportCount_ = 0;
    int clearCount_ = 0;
    int drawCount_ = 0;
    int badDraws_ = 0;
    uint32_t nextMesh_ = 1;
    uint32_t nextProgram_ = 1;
    uint32_t lastUploaded_ = 0;
    uint32_t lastCreatedProgram_ = 0;
    uint32_t boundProgram_ = 0;
    uint32_t lastDrawn_ = 0;
    int lastViewportW_ = 0;
    int lastViewportH_ = 0;
    float lastClear_[4] = {0, 0, 0, 0};
    float lastMat4_[16] = {0};
    float lastVec3_[3] = {0, 0, 0};
    std::string lastUniformMat4Name_;
    std::string lastUniformVec3Name_;

private:
    std::set<uint32_t> meshes_;
    std::set<uint32_t> programs_;
};

MeshUploadData makeMesh() {
    MeshUploadData m;
    m.positions = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    m.normals = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    m.indices = {0, 1, 2};
    return m;
}

ProgramSource makeProgram() {
    ProgramSource s;
    s.vertexShader = "void main(){}";
    s.fragmentShader = "void main(){}";
    return s;
}

} // namespace

TEST(RenderDevice, UploadAndProgramReturnIncreasingHandles) {
    HostTraceRenderDevice dev;
    EXPECT_EQ(dev.uploadMesh(makeMesh()), 1u);
    EXPECT_EQ(dev.uploadMesh(makeMesh()), 2u);
    EXPECT_EQ(dev.createProgram(makeProgram()), 1u);
    EXPECT_EQ(dev.createProgram(makeProgram()), 2u);
}

TEST(RenderDevice, DrawRequiresBoundProgramAndLiveMesh) {
    HostTraceRenderDevice dev;
    const auto m = dev.uploadMesh(makeMesh());
    const auto p = dev.createProgram(makeProgram());
    dev.drawMesh(m); // 未绑程序 → 使用错误
    EXPECT_EQ(dev.badDraws_, 1);
    dev.useProgram(p);
    dev.drawMesh(m); // 齐了 → ok
    EXPECT_EQ(dev.drawCount_, 2);
    EXPECT_EQ(dev.lastDrawn_, m);
    EXPECT_EQ(dev.badDraws_, 1);
}

TEST(RenderDevice, ReleaseMakesHandlesDeadAndDefensive) {
    HostTraceRenderDevice dev;
    const auto m = dev.uploadMesh(makeMesh());
    const auto p = dev.createProgram(makeProgram());
    dev.useProgram(p);
    dev.releaseMesh(m);
    dev.drawMesh(m); // 已释放网格 → 使用错误
    EXPECT_EQ(dev.badDraws_, 1);
    dev.releaseProgram(p);
    dev.useProgram(p); // 已释放程序 → 防御（绑定清零，badUse 计数）
    EXPECT_EQ(dev.badUseProgram_, 1);
    dev.releaseMesh(m); // 幂等释放不崩
    dev.releaseProgram(p);
    EXPECT_EQ(dev.liveMeshCountForTest(), 0u);
    EXPECT_EQ(dev.liveProgramCountForTest(), 0u);
}

TEST(RenderDevice, InvalidInputsRejectedWithNullHandles) {
    HostTraceRenderDevice dev;
    EXPECT_EQ(dev.uploadMesh(MeshUploadData()), 0u);
    EXPECT_EQ(dev.createProgram(ProgramSource()), 0u);
}

TEST(RenderDevice, UniformViewportClearRecorded) {
    HostTraceRenderDevice dev;
    const float m4[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    dev.setUniformMat4("uMVP", m4);
    dev.setUniformVec3("uLightDir", 0.5f, -0.5f, 1.0f);
    dev.setViewport(1080, 2400);
    dev.clearColor(0.42f, 0.55f, 0.74f, 1.0f);
    EXPECT_EQ(dev.uniformMat4Count_, 1);
    EXPECT_EQ(dev.lastUniformMat4Name_, "uMVP");
    EXPECT_EQ(dev.lastMat4_[0], 1.0f);
    EXPECT_EQ(dev.lastMat4_[15], 1.0f);
    EXPECT_EQ(dev.uniformVec3Count_, 1);
    EXPECT_EQ(dev.lastUniformVec3Name_, "uLightDir");
    EXPECT_EQ(dev.lastVec3_[2], 1.0f);
    EXPECT_EQ(dev.viewportCount_, 1);
    EXPECT_EQ(dev.lastViewportW_, 1080);
    EXPECT_EQ(dev.clearCount_, 1);
    EXPECT_EQ(dev.lastClear_[1], 0.55f);
}
