#pragma once

#include <string>

#include "../tiling/TileKey.h"

namespace earth_engine {

/// 瓦片 URL 模板：{z}/{x}/{y} 占位替换（XYZ 顶层原点约定，与 scheme 一致）。
/// 示例模板："https://server.example/{z}/{x}/{y}.png"。
/// 将来需要 TMS（y 翻转）或子域 {s} 时在此扩展（{tms_y} / {s}），保持单一格式化点。
class TileUrlFormatter {
public:
    /// 未知占位符原样保留（便于排查），并返回是否替换成功。
    static std::string format(const std::string& urlTemplate, const TileKey& key);

    /// 是否含占位符。
    static bool hasPlaceholders(const std::string& urlTemplate);
};

} // namespace earth_engine
