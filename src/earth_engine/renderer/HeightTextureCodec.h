#pragma once

#include <cstdint>
#include <vector>

#include "IRenderDevice.h"

namespace earth_engine::render {

/// 每瓦高度纹理编码（GPU 位移/高度纹理路径的数据前提，L2 深化）。
///
/// 语义与口径（T-E1 记账延续；纹理=顶点数据之外的第 N 份字节，账要能查）：
/// - 输入：瓦内规则网格高度（行序 row0=北，w×h，椭球高米，double）；
/// - 输出：Texture2DData（RGBA8，w×h）——每像素按 **Terrain-RGB 全局绝对编码**
///   （h = -10000 + (R·65536+G·256+B)·0.1，量化步长 0.1m）。选全局绝对而非逐瓦
///   min/max 归一化的原因与本仓环形源同源纪律一致：同一物理高度在任何瓦解码
///   结果相同（跨瓦一致，无缝），且 CPU 查高/高度纹理可互验；
/// - 返回值附带该瓦 min/max（含 no-data？本核输入假设全有效；no-data 语义由上层
///   把黑（0,0,0）= -10000 记为哨兵——与 Terrain-RGB 隐式哨兵一致）；
/// - 回读：decodeHeight(pixel) 精确还原编码（浮点往返误差 ≤ 0.06m）。
struct HeightTextureStats {
    double minHeightMeters = 0.0;
    double maxHeightMeters = 0.0;
    size_t byteCount = 0; // w*h*4（账：每瓦高度纹理字节）
};

struct HeightTextureResult {
    Texture2DData texture;
    HeightTextureStats stats;
};

class HeightTextureCodec {
public:
    /// 编码（含 min/max 扫描与字节账）。
    /// @param heights row0=北、w×h；heights.size() 必须 == w*h。
    static HeightTextureResult encode(const std::vector<double>& heights, int w, int h);

    /// 回读（u/v 0..1 或像素坐标语义由采样层决定；这里给像素整数坐标）。
    static double decodeHeightAt(const Texture2DData& texture, int col, int row);
};

} // namespace earth_engine::render
