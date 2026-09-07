#include "earth_engine/core/math/Mat4.h"

#include <algorithm>
#include <cmath>

namespace earth_engine {

Mat4 Mat4::transpose() const {
    Mat4 result;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            result.m_[r * 4 + c] = m_[c * 4 + r];
        }
    }
    return result;
}

bool Mat4::equalsEpsilon(const Mat4& rhs, double relativeEpsilon, double absoluteEpsilon) const {
    for (int i = 0; i < 16; ++i) {
        const double diff = std::fabs(m_[i] - rhs.m_[i]);
        const double bound =
            std::max(absoluteEpsilon, relativeEpsilon * std::max(std::fabs(m_[i]), std::fabs(rhs.m_[i])));
        if (diff > bound) {
            return false;
        }
    }
    return true;
}

Mat4 Mat4::inverse(bool* outOk) const {
    // 部分主元高斯-若尔当消元，作用于增广单位阵。
    double a[16];
    double inv[16] = {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    std::copy(m_, m_ + 16, a);

    const double kPivotFloor = 1.0e-14;
    for (int col = 0; col < 4; ++col) {
        // 选主元行
        int pivotRow = col;
        double maxAbs = std::fabs(a[col * 4 + col]);
        for (int row = col + 1; row < 4; ++row) {
            const double v = std::fabs(a[col * 4 + row]);
            if (v > maxAbs) {
                maxAbs = v;
                pivotRow = row;
            }
        }
        if (maxAbs < kPivotFloor) {
            if (outOk) {
                *outOk = false;
            }
            return identity();
        }
        if (pivotRow != col) {
            for (int c = 0; c < 4; ++c) {
                std::swap(a[c * 4 + col], a[c * 4 + pivotRow]);
                std::swap(inv[c * 4 + col], inv[c * 4 + pivotRow]);
            }
        }
        const double pivot = a[col * 4 + col];
        for (int c = 0; c < 4; ++c) {
            a[c * 4 + col] /= pivot;
            inv[c * 4 + col] /= pivot;
        }
        for (int row = 0; row < 4; ++row) {
            if (row == col) {
                continue;
            }
            const double factor = a[col * 4 + row];
            if (factor == 0.0) {
                continue;
            }
            for (int c = 0; c < 4; ++c) {
                a[c * 4 + row] -= factor * a[c * 4 + col];
                inv[c * 4 + row] -= factor * inv[c * 4 + col];
            }
        }
    }

    Mat4 result(inv[0], inv[1], inv[2], inv[3],
                inv[4], inv[5], inv[6], inv[7],
                inv[8], inv[9], inv[10], inv[11],
                inv[12], inv[13], inv[14], inv[15]);
    if (outOk) {
        *outOk = true;
    }
    return result;
}

} // namespace earth_engine
