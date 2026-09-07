#pragma once

#include <optional>
#include <string>
#include <vector>

namespace demoscene {

/// 同步 HTTP(S) GET（Android demo 专用）：经 JNI 调 Java NativeRenderer.httpGetBytes
/// （HttpURLConnection + 系统 TLS）。非 200 / 异常 → nullopt。
std::optional<std::vector<uint8_t>> httpGetBytes(const std::string& url);

} // namespace demoscene
