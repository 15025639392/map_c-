#pragma once

#include <vector>

#include "Cartographic.h"

namespace earth_engine {

/// 高程基准改正：把 DEM 高度（正高/大地水准面起算）改正为椭球高所需的
/// undulation（椭球高 = 正高 + undulation）。默认恒等（undulation=0）。
/// 真实 EGM96 网格可经 GridHeightDatumCorrector 接入；本仓渲染默认不启用
/// （见 engine-targets §6：已知偏差，显式记录）。
class IHeightDatumCorrector {
public:
    virtual ~IHeightDatumCorrector() = default;

    /// 返回该经纬处的 undulation（米）。经纬单位弧度。
    virtual double undulationMeters(const Cartographic& cartographic) const = 0;
};

/// 恒等改正（undulation ≡ 0）。
class IdentityHeightDatumCorrector : public IHeightDatumCorrector {
public:
    double undulationMeters(const Cartographic&) const override { return 0.0; }
};

/// 规则经纬网格 undulation 表（双线性插值，边缘外推钳制到网格边）。
/// grid 布局：行 = 纬度（自 south 向北），行内 = 经度（自 west 向东）；
/// 网格点间距 cellDegrees。
class GridHeightDatumCorrector : public IHeightDatumCorrector {
public:
    /// westDeg/southDeg = 网格西南角；lonCount/latCount ≥ 1。
    GridHeightDatumCorrector(double westDeg, double southDeg, double cellDegrees,
                             int lonCount, int latCount, const double* gridMeters);

    double undulationMeters(const Cartographic& cartographic) const override;

    bool empty() const { return values_.empty(); }

private:
    double westDeg_;
    double southDeg_;
    double cellDegrees_;
    int lonCount_;
    int latCount_;
    std::vector<double> values_;
};

} // namespace earth_engine
