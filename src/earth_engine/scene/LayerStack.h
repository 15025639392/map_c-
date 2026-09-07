#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace earth_engine {
namespace scene {

/// 场景图层（S3 host 语义；与 GPU 解耦，宿主实现负责持有渲染句柄）。
///
/// - 每层有身份（name）、内容类别（kind，如 "dem"/"imagery"/"label"/"overlay"）、
///   可见性、透明度（0 视同关闭）与生命周期 revision（内容/属性变化即递增，
///   宿主据此做增量上传差分，见 revision()）；
/// - 纯 host：不碰渲染/网络，只表达"图层元数据 + 变更账"，可测/确定性。
class Layer {
public:
    explicit Layer(std::string name, std::string kind = std::string());
    virtual ~Layer() = default;

    const std::string& name() const { return name_; }
    const std::string& kind() const { return kind_; }

    bool visible() const { return visible_; }
    void setVisible(bool visible) {
        if (visible != visible_) {
            visible_ = visible;
            ++revision_;
        }
    }

    /// [0,1]；0 = 完全透明（调度/绘制等同关闭）。
    double opacity() const { return opacity_; }
    void setOpacity(double opacity) {
        opacity = opacity < 0.0 ? 0.0 : (opacity > 1.0 ? 1.0 : opacity);
        if (opacity != opacity_) {
            opacity_ = opacity;
            ++revision_;
        }
    }

    /// 是否参与本轮调度/绘制（可见 且 透明度 > 0）。
    bool active() const { return visible_ && opacity_ > 0.0; }

    /// 内容/属性变更计数（宿主差分上传依据；只增不减）。
    std::uint64_t revision() const { return revision_; }
    /// 宿主标记内容变更（几何/纹理换代）。
    void touch() { ++revision_; }

private:
    std::string name_;
    std::string kind_;
    bool visible_ = true;
    double opacity_ = 1.0;
    std::uint64_t revision_ = 0;
};

/// 图层栈（S3）：有序图层容器，绘制序 = 栈序（前 → 后）。
///
/// - addLayer 追加到栈尾（后绘制）；insertLayer(name, index) 指定位置；
/// - 按名查询/开关/透明度；remove 释放并返回层指针（所有权交调用方）；
/// - activeOrder() 返回按绘制序排列的活动图层（跳过隐藏/透明层）——宿主每帧据此
///   绘制即可得到确定性顺序；revisions() 供宿主做逐层差分；
/// - 生命周期：beginFrame()/endFrame() 包围每帧调度（可计数断言配对使用）。
class LayerStack {
public:
    /// 追加（栈尾 = 最后绘制/最上层）。返回稳定指针（vector 扩容无效化注意）。
    Layer* addLayer(std::unique_ptr<Layer> layer);
    /// 插入到 index（0 = 最底）。返回指针；index 越界 → 追加。
    Layer* insertLayer(std::unique_ptr<Layer> layer, size_t index);
    /// 移出并返回（未找到 → nullptr）。所有权交调用方。
    std::unique_ptr<Layer> removeLayer(const std::string& name);

    Layer* find(const std::string& name);
    const Layer* find(const std::string& name) const;

    size_t size() const { return layers_.size(); }
    bool empty() const { return layers_.empty(); }
    void clear();

    /// 按绘制序返回活动图层指针（可见 && 透明度>0）。
    std::vector<Layer*> activeOrder();
    /// 按绘制序返回每层 (name, kind, revision) 快照（宿主差分基线）。
    struct SnapshotEntry {
        const std::string* name;
        const std::string* kind;
        std::uint64_t revision;
    };
    std::vector<SnapshotEntry> revisions() const;

    void beginFrame() { ++frame_; }
    void endFrame() { ++frame_; }
    std::uint64_t frame() const { return frame_; }

private:
    std::vector<std::unique_ptr<Layer>> layers_;
    std::uint64_t frame_ = 0;
};

} // namespace scene
} // namespace earth_engine
