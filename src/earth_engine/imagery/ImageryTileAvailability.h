#pragma once

#include <functional>
#include <optional>

#include "earth_engine/tiling/TileKey.h"

namespace earth_engine {

/// 影像可用性沿父链退化的最深层数上限（自身可用不算退化；超过此层仍无可用
/// 祖先则判定为空——以根瓦/0 冒充数据是禁止的"假数据"）。
inline constexpr int kMaxImageryFallbackLevels = 10;

/// 对单个请求瓦片的一次决议结果。
struct ImageryTileResolution {
    /// 最终应使用的瓦片键：可用自身或最近可用祖先；全链无可用祖先时为 nullopt。
    std::optional<TileKey> resolved;
    /// 决议键相对请求键的退化层数（0 = 自身可用；>0 = 降级到祖先的深度）。
    int fallbackDepth = 0;
    /// 是否决议到可用数据（resolved.has_value() 的镜像）。
    bool covered = false;

    explicit ImageryTileResolution(std::optional<TileKey> r = std::nullopt, int depth = 0)
        : resolved(r), fallbackDepth(depth), covered(r.has_value()) {}
};

/// 影像北极星：给定"源谓词"，对请求键沿 parent() 链求最近可用祖先键。
///
/// 决议语义：
///  - 从请求键自身开始，沿 parent() 链向上逐级调用 hasData，最多检查
///    kMaxImageryFallbackLevels 层（含自身），或直到 z=0 根瓦，取先到者；
///  - 自身可用 → resolved == 请求键、fallbackDepth == 0；
///  - 否则取最近可用祖先（fallbackDepth > 0）——退化到更低分辨率的真实祖先数据，
///    绝不空洞渲染、绝不用假数据填充；
///  - 检查范围内全无可用 → resolved == nullopt（明确"空"）。
///
/// 纯函数、无状态：数据到达使 hasData 由 false 变 true 后，对同一请求键再次调用
/// 即得新决议（如自身可用），无需缓存/状态对象。
///
/// @param hasData 源谓词：true 表示该键存在真实数据。
/// @param request 请求瓦片键。
/// @return 决议结果。
ImageryTileResolution resolveImageryTile(const std::function<bool(const TileKey&)>& hasData,
                                         const TileKey& request);

} // namespace earth_engine
