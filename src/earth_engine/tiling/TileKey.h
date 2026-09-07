#pragma once

#include <array>
#include <optional>
#include <string>

namespace earth_engine {

/// 瓦片键：z/x/y（z=层级，x/y 在 [0, 2^z) 范围，XYZ 约定 y 自北向南递增）。
/// 用于 Web Mercator / Geographic 正方形瓦片网格；四叉树结构由
/// parent()/children() 表达（0/0 为根，整层 2^z × 2^z 瓦片）。
/// 键语义纯数据：不含投影；与投影的换算在 WebMercatorTileScheme。
class TileKey {
public:
    /// 支持的层级上限（x/y 需要容纳 2^z，int 可表示的安全上界）。
    static constexpr int kMaxLevel = 29;

    /// 默认 = 根瓦片 (0/0/0)。
    constexpr TileKey() : z_(0), x_(0), y_(0) {}
    constexpr TileKey(int z, int x, int y) : z_(z), x_(x), y_(y) {}

    int z() const { return z_; }
    int x() const { return x_; }
    int y() const { return y_; }

    /// z 层每边瓦片数（2^z；z 越界返回 0）。
    static constexpr int tilesPerSide(int z) {
        return (z < 0 || z > 30) ? 0 : (1 << z);
    }

    bool isValid() const {
        const int n = tilesPerSide(z_);
        return n > 0 && x_ >= 0 && x_ < n && y_ >= 0 && y_ < n;
    }

    /// 父瓦片；根瓦片无父。
    std::optional<TileKey> parent() const {
        if (z_ == 0) {
            return std::nullopt;
        }
        return TileKey(z_ - 1, x_ / 2, y_ / 2);
    }

    /// 四个子瓦片（仅在 z_ < kMaxLevel 时有意义）。
    std::array<TileKey, 4> children() const {
        const int cx = x_ * 2;
        const int cy = y_ * 2;
        return {TileKey(z_ + 1, cx, cy), TileKey(z_ + 1, cx + 1, cy),
                TileKey(z_ + 1, cx, cy + 1), TileKey(z_ + 1, cx + 1, cy + 1)};
    }

    /// 沿父链向上第 `levels` 层（levels=0 返回自身）；超出根返回 nullopt。
    std::optional<TileKey> ancestor(int levels) const {
        TileKey k = *this;
        for (int i = 0; i < levels; ++i) {
            const auto p = k.parent();
            if (!p) {
                return std::nullopt;
            }
            k = *p;
        }
        return k;
    }

    bool operator==(const TileKey& rhs) const {
        return z_ == rhs.z_ && x_ == rhs.x_ && y_ == rhs.y_;
    }
    bool operator!=(const TileKey& rhs) const { return !(*this == rhs); }
    bool operator<(const TileKey& rhs) const {
        if (z_ != rhs.z_) {
            return z_ < rhs.z_;
        }
        if (y_ != rhs.y_) {
            return y_ < rhs.y_;
        }
        return x_ < rhs.x_;
    }

    /// "z/x/y"（与 XYZ URL 模板同序）。
    std::string toString() const {
        return std::to_string(z_) + "/" + std::to_string(x_) + "/" + std::to_string(y_);
    }

private:
    int z_;
    int x_;
    int y_;
};

} // namespace earth_engine

namespace std {

template <>
struct hash<earth_engine::TileKey> {
    size_t operator()(const earth_engine::TileKey& key) const noexcept {
        // 三个 int 的确定性混合（z 提升高位，避免 (x,y) 与 (y,x) 撞车）。
        size_t h = static_cast<size_t>(key.z());
        h = h * 73856093u ^ static_cast<size_t>(key.x());
        h = h * 19349663u ^ static_cast<size_t>(key.y());
        return h;
    }
};

} // namespace std
