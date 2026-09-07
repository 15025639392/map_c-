#include "earth_engine/providers/TileUrlFormatter.h"

#include <string>

namespace earth_engine {

namespace {

void replaceAllInPlace(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

} // namespace

std::string TileUrlFormatter::format(const std::string& urlTemplate, const TileKey& key) {
    std::string out = urlTemplate;
    replaceAllInPlace(out, "{z}", std::to_string(key.z()));
    replaceAllInPlace(out, "{x}", std::to_string(key.x()));
    replaceAllInPlace(out, "{y}", std::to_string(key.y()));
    return out;
}

bool TileUrlFormatter::hasPlaceholders(const std::string& urlTemplate) {
    return urlTemplate.find("{") != std::string::npos;
}

} // namespace earth_engine
