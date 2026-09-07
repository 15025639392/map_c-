#include <gtest/gtest.h>

#include <set>
#include <unordered_set>
#include <vector>

#include "earth_engine/tiling/TileKey.h"

using namespace earth_engine;

TEST(TileKey, DefaultIsRoot) {
    const TileKey root;
    EXPECT_EQ(root.z(), 0);
    EXPECT_EQ(root.x(), 0);
    EXPECT_EQ(root.y(), 0);
    EXPECT_TRUE(root.isValid());
    EXPECT_EQ(root.toString(), "0/0/0");
}

TEST(TileKey, Validity) {
    EXPECT_TRUE(TileKey(1, 1, 0).isValid());
    EXPECT_TRUE(TileKey(2, 3, 3).isValid());
    EXPECT_TRUE(TileKey(0, 0, 0).isValid());
    EXPECT_FALSE(TileKey(0, 1, 0).isValid()); // z0 只有 1 瓦
    EXPECT_FALSE(TileKey(1, 2, 0).isValid());
    EXPECT_FALSE(TileKey(1, 0, 2).isValid());
    EXPECT_FALSE(TileKey(-1, 0, 0).isValid());
    EXPECT_FALSE(TileKey(2, -1, 0).isValid());
    // z=2: x/y ∈ [0,4)
    EXPECT_TRUE(TileKey(2, 3, 3).isValid());
    EXPECT_FALSE(TileKey(2, 4, 0).isValid());
    // 大层级仍在 int 安全域。
    EXPECT_TRUE(TileKey(20, 123456, 654321).isValid());
    EXPECT_FALSE(TileKey(20, 1 << 20, 0).isValid());
}

TEST(TileKey, ToString) {
    EXPECT_EQ(TileKey(2, 1, 3).toString(), "2/1/3");
    EXPECT_EQ(TileKey(10, 512, 384).toString(), "10/512/384");
}

TEST(TileKey, ParentChildRelations) {
    // 根无父。
    EXPECT_FALSE(TileKey(0, 0, 0).parent().has_value());
    // 子→父 = 整除 2。
    EXPECT_EQ(TileKey(3, 5, 7).parent().value(), TileKey(2, 2, 3));
    // 父→四子覆盖且各自归父。
    const TileKey p(2, 1, 1);
    const auto children = p.children();
    ASSERT_EQ(children.size(), 4u);
    const TileKey expected[4] = {TileKey(3, 2, 2), TileKey(3, 3, 2),
                                 TileKey(3, 2, 3), TileKey(3, 3, 3)};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(children[static_cast<size_t>(i)], expected[i]) << "child " << i;
        EXPECT_EQ(children[static_cast<size_t>(i)].parent().value(), p);
    }
}

TEST(TileKey, AncestorChain) {
    const TileKey k(4, 9, 7);
    EXPECT_EQ(k.ancestor(0).value(), k);
    EXPECT_EQ(k.ancestor(1).value(), TileKey(3, 4, 3));
    EXPECT_EQ(k.ancestor(2).value(), TileKey(2, 2, 1));
    EXPECT_EQ(k.ancestor(4).value(), TileKey(0, 0, 0));
    EXPECT_FALSE(k.ancestor(5).has_value());
}

TEST(TileKey, OrderingAndContainers) {
    // operator<：z 优先，其次 y（行优先），最后 x。
    EXPECT_LT(TileKey(1, 0, 0), TileKey(2, 0, 0));
    EXPECT_LT(TileKey(2, 1, 0), TileKey(2, 0, 1)); // 同行（y=0）优先于 y=1
    EXPECT_LT(TileKey(2, 0, 1), TileKey(2, 1, 1)); // 同 y 再比 x
    EXPECT_TRUE(TileKey(2, 1, 0) == TileKey(2, 1, 0));

    std::set<TileKey> s;
    s.insert(TileKey(3, 5, 7));
    s.insert(TileKey(2, 1, 1));
    s.insert(TileKey(3, 5, 7));
    EXPECT_EQ(s.size(), 2u);

    // 哈希容器：等键同桶、查找正确。
    std::unordered_set<TileKey> us;
    constexpr int kCount = 64;
    for (int i = 0; i < kCount; ++i) {
        us.insert(TileKey(6, (i * 3) % 64, i % 64));
    }
    EXPECT_EQ(us.size(), kCount);
    // 已插入键（i=17 → x=51, y=17）。
    EXPECT_NE(us.find(TileKey(6, 51, 17)), us.end());
    // 未插入键（y=2 行 x=5 不在序列里）。
    EXPECT_EQ(us.find(TileKey(6, 5, 2)), us.end());

    // 排序向量一致性。
    std::vector<TileKey> v = {TileKey(5, 1, 2), TileKey(5, 2, 1)};
    std::sort(v.begin(), v.end());
    EXPECT_LT(v[0], v[1]); // y=1 行在前
    EXPECT_EQ(v[0], TileKey(5, 2, 1));
}
