#include <gtest/gtest.h>

#include "earth_engine/core/math/Mat4.h"

using namespace earth_engine;

namespace {


} // namespace

TEST(Mat4, IdentityAndAccess) {
    const Mat4 i = Mat4::identity();
    EXPECT_EQ(i.at(0, 0), 1.0);
    EXPECT_EQ(i.at(3, 3), 1.0);
    EXPECT_EQ(i.at(1, 0), 0.0);
    EXPECT_EQ(i.at(3, 0), 0.0);
    EXPECT_EQ(i.column(3), Vec3(0.0, 0.0, 0.0));
    // 单位阵作用不变。
    EXPECT_EQ(i.transformPoint(Vec3(1.0, 2.0, 3.0)), Vec3(1.0, 2.0, 3.0));
    EXPECT_EQ(i.transformDirection(Vec3(1.0, 2.0, 3.0)), Vec3(1.0, 2.0, 3.0));
}

TEST(Mat4, VectorColumnConstructor) {
    // 列主序约定验证：第 c 列由第 c 组 4 个标量组成。
    const Mat4 m(Vec3(1.0, 2.0, 3.0), Vec3(4.0, 5.0, 6.0),
                 Vec3(7.0, 8.0, 9.0), Vec3(10.0, 11.0, 12.0));
    EXPECT_EQ(m.at(0, 0), 1.0);
    EXPECT_EQ(m.at(0, 1), 2.0);
    EXPECT_EQ(m.at(0, 2), 3.0);
    EXPECT_EQ(m.at(0, 3), 0.0); // 隐含 w=0
    EXPECT_EQ(m.at(1, 2), 6.0);
    EXPECT_EQ(m.at(3, 0), 10.0);
    EXPECT_EQ(m.at(3, 3), 1.0); // 隐含 w=1
    EXPECT_EQ(m.translation(), Vec3(10.0, 11.0, 12.0));
}

TEST(Mat4, Translation) {
    const Mat4 t = Mat4::fromTranslation(Vec3(10.0, -20.0, 30.0));
    EXPECT_EQ(t.transformPoint(Vec3(1.0, 2.0, 3.0)), Vec3(11.0, -18.0, 33.0));
    // 平移不影响方向向量。
    EXPECT_EQ(t.transformDirection(Vec3(1.0, 2.0, 3.0)), Vec3(1.0, 2.0, 3.0));

    bool ok = false;
    const Mat4 inv = t.inverse(&ok);
    EXPECT_TRUE(ok);
    const Mat4 product = t * inv;
    EXPECT_TRUE(product.equalsEpsilon(Mat4::identity(), 1.0e-12));
    EXPECT_TRUE(inv.equalsEpsilon(Mat4::fromTranslation(Vec3(-10.0, 20.0, -30.0)), 1.0e-12));
}

TEST(Mat4, GeneralInverse) {
    // 用可逆整型矩阵（行列式非零）验证 A * A⁻¹ = I。
    // 列主序给定：col0=(1,0,0,0), col1=(0,2,0,1), col2=(0,0,3,0), col3=(1,1,1,4)…
    // 直接选一个简单可逆矩阵：[[2,0,0,0],[0,3,0,0],[0,0,4,0],[1,2,3,5]]（行主序）
    const Mat4 m(2.0, 0.0, 0.0, 0.0,   // col0（行主序第 0 列）
                 0.0, 3.0, 0.0, 0.0,   // col1
                 0.0, 0.0, 4.0, 0.0,   // col2
                 1.0, 2.0, 3.0, 5.0);  // col3
    bool ok = false;
    const Mat4 inv = m.inverse(&ok);
    ASSERT_TRUE(ok);
    const Mat4 product = m * inv;
    EXPECT_TRUE(product.equalsEpsilon(Mat4::identity(), 1.0e-9, 1.0e-9));

    // 再验证一次 A⁻¹ * A = I（左逆）。
    EXPECT_TRUE((inv * m).equalsEpsilon(Mat4::identity(), 1.0e-9, 1.0e-9));
}

TEST(Mat4, SingularInverseReportsFailure) {
    // 全零行 → 奇异。
    const Mat4 singular(1.0, 0.0, 0.0, 0.0,
                        0.0, 1.0, 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        0.0, 0.0, 0.0, 0.0);
    bool ok = true;
    (void)singular.inverse(&ok);
    EXPECT_FALSE(ok);
}

TEST(Mat4, Transpose) {
    const Mat4 m(1.0, 2.0, 3.0, 4.0,
                 5.0, 6.0, 7.0, 8.0,
                 9.0, 10.0, 11.0, 12.0,
                 13.0, 14.0, 15.0, 16.0);
    const Mat4 t = m.transpose();
    EXPECT_EQ(t.at(0, 1), m.at(1, 0));
    EXPECT_EQ(t.at(2, 3), m.at(3, 2));
    EXPECT_EQ(t.transpose(), m);
    // 两次转置 = 恒等。
    EXPECT_EQ(m.transpose().transpose(), m);
}

TEST(Mat4, MultiplyOrder) {
    // 先平移后旋转的顺序不可交换；验证 operator* 走标准矩阵乘法。
    const Mat4 t = Mat4::fromTranslation(Vec3(1.0, 0.0, 0.0));
    const Mat4 scale = Mat4(2.0, 0.0, 0.0, 0.0,
                            0.0, 2.0, 0.0, 0.0,
                            0.0, 0.0, 2.0, 0.0,
                            0.0, 0.0, 0.0, 1.0);
    const Vec3 p(1.0, 1.0, 1.0);
    // (T*S)(p) = T(S(p)) = (3,2,2)
    EXPECT_EQ((t * scale).transformPoint(p), Vec3(3.0, 2.0, 2.0));
    // (S*T)(p) = S(T(p)) = (4,2,2)
    EXPECT_EQ((scale * t).transformPoint(p), Vec3(4.0, 2.0, 2.0));
    // 结合律抽查。
    const Mat4 a = Mat4::fromTranslation(Vec3(1.0, 2.0, 3.0));
    const Mat4 b = scale;
    const Mat4 c = Mat4::fromTranslation(Vec3(0.0, 1.0, 0.0));
    EXPECT_EQ((a * b) * c, a * (b * c));
}

TEST(Mat4, EqualsEpsilon) {
    const Mat4 a = Mat4::fromTranslation(Vec3(1.0, 2.0, 3.0));
    const Mat4 b = Mat4::fromTranslation(Vec3(1.0 + 1.0e-10, 2.0, 3.0));
    EXPECT_TRUE(a.equalsEpsilon(b, 1.0e-6));
    EXPECT_FALSE(a.equalsEpsilon(b, 1.0e-12));
    EXPECT_NE(a, b);
}
