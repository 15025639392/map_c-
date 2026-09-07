#pragma once

#include "Vec3.h"

namespace earth_engine {

/// 4x4 双精度矩阵，**列主序**存储（与 GL/glm 习惯一致）：
/// m_[col * 4 + row]。元素取值按列优先排列。
/// 用途：局部帧（ENU）→ ECEF 的刚体变换、视图/模型矩阵等。
class Mat4 {
public:
    Mat4() : Mat4(identity()) {}

    /// 以 16 个列主序标量构造。
    explicit Mat4(double m0, double m1, double m2, double m3,
                  double m4, double m5, double m6, double m7,
                  double m8, double m9, double m10, double m11,
                  double m12, double m13, double m14, double m15)
        : m_{m0, m1, m2, m3, m4, m5, m6, m7,
             m8, m9, m10, m11, m12, m13, m14, m15} {}

    /// 以 4 个基向量（每列含隐含 w）构造。
    explicit Mat4(const Vec3& col0, const Vec3& col1, const Vec3& col2, const Vec3& col3)
        : m_{col0.x(), col0.y(), col0.z(), 0.0,
             col1.x(), col1.y(), col1.z(), 0.0,
             col2.x(), col2.y(), col2.z(), 0.0,
             col3.x(), col3.y(), col3.z(), 1.0} {}

    static Mat4 identity() {
        return Mat4(1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0);
    }

    static Mat4 fromTranslation(const Vec3& t) {
        return Mat4(1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    t.x(), t.y(), t.z(), 1.0);
    }

    /// 行/列索引取值：at(col, row)。列主序。
    double at(int col, int row) const { return m_[col * 4 + row]; }
    double* data() { return m_; }
    const double* data() const { return m_; }

    /// 取第 col 列的前三维。
    Vec3 column(int col) const { return Vec3(m_[col * 4], m_[col * 4 + 1], m_[col * 4 + 2]); }
    Vec3 translation() const { return column(3); }

    /// 旋转块（去掉平移后）作用到方向向量：M·(v,0)。
    Vec3 transformDirection(const Vec3& v) const {
        return Vec3(m_[0] * v.x() + m_[4] * v.y() + m_[8] * v.z(),
                    m_[1] * v.x() + m_[5] * v.y() + m_[9] * v.z(),
                    m_[2] * v.x() + m_[6] * v.y() + m_[10] * v.z());
    }

    /// 完整仿射变换：M·(v,1)。
    Vec3 transformPoint(const Vec3& v) const {
        return Vec3(m_[0] * v.x() + m_[4] * v.y() + m_[8] * v.z() + m_[12],
                    m_[1] * v.x() + m_[5] * v.y() + m_[9] * v.z() + m_[13],
                    m_[2] * v.x() + m_[6] * v.y() + m_[10] * v.z() + m_[14]);
    }

    // ---- 组合 ----
    Mat4 operator*(const Mat4& rhs) const { return multiply(*this, rhs); }
    Mat4 transpose() const;
    /// 一般 4x4 逆（部分主元高斯消元）。奇异时返回 identity 并置 outOk=false。
    Mat4 inverse(bool* outOk = nullptr) const;

    bool operator==(const Mat4& rhs) const {
        for (int i = 0; i < 16; ++i) {
            if (m_[i] != rhs.m_[i]) {
                return false;
            }
        }
        return true;
    }
    bool operator!=(const Mat4& rhs) const { return !(*this == rhs); }

    bool equalsEpsilon(const Mat4& rhs, double relativeEpsilon, double absoluteEpsilon = 0.0) const;

    /// 标准矩阵乘法（左乘：result = a * b）。
    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 result;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) {
                    sum += a.m_[k * 4 + r] * b.m_[c * 4 + k];
                }
                result.m_[c * 4 + r] = sum;
            }
        }
        return result;
    }

private:
    double m_[16];
};

} // namespace earth_engine
