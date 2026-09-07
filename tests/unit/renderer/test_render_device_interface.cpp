// IRenderDevice 最小面语义（S1/L2 第一步）：接口 + host 测试替身——
// 上传/释放/绘制/视口的句柄与防御语义，供后续 GLES3/Metal 实现对齐。
#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <vector>

#include "earth_engine/renderer/IRenderDevice.h"

using namespace earth_engine::render;

namespace {

/// host 测试替身：记录调用；句柄防御（无效上传 → 0；绘制未上传/已释放句柄 →
/// badDraw 计数，不崩溃）——接口语义的口径由本替身钉住。
class HostTraceRenderDevice final : public IRenderDevice {
public:
    uint32_t uploadMesh(const MeshUploadData& mesh) override {
        ++uploadCount_;
        if (!mesh.valid()) {
            return 0; // 无效上传 → 空句柄
        }
        const uint32_t h = nextHandle_++;
        live_.insert(h);
        lastUploaded_ = h;
        return h;
    }
    void releaseMesh(uint32_t handle) override {
        ++releaseCount_;
        live_.erase(handle); // 已释放/不存在：静默防御
    }
    void drawMesh(uint32_t handle) override {
        ++drawCount_;
        lastDrawn_ = handle;
        if (handle == 0 || live_.count(handle) == 0) {
            ++badDraws_; // 防御：不崩溃，计数暴露使用错误
        }
    }
    void setViewport(int w, int h) override {
        ++viewportCount_;
        lastViewportW_ = w;
        lastViewportH_ = h;
    }

    size_t liveSizeForTest() const { return live_.size(); }

    int uploadCount_ = 0;
    int releaseCount_ = 0;
    int drawCount_ = 0;
    int viewportCount_ = 0;
    int badDraws_ = 0;
    uint32_t nextHandle_ = 1;
    uint32_t lastUploaded_ = 0;
    uint32_t lastDrawn_ = 0;
    int lastViewportW_ = 0;
    int lastViewportH_ = 0;

private:
    std::set<uint32_t> live_;
};

MeshUploadData makeMesh() {
    MeshUploadData m;
    m.positions = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    m.normals = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    m.indices = {0, 1, 2};
    return m;
}

} // namespace

TEST(RenderDevice, UploadReturnsIncreasingHandles) {
    HostTraceRenderDevice dev;
    const auto a = dev.uploadMesh(makeMesh());
    const auto b = dev.uploadMesh(makeMesh());
    EXPECT_EQ(a, 1u);
    EXPECT_EQ(b, 2u);
    EXPECT_EQ(dev.uploadCount_, 2);
}

TEST(RenderDevice, DrawAfterUploadRecordsHandle) {
    HostTraceRenderDevice dev;
    const auto h = dev.uploadMesh(makeMesh());
    dev.drawMesh(h);
    EXPECT_EQ(dev.drawCount_, 1);
    EXPECT_EQ(dev.lastDrawn_, h);
    EXPECT_EQ(dev.badDraws_, 0);
}

TEST(RenderDevice, DeadHandleDrawIsDefendedAndCounted) {
    HostTraceRenderDevice dev;
    const auto h = dev.uploadMesh(makeMesh());
    dev.releaseMesh(h);
    dev.drawMesh(h); // 已释放：防御计数，不崩溃
    EXPECT_EQ(dev.releaseCount_, 1);
    EXPECT_EQ(dev.drawCount_, 1);
    EXPECT_EQ(dev.badDraws_, 1);
    // 双释放防御。
    dev.releaseMesh(h);
    dev.releaseMesh(h);
    EXPECT_EQ(dev.releaseCount_, 3);
    EXPECT_EQ(dev.liveSizeForTest(), 0u);
    // 空句柄绘制同样防御。
    dev.drawMesh(0);
    EXPECT_EQ(dev.badDraws_, 2);
}

TEST(RenderDevice, ViewportSetsRecorded) {
    HostTraceRenderDevice dev;
    dev.setViewport(1080, 2400);
    EXPECT_EQ(dev.viewportCount_, 1);
    EXPECT_EQ(dev.lastViewportW_, 1080);
    EXPECT_EQ(dev.lastViewportH_, 2400);
}

TEST(RenderDevice, InvalidMeshUploadRejectedWithNullHandle) {
    MeshUploadData empty;
    EXPECT_FALSE(empty.valid());
    MeshUploadData badIndex = makeMesh();
    badIndex.indices.clear();
    EXPECT_FALSE(badIndex.valid());
    HostTraceRenderDevice dev;
    EXPECT_EQ(dev.uploadMesh(empty), 0u);
    EXPECT_EQ(dev.uploadMesh(badIndex), 0u);
    EXPECT_EQ(dev.uploadCount_, 2);
    EXPECT_EQ(dev.lastUploaded_, 0u);
}
