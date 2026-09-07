// TerrainGroundGuard：贴地防护（S6 host 切片，"不穿地"策略纯函数）。
#include <gtest/gtest.h>

#include <limits>
#include <optional>

#include "earth_engine/camera/TerrainGroundGuard.h"
#include "earth_engine/core/geodesy/Cartographic.h"
#include "earth_engine/core/math/MathUtils.h"

using namespace earth_engine;

namespace {

// 恒定地面高度（米）。
TerrainGroundGuard::GroundHeightFn flatGround(double h) {
    return [h](const Cartographic&) -> std::optional<double> { return h; };
}

} // namespace

TEST(TerrainGroundGuard, BelowGroundIsLiftedWithClearance) {
    const TerrainGroundGuard guard(/*minClearance=*/5.0);
    const auto r = guard.enforceClearance(degreesToRadians(106.5), degreesToRadians(29.7),
                                          400.0, flatGround(500.0));
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->heightMeters, 505.0); // ground+clearance
    EXPECT_TRUE(r->clamped);
}

TEST(TerrainGroundGuard, AboveGroundUnchanged) {
    const TerrainGroundGuard guard(/*minClearance=*/5.0);
    const auto r = guard.enforceClearance(degreesToRadians(106.5), degreesToRadians(29.7),
                                          900.0, flatGround(500.0));
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->heightMeters, 900.0);
    EXPECT_FALSE(r->clamped);
}

TEST(TerrainGroundGuard, NoGroundDataLeavesHeightUntouched) {
    const TerrainGroundGuard guard(5.0);
    const auto none = [](const Cartographic&) -> std::optional<double> {
        return std::nullopt;
    };
    const auto r = guard.enforceClearance(degreesToRadians(10.0), degreesToRadians(10.0),
                                          100.0, none);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->heightMeters, 100.0);
    EXPECT_FALSE(r->clamped); // 无地表约束不算"抬升"
}

TEST(TerrainGroundGuard, InvalidInputsReturnNullopt) {
    const TerrainGroundGuard guard(5.0);
    EXPECT_FALSE(guard.enforceClearance(std::numeric_limits<double>::quiet_NaN(), 0.0, 100.0,
                                        flatGround(0.0))
                     .has_value());
    EXPECT_FALSE(guard.enforceClearance(0.0, 0.0,
                                        std::numeric_limits<double>::infinity(), flatGround(0.0))
                     .has_value());
}

TEST(TerrainGroundGuard, CurvedGroundFollowsElevation) {
    const TerrainGroundGuard guard(2.0);
    // 山地：地面 500+300·|sin| 类；相机在某点 700，地面 780 → 抬到 782。
    const auto hill = [](const Cartographic& c) -> std::optional<double> {
        return 500.0 + 300.0 * std::abs(std::sin(c.latitude() * 90.0));
    };
    const double lat = degreesToRadians(29.7);
    const double groundAt = 500.0 + 300.0 * std::abs(std::sin(lat * 90.0));
    const double camH = groundAt - 80.0; // 明显穿地
    const auto r = guard.enforceClearance(0.0, lat, camH, hill);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->heightMeters, groundAt + 2.0);
    EXPECT_TRUE(r->clamped);
}
