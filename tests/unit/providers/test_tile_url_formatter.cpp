#include <gtest/gtest.h>

#include "earth_engine/providers/TileUrlFormatter.h"

using namespace earth_engine;

TEST(TileUrlFormatter, SubstitutesZXY) {
    const std::string out =
        TileUrlFormatter::format("https://t.example.com/{z}/{x}/{y}.png", TileKey(5, 12, 34));
    EXPECT_EQ(out, "https://t.example.com/5/12/34.png");
}

TEST(TileUrlFormatter, MultipleAndRepeatedPlaceholders) {
    // 同一占位出现多次全部替换。
    const std::string out =
        TileUrlFormatter::format("{z}-{z}/{x}/{y}/{x}", TileKey(3, 7, 8));
    EXPECT_EQ(out, "3-3/7/8/7");
}

TEST(TileUrlFormatter, UnknownPlaceholderPreserved) {
    const std::string out =
        TileUrlFormatter::format("https://x/{z}/{x}/{y}/{tms_y}", TileKey(2, 1, 1));
    EXPECT_EQ(out, "https://x/2/1/1/{tms_y}"); // 未知占位原样保留
}

TEST(TileUrlFormatter, NoPlaceholderPassesThrough) {
    const std::string out = TileUrlFormatter::format("https://static/earth.png", TileKey(0, 0, 0));
    EXPECT_EQ(out, "https://static/earth.png");
    EXPECT_FALSE(TileUrlFormatter::hasPlaceholders(out));
    EXPECT_TRUE(TileUrlFormatter::hasPlaceholders("https://x/{z}.png"));
}
