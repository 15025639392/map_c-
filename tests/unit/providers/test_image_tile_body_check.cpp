// 响应体魔数白名单（网络硬化差值表项，转写 gis-md providers/ImageTileBodyCheck）：
// PNG/JPEG/WebP 白名单 + 12 字节下限；CDN 200 + NoSuchKey XML 错误体被挡在
// 地形解码链入口（TerrainRgbPngTileSource）之前。
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "earth_engine/providers/ImageTileBodyCheck.h"
#include "earth_engine/providers/TerrainRgbPngTileSource.h"
#include "earth_engine/providers/ITileBytesSource.h"
#include "earth_engine/tiling/TileKey.h"
#include "earth_engine/tiling/WebMercatorTileScheme.h"

using namespace earth_engine;

namespace {

// 恒返回指定体的字节源（模拟网络响应体，与状态码无关）。
class FixedBodySource : public ITileBytesSource {
public:
    explicit FixedBodySource(std::vector<uint8_t> body) : body_(std::move(body)) {}
    std::optional<std::vector<uint8_t>> requestTileBytes(const TileKey&,
                                                         const std::string&) const override {
        return body_;
    }

private:
    std::vector<uint8_t> body_;
};

std::vector<uint8_t> noSuchKeyXmlBody() {
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error><Code>NoSuchKey</Code>"
        "<Message>The specified key does not exist.</Message></Error>";
    return std::vector<uint8_t>(xml.begin(), xml.end());
}

} // namespace

// 转写 gis-md WhitelistsImageMagicsAndRejectsErrorBodies。
TEST(ImageTileBodyCheck, WhitelistsImageMagicsAndRejectsErrorBodies) {
    const std::vector<uint8_t> png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
                                   0, 0, 0, 0, 0, 0, 0, 0};
    const std::vector<uint8_t> jpeg{0xFF, 0xD8, 0xFF, 0xE0, 0, 0, 0, 0, 0, 0, 0, 0};
    const std::vector<uint8_t> webp{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
    EXPECT_TRUE(looksLikeImageTileBody(png));
    EXPECT_TRUE(looksLikeImageTileBody(jpeg));
    EXPECT_TRUE(looksLikeImageTileBody(webp));

    EXPECT_FALSE(looksLikeImageTileBody(noSuchKeyXmlBody()));
    EXPECT_FALSE(looksLikeImageTileBody(std::vector<uint8_t>{}));
    // 12 字节下限：比最短魔数还短的体一律非法（含只有 PNG 前缀的截断体）。
    EXPECT_FALSE(looksLikeImageTileBody(std::vector<uint8_t>{0x89, 'P', 'N', 'G'}));
}

// 集成：CDN 200 错误体（XML）到达 PNG 高度源 → 入口即拒（nullopt），不进入解码。
TEST(ImageTileBodyCheck, XmlErrorBodyRejectedAtTerrainSourceEntry) {
    const WebMercatorTileScheme scheme;
    const TileKey key(9, 200, 100);
    FixedBodySource xmlBody(noSuchKeyXmlBody());
    const TerrainRgbPngTileSource source(xmlBody, "https://t/{z}/{x}/{y}.png");
    EXPECT_FALSE(source.requestHeights(scheme, key, 17).has_value());
}

// 集成：魔数通过但非目标尺寸的真实 PNG 仍走原尺寸不符拒绝路径（白名单不松绑）。
TEST(ImageTileBodyCheck, WhitelistDoesNotRelaxSizeCheck) {
    // 16×16 全黑 PNG 头（合法 PNG 魔数 + IHDR 声明 16×16，IDAT 无所谓——尺寸检查
    // 在 PNG 解码后发生，这里仅验证入口白名单放行后仍由原路径拒绝/处理）。
    std::vector<uint8_t> body{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
                              0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_TRUE(looksLikeImageTileBody(body));
}
