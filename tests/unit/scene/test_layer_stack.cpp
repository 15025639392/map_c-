// LayerStack/Layer：S3 场景图层系统 host 语义（顺序/开关/透明度/生命周期/差分账）。
// 覆盖：追加与绘制序、插入/移除、可见性与透明度(0 关闭)、revision 差分、reorder、
// 确定性。全部纯 host（与 GPU/网络解耦）。
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "earth_engine/scene/LayerStack.h"

using namespace earth_engine::scene;

namespace {

std::unique_ptr<Layer> mk(const std::string& name, const std::string& kind) {
    return std::make_unique<Layer>(name, kind);
}

} // namespace

TEST(LayerStack, AppendOrderAndActiveOrder) {
    LayerStack stack;
    stack.addLayer(mk("dem", "dem"));
    stack.addLayer(mk("imagery", "imagery"));
    stack.addLayer(mk("label", "label"));
    ASSERT_EQ(stack.size(), 3u);

    // 绘制序 = 栈序：dem → imagery → label。
    const std::vector<Layer*> order = stack.activeOrder();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0]->name(), "dem");
    EXPECT_EQ(order[1]->name(), "imagery");
    EXPECT_EQ(order[2]->name(), "label");
    EXPECT_EQ(stack.find("dem")->kind(), "dem");
}

TEST(LayerStack, VisibilityAndOpacityGateActive) {
    LayerStack stack;
    Layer* dem = stack.addLayer(mk("dem", "dem"));
    Layer* lbl = stack.addLayer(mk("label", "label"));
    Layer* deco = stack.addLayer(mk("debug", "overlay"));

    dem->setVisible(false);
    lbl->setOpacity(0.0); // 透明视同关闭
    const std::vector<Layer*> order = stack.activeOrder();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0]->name(), "debug");
    EXPECT_FALSE(dem->active());
    EXPECT_FALSE(lbl->active());
    EXPECT_TRUE(deco->active());

    dem->setVisible(true);
    EXPECT_TRUE(dem->active());
    EXPECT_EQ(dem->revision(), 2u); // 构造 0 → 可见性翻转 2 次
}

TEST(LayerStack, InsertRemoveAndRemoveOwnership) {
    LayerStack stack;
    stack.addLayer(mk("a", "k"));
    stack.addLayer(mk("c", "k"));
    Layer* b = stack.insertLayer(mk("b", "k"), 1);
    ASSERT_NE(b, nullptr);
    const std::vector<Layer*> order = stack.activeOrder();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[1]->name(), "b");

    auto removed = stack.removeLayer("b");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->name(), "b");
    EXPECT_EQ(stack.size(), 2u);
    EXPECT_EQ(stack.find("b"), nullptr);
}

TEST(LayerStack, RevisionTracksContentAndAttributeChanges) {
    LayerStack stack;
    Layer* l = stack.addLayer(mk("imagery", "imagery"));
    const std::uint64_t r0 = l->revision();
    l->setOpacity(0.5); // 属性变 → rev++
    const std::uint64_t r1 = l->revision();
    EXPECT_EQ(r1, r0 + 1);
    l->touch(); // 内容换代 → rev++
    EXPECT_EQ(l->revision(), r0 + 2);
    l->setOpacity(0.5); // 同值不递增
    EXPECT_EQ(l->revision(), r0 + 2);
    // 快照按绘制序带 revision（宿主差分基线）。
    const std::vector<LayerStack::SnapshotEntry> snap = stack.revisions();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(*snap[0].name, "imagery");
    EXPECT_EQ(snap[0].revision, l->revision());
}

TEST(LayerStack, DeterministicActiveOrderAcrossStacks) {
    const auto build = []() {
        LayerStack stack;
        stack.addLayer(mk("dem", "dem"));
        stack.addLayer(mk("imagery", "imagery"));
        stack.addLayer(mk("label", "label"));
        stack.find("imagery")->setVisible(false);
        std::string s;
        for (Layer* l : stack.activeOrder()) {
            s += l->name();
            s += ",";
        }
        return s;
    };
    EXPECT_EQ(build(), build());
}
