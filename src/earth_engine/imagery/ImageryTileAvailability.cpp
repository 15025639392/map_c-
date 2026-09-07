#include "earth_engine/imagery/ImageryTileAvailability.h"

namespace earth_engine {

ImageryTileResolution resolveImageryTile(const std::function<bool(const TileKey&)>& hasData,
                                         const TileKey& request) {
    if (hasData(request)) {
        return ImageryTileResolution(request, 0);
    }

    // 自身不可用：沿父链找最近可用祖先。
    // 最多 kMaxImageryFallbackLevels 层退化（自身已查过一次，这里至多再走
    // kMaxImageryFallbackLevels 层，含根瓦 z=0 后 parent() 自然为 nullopt 终止）。
    TileKey cur = request;
    for (int depth = 1; depth <= kMaxImageryFallbackLevels; ++depth) {
        const std::optional<TileKey> parent = cur.parent();
        if (!parent) {
            break; // 已到 z=0 根瓦且根瓦不可用（上一层循环已判过），链尽。
        }
        cur = *parent;
        if (hasData(cur)) {
            return ImageryTileResolution(cur, depth);
        }
    }

    // 检查范围内（含退化上限层内的根瓦）全无真实数据：明确为空，绝不冒充。
    return ImageryTileResolution();
}

} // namespace earth_engine
