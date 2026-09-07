#include "earth_engine/vector/VectorGrounding.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace earth_engine {
namespace vector {

namespace {

} // namespace

std::vector<VectorPoint> decodeGeoJsonPoints(const std::string& json) {
    std::vector<VectorPoint> points;
    // 找全部 "coordinates" 后跟的坐标数组数字；每个坐标数组 = 一个 Point/MultiPoint 几何。
    size_t pos = 0;
    const std::string needle = "coordinates";
    while (true) {
        const size_t hit = json.find(needle, pos);
        if (hit == std::string::npos) {
            break;
        }
        size_t open = json.find('[', hit);
        if (open == std::string::npos) {
            break;
        }
        // 只收集本 coordinates 数组（平衡括号内）的数字 token。
        std::vector<double> nums;
        {
            size_t i = open + 1;
            int depth = 1;
            const size_t n = json.size();
            while (i < n && depth > 0) {
                const char c = json[i];
                if (c == '[') {
                    ++depth;
                    ++i;
                } else if (c == ']') {
                    --depth;
                    ++i;
                } else if ((c >= '0' && c <= '9') || c == '-') {
                    char* end = nullptr;
                    const double v = std::strtod(json.c_str() + i, &end);
                    if (end != json.c_str() + i && std::isfinite(v)) {
                        nums.push_back(v);
                        i = static_cast<size_t>(end - json.c_str());
                        continue;
                    }
                    ++i;
                } else {
                    ++i;
                }
            }
        }
        if (nums.size() >= 2 && (nums.size() % 2) == 0) {
            for (size_t k = 0; k < nums.size(); k += 2) {
                VectorPoint p;
                p.lonRad = nums[k] * 0.017453292519943295; // deg → rad
                p.latRad = nums[k + 1] * 0.017453292519943295;
                points.push_back(p);
            }
        }
        pos = open + 1;
    }
    return points;
}

PointStyle styleForKey(const std::string& kind) {
    PointStyle s;
    if (kind == "road") {
        s.colorArgb = 0xFFF2E25B;
        s.widthPx = 3.0;
    } else if (kind == "poi") {
        s.colorArgb = 0xFF4FC3F7;
        s.widthPx = 6.0;
    }
    // 未知键 = 默认样式。
    return s;
}

std::optional<Vec3> VectorGrounding::projectToTerrain(const VectorPoint& point,
                                                      double offsetMeters) const {
    if (ground_ == nullptr || !std::isfinite(point.lonRad) || !std::isfinite(point.latRad) ||
        !std::isfinite(offsetMeters)) {
        return std::nullopt;
    }
    const Cartographic c(point.lonRad, point.latRad, 0.0);
    const std::optional<double> h = ground_(c);
    if (!h.has_value() || !std::isfinite(*h)) {
        return std::nullopt; // 无地表数据：不落点
    }
    // 贴地（+浮空 offset）：沿地表法向 = cartographicToCartesian(lon,lat,h) 语义。
    return ellipsoid_.cartographicToCartesian(
        Cartographic(point.lonRad, point.latRad, *h + offsetMeters));
}

} // namespace vector
} // namespace earth_engine
