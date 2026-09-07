#include "earth_engine/content/HeightmapTile.h"

#include <algorithm>
#include <limits>

namespace earth_engine {

namespace {

// mercator 米比较的容差（相对瓦片尺寸）。
constexpr double kEdgeSlop = 1.0e-9;
// OpenGlobus RgbTerrain.checkNoDataValue：> 50000 一律视为 no-data
// （镜像 gis-md isNoData；仅哨兵路径生效）。
constexpr double kNoDataHeightThreshold = 50000.0;

} // namespace

HeightmapTile::HeightmapTile(const WebMercatorTileScheme& scheme, const TileKey& key,
                             const double* heights, int width, int height,
                             const double* noDataValues, int noDataCount)
    : scheme_(&scheme),
      key_(key),
      heights_(heights),
      width_(width),
      height_(height),
      noDataValues_(noDataValues),
      noDataCount_(noDataCount) {}

Rectangle HeightmapTile::coverageRadians() const {
    return scheme_->tileRectangleRadians(key_);
}

bool HeightmapTile::contains(const Cartographic& cartographic) const {
    // Web Mercator 纬度上限外的点不可能在本瓦片内（瓦片不覆盖极区）。
    if (!(cartographic.latitude() >= -WebMercatorProjection::maximumLatitudeRadians() &&
          cartographic.latitude() <= WebMercatorProjection::maximumLatitudeRadians())) {
        return false;
    }
    const Vec2 meters = scheme_->projectToMeters(cartographic);
    const Vec2 origin = scheme_->tileOriginMeters(key_);
    const Vec2 size = scheme_->tileSizeMeters(key_.z());
    const double slopX = kEdgeSlop * size.x();
    const double slopY = kEdgeSlop * size.y();
    return meters.x() >= origin.x() - slopX && meters.x() <= origin.x() + size.x() + slopX &&
           meters.y() >= origin.y() - slopY && meters.y() <= origin.y() + size.y() + slopY;
}

std::optional<Vec2> HeightmapTile::cartographicToPixel(const Cartographic& cartographic) const {
    if (!contains(cartographic)) {
        return std::nullopt;
    }
    const Vec2 meters = scheme_->projectToMeters(cartographic);
    const Vec2 origin = scheme_->tileOriginMeters(key_);
    const Vec2 size = scheme_->tileSizeMeters(key_.z());
    const double fx = (meters.x() - origin.x()) / size.x(); // [0,1]，西→东
    const double fy = (meters.y() - origin.y()) / size.y(); // [0,1]，南→北
    // row 0 = 北：像素 row 与 fy 反向。
    const double col = fx * static_cast<double>(width_ - 1);
    const double row = (1.0 - fy) * static_cast<double>(height_ - 1);
    return Vec2(col, row);
}

Cartographic HeightmapTile::pixelToCartographic(double col, double row) const {
    const Vec2 origin = scheme_->tileOriginMeters(key_);
    const Vec2 size = scheme_->tileSizeMeters(key_.z());
    const double fx =
        width_ > 1
            ? HeightmapSampler::clampToIndex(col, width_ - 1) / static_cast<double>(width_ - 1)
            : 0.0;
    const double fy =
        height_ > 1
            ? 1.0 - HeightmapSampler::clampToIndex(row, height_ - 1) / static_cast<double>(height_ - 1)
            : 0.0;
    const double mx = origin.x() + fx * size.x();
    const double my = origin.y() + fy * size.y();
    return scheme_->unprojectMeters(Vec2(mx, my));
}

std::optional<double> HeightmapTile::sampleHeightAt(const Cartographic& cartographic) const {
    const std::optional<Vec2> pixel = cartographicToPixel(cartographic);
    if (!pixel) {
        return std::nullopt;
    }
    const HeightmapSampler sampler(heights_, width_, height_, noDataValues_, noDataCount_);
    return sampler.sampleBilinear(pixel->x(), pixel->y());
}

bool HeightmapTile::isNoDataValue(double height) const {
    if (height > kNoDataHeightThreshold) {
        return true;
    }
    for (int i = 0; i < noDataCount_; ++i) {
        if (height == noDataValues_[i]) {
            return true;
        }
    }
    return false;
}

std::pair<double, double> HeightmapTile::minMaxHeight() const {
    const int count = width_ * height_;
    if (noDataCount_ <= 0) {
        // 无哨兵：保持既有全量扫描（结果不变）。
        double minH = heights_[0];
        double maxH = heights_[0];
        for (int i = 1; i < count; ++i) {
            minH = std::min(minH, heights_[i]);
            maxH = std::max(maxH, heights_[i]);
        }
        return {minH, maxH};
    }
    // 有哨兵：min/max 只统计有效高度（no-data 混进来会把包围体/量化原点拉偏；
    // 镜像 gis-md assignHeights —— min/max 是包围体与几何误差的输入，T-P13 教训）。
    double minH = std::numeric_limits<double>::max();
    double maxH = std::numeric_limits<double>::lowest();
    for (int i = 0; i < count; ++i) {
        if (isNoDataValue(heights_[i])) {
            continue;
        }
        minH = std::min(minH, heights_[i]);
        maxH = std::max(maxH, heights_[i]);
    }
    if (minH > maxH) {
        return {0.0, 0.0}; // 全哨兵：无有效高度
    }
    return {minH, maxH};
}

} // namespace earth_engine
