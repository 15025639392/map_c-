// VectorGrounding/decodeGeoJsonPoints/styleForKey：矢量最小切片（S5 host 先行）。
// 覆盖：GeoJSON 点/MultiPoint 子集解码（度→弧度）、坐标数组括号边界隔离、
// 贴地投影（groundFn 地表椭球高 + 浮空 offset → ECEF；无地表数据 → nullopt）、
// 样式键映射确定性。全部纯 host。
#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/geodesy/Ellipsoid.h"
#include "earth_engine/core/math/MathUtils.h"
#include "earth_engine/vector/VectorGrounding.h"

using namespace earth_engine;
using namespace earth_engine::vector;

namespace {

const char* kMultiPointFixture =
    R"({"type":"FeatureCollection","features":[
       {"type":"Feature","properties":{"kind":"poi"},
        "geometry":{"type":"MultiPoint","coordinates":[[106.44,29.70],[106.45,29.71],[106.46,29.72]]}},
       {"type":"Feature","properties":{"kind":"road"},
        "geometry":{"type":"Point","coordinates":[106.47,29.73]}}]})";

} // namespace

TEST(VectorMinimal, DecodeGeoJsonMultiPointAndPoint) {
    const std::vector<VectorPoint> pts = decodeGeoJsonPoints(kMultiPointFixture);
    ASSERT_EQ(pts.size(), 4u);
    EXPECT_NEAR(pts[0].lonRad, degreesToRadians(106.44), 1e-12);
    EXPECT_NEAR(pts[0].latRad, degreesToRadians(29.70), 1e-12);
    EXPECT_NEAR(pts[3].lonRad, degreesToRadians(106.47), 1e-12);
    EXPECT_NEAR(pts[3].latRad, degreesToRadians(29.73), 1e-12);
}

TEST(VectorMinimal, DecodeEmptyAndGarbage) {
    EXPECT_TRUE(decodeGeoJsonPoints("").empty());
    EXPECT_TRUE(decodeGeoJsonPoints("{not json").empty());
    EXPECT_TRUE(decodeGeoJsonPoints(R"({"coordinates":[]})").empty());
    // 括号边界隔离：前一个坐标数组的数字不泄漏进后一个。
    const char* fixture =
        R"({"a":{"coordinates":[[1.0,2.0]]},"b":{"coordinates":[[3.0,4.0]]}})";
    const std::vector<VectorPoint> pts = decodeGeoJsonPoints(fixture);
    ASSERT_EQ(pts.size(), 2u);
    EXPECT_NEAR(pts[0].lonRad, degreesToRadians(1.0), 1e-12);
    EXPECT_NEAR(pts[1].lonRad, degreesToRadians(3.0), 1e-12);
}

TEST(VectorMinimal, GroundingProjectsToTerrainPlusOffset) {
    auto flatGround = [](const Cartographic&) -> std::optional<double> { return 800.0; };
    VectorGrounding grounder(flatGround);
    VectorPoint p;
    p.lonRad = degreesToRadians(106.44);
    p.latRad = degreesToRadians(29.70);

    const std::optional<Vec3> ecef = grounder.projectToTerrain(p, /*offset=*/2.0);
    ASSERT_TRUE(ecef.has_value());
    const Cartographic back = Ellipsoid::WGS84().cartesianToCartographic(*ecef);
    EXPECT_NEAR(back.height(), 802.0, 1e-6); // 贴地 + 2m 浮空
    EXPECT_NEAR(back.longitude(), p.lonRad, 1e-9);
    EXPECT_NEAR(back.latitude(), p.latRad, 1e-9);

    const std::optional<Vec3> flat = grounder.projectToTerrain(p, 0.0);
    ASSERT_TRUE(flat.has_value());
    EXPECT_NEAR(Ellipsoid::WGS84().cartesianToCartographic(*flat).height(), 800.0, 1e-6);
}

TEST(VectorMinimal, GroundingWithoutDataReturnsNullopt) {
    VectorGrounding grounder(
        [](const Cartographic&) -> std::optional<double> { return std::nullopt; });
    VectorPoint p;
    p.lonRad = degreesToRadians(106.44);
    p.latRad = degreesToRadians(29.70);
    EXPECT_FALSE(grounder.projectToTerrain(p).has_value());
}

TEST(VectorMinimal, StyleForKeyIsDeterministic) {
    const PointStyle road = styleForKey("road");
    EXPECT_TRUE(road.visible);
    EXPECT_NEAR(road.widthPx, 3.0, 1e-12);
    const PointStyle def = styleForKey("unknown-key");
    EXPECT_NEAR(def.widthPx, 2.0, 1e-12);
    // 同一键两次调用结果一致。
    const PointStyle road2 = styleForKey("road");
    EXPECT_EQ(road.colorArgb, road2.colorArgb);
    EXPECT_DOUBLE_EQ(road.widthPx, road2.widthPx);
}
