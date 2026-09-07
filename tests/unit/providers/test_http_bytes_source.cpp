#include <gtest/gtest.h>

#ifndef MAP_HTTP_AVAILABLE
#define MAP_HTTP_AVAILABLE 0
#endif

#if MAP_HTTP_AVAILABLE

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "earth_engine/content/HeightmapCodec.h"
#include "earth_engine/content/HeightmapTile.h"
#include "earth_engine/providers/CurlBytesSource.h"
#include "earth_engine/providers/TerrainRgbTileSource.h"

using namespace earth_engine;

namespace {

double hillFn(const Cartographic& c) {
    return 500.0 + 300.0 * std::sin(c.longitude() * 14.0) * std::cos(c.latitude() * 18.0);
}

/// 用 python3 起的本地只读 HTTP 服务（--directory），RAII 关停。
class LocalHttpServer {
public:
    LocalHttpServer(const std::filesystem::path& root, int port) : port_(port) {
        const std::string rootStr = root.string();
        const std::string cmd = "python3 -m http.server " + std::to_string(port) +
                                " --bind 127.0.0.1 --directory '" + rootStr +
                                "' >/dev/null 2>&1 & echo $! > '" + pidPath_ + "'";
        const int rc = std::system(cmd.c_str());
        (void)rc;
        // 等待就绪（用系统 curl 探测根路径）。
        const std::string probe = "curl -s -o /dev/null http://127.0.0.1:" + std::to_string(port) +
                                  "/ >/dev/null 2>&1";
        for (int i = 0; i < 50; ++i) {
            if (std::system(probe.c_str()) == 0) {
                ready_ = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    ~LocalHttpServer() {
        std::ifstream in(pidPath_);
        std::string pid;
        in >> pid;
        if (!pid.empty()) {
            const std::string kill = "kill " + pid + " 2>/dev/null";
            (void)std::system(kill.c_str());
        }
        (void)std::remove(pidPath_.c_str());
    }

    bool ready() const { return ready_; }
    int port() const { return port_; }

private:
    int port_;
    bool ready_ = false;
    std::string pidPath_ = "/tmp/map_cplus_http_" + std::to_string(::getpid()) + ".pid";
};

} // namespace

TEST(HttpBytesSource, FetchesFixtureAndDecodes) {
    // 建临时目录：9/<x>/<y>.bin 为 Terrain-RGB RGB 行（17²）。
    const auto keyOpt =
        WebMercatorTileScheme().tileKeyForCartographic(Cartographic::fromDegrees(106.5, 29.7), 9);
    ASSERT_TRUE(keyOpt.has_value());
    const TileKey key = keyOpt.value();

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("map_cplus_http_" + std::to_string(::getpid()));
    const std::filesystem::path keyDir = root / std::to_string(key.z()) /
                                         std::to_string(key.x());
    std::filesystem::create_directories(keyDir);
    const std::filesystem::path filePath = keyDir / (std::to_string(key.y()) + ".bin");

    constexpr int kSize = 17;
    {
        const WebMercatorTileScheme scheme;
        std::vector<double> zeros(static_cast<size_t>(kSize * kSize), 0.0);
        const HeightmapTile probe(scheme, key, zeros.data(), kSize, kSize);
        std::vector<uint8_t> rgb(static_cast<size_t>(kSize * kSize * 3));
        for (int row = 0; row < kSize; ++row) {
            for (int col = 0; col < kSize; ++col) {
                const Cartographic c = probe.pixelToCartographic(col, row);
                uint8_t r, g, b;
                HeightmapCodec::encodeTerrainRgbPixel(hillFn(c), r, g, b);
                const size_t px = (static_cast<size_t>(row) * kSize + col) * 3;
                rgb[px] = r;
                rgb[px + 1] = g;
                rgb[px + 2] = b;
            }
        }
        std::ofstream out(filePath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(rgb.data()),
                  static_cast<std::streamsize>(rgb.size()));
    }

    LocalHttpServer server(root, 18431 + (::getpid() % 1000));
    ASSERT_TRUE(server.ready()) << "本地 HTTP 服务未能就绪";

    const WebMercatorTileScheme scheme;
    const CurlBytesSource curl(/*timeoutMs=*/8000);
    const TerrainRgbTileSource source(
        curl, "http://127.0.0.1:" + std::to_string(server.port()) + "/{z}/{x}/{y}.bin");

    const auto grid = source.requestHeights(scheme, key, kSize);
    ASSERT_TRUE(grid.has_value());
    // 与 fn 逐点对照（HTTP 传输 + 解码全链路）。
    std::vector<double> zeros(static_cast<size_t>(kSize * kSize), 0.0);
    const HeightmapTile probe(scheme, key, zeros.data(), kSize, kSize);
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const Cartographic c = probe.pixelToCartographic(col, row);
            EXPECT_NEAR(grid->heights[static_cast<size_t>(row * kSize + col)], hillFn(c), 0.06)
                << "row " << row << " col " << col;
        }
    }

    // 不存在的瓦 → 404 → nullopt。
    const TileKey missingKey(key.z(), key.x() + 100, key.y());
    EXPECT_FALSE(source.requestHeights(scheme, missingKey, kSize).has_value());

    std::filesystem::remove_all(root);
}

#else

TEST(HttpBytesSource, DisabledWithoutLibcurl) { GTEST_SKIP() << "libcurl 不可用"; }

#endif
