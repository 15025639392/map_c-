#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// 最小 PNG 编码器（测试 fixture 用）：8bit RGB、zlib stored-deflate。
/// 由 stb（库内 STB_IMAGE_IMPLEMENTATION）解码验证——自足，不依赖第三方测试头。
/// 供多套件复用（png_terrain_source / nasa_ring_source …），避免各自复制。
namespace mapc_test {

class Crc32 {
public:
    Crc32() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table_[i] = c;
        }
    }
    uint32_t update(uint32_t crc, const uint8_t* data, size_t n) const {
        crc = crc ^ 0xFFFFFFFFu;
        for (size_t i = 0; i < n; ++i) {
            crc = table_[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }

private:
    uint32_t table_[256];
};

inline void appendBigEndian32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void appendPngChunk(std::vector<uint8_t>& out, const char type[4],
                           const std::vector<uint8_t>& data) {
    static const Crc32 crc;
    appendBigEndian32(out, static_cast<uint32_t>(data.size()));
    const size_t typePos = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t c = crc.update(0, out.data() + typePos, 4 + data.size());
    appendBigEndian32(out, c);
}

/// 生成 RGB 行（filter 0）→ zlib stored → PNG 字节。
inline std::vector<uint8_t> writePngRgb(const std::vector<uint8_t>& rgb, int w, int h) {
    std::vector<uint8_t> png;
    png.insert(png.end(), {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
    // IHDR
    std::vector<uint8_t> ihdr;
    appendBigEndian32(ihdr, static_cast<uint32_t>(w));
    appendBigEndian32(ihdr, static_cast<uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0}); // bit8, color RGB
    appendPngChunk(png, "IHDR", ihdr);
    // IDAT：每行前插 filter 0，然后 zlib stored 块。
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (w * 3 + 1));
    for (int row = 0; row < h; ++row) {
        raw.push_back(0);
        const size_t off = static_cast<size_t>(row) * w * 3;
        raw.insert(raw.end(), rgb.begin() + static_cast<long>(off),
                   rgb.begin() + static_cast<long>(off) + static_cast<size_t>(w) * 3);
    }
    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        const size_t remain = raw.size() - pos;
        const size_t len = remain < 65535 ? remain : 65535;
        const bool final = (pos + len == raw.size());
        zlib.push_back(static_cast<uint8_t>((final ? 1 : 0) | (0 << 1))); // stored 块头
        zlib.push_back(static_cast<uint8_t>(len & 0xFF));
        zlib.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        const uint16_t nlen = static_cast<uint16_t>(~len & 0xFFFF);
        zlib.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<long>(pos),
                    raw.begin() + static_cast<long>(pos + len));
        pos += len;
    }
    // zlib adler-32（两轮求和）。
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for (const uint8_t b : raw) {
        s1 = (s1 + b) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    const uint32_t adlerFinal = (s2 << 16) | s1;
    appendBigEndian32(zlib, adlerFinal);
    appendPngChunk(png, "IDAT", zlib);
    // IEND
    appendPngChunk(png, "IEND", {});
    return png;
}

/// RGBA PNG（color type 6，保留 alpha）。
inline std::vector<uint8_t> writePngRgbaRgba(const std::vector<uint8_t>& rgba, int w, int h) {
    std::vector<uint8_t> png;
    png.insert(png.end(), {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
    std::vector<uint8_t> ihdr;
    appendBigEndian32(ihdr, static_cast<uint32_t>(w));
    appendBigEndian32(ihdr, static_cast<uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0}); // bit8, color RGBA
    appendPngChunk(png, "IHDR", ihdr);
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (w * 4 + 1));
    for (int row = 0; row < h; ++row) {
        raw.push_back(0);
        const size_t off = static_cast<size_t>(row) * w * 4;
        raw.insert(raw.end(), rgba.begin() + static_cast<long>(off),
                   rgba.begin() + static_cast<long>(off) + static_cast<size_t>(w) * 4);
    }
    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        const size_t remain = raw.size() - pos;
        const size_t len = remain < 65535 ? remain : 65535;
        const bool final = (pos + len == raw.size());
        zlib.push_back(static_cast<uint8_t>((final ? 1 : 0) | (0 << 1)));
        zlib.push_back(static_cast<uint8_t>(len & 0xFF));
        zlib.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        const uint16_t nlen = static_cast<uint16_t>(~len & 0xFFFF);
        zlib.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<long>(pos),
                    raw.begin() + static_cast<long>(pos + len));
        pos += len;
    }
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for (const uint8_t b : raw) {
        s1 = (s1 + b) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    const uint32_t adlerFinal = (s2 << 16) | s1;
    appendBigEndian32(zlib, adlerFinal);
    appendPngChunk(png, "IDAT", zlib);
    appendPngChunk(png, "IEND", {});
    return png;
}

} // namespace mapc_test
