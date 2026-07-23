#include "screenshot.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

static uint32_t crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t adler32(const uint8_t* data, size_t size) {
    constexpr uint32_t mod = 65521;
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % mod;
        b = (b + a) % mod;
    }
    return (b << 16) | a;
}

static void appendBigEndian(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

static void appendChunk(std::vector<uint8_t>& png, const char type[4],
                        const std::vector<uint8_t>& data) {
    appendBigEndian(png, static_cast<uint32_t>(data.size()));
    size_t crcStart = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    appendBigEndian(png, crc32(png.data() + crcStart, png.size() - crcStart));
}

static std::vector<uint8_t> deflateUncompressed(
    const std::vector<uint8_t>& input) {
    std::vector<uint8_t> out = {0x78, 0x01};
    size_t offset = 0;
    while (offset < input.size()) {
        size_t count = std::min<size_t>(65535, input.size() - offset);
        bool finalBlock = offset + count == input.size();
        uint16_t length = static_cast<uint16_t>(count);
        uint16_t inverseLength = static_cast<uint16_t>(~length);
        out.push_back(finalBlock ? 0x01 : 0x00);
        out.push_back(static_cast<uint8_t>(length & 0xFF));
        out.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(inverseLength & 0xFF));
        out.push_back(static_cast<uint8_t>((inverseLength >> 8) & 0xFF));
        out.insert(out.end(), input.begin() + static_cast<std::ptrdiff_t>(offset),
                   input.begin() + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;
    }
    appendBigEndian(out, adler32(input.data(), input.size()));
    return out;
}

bool saveSurfacePng(SDL_Surface* surface, const std::string& path) {
    if (!surface || path.empty()) return false;

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(
        surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgba) return false;

    std::vector<uint8_t> pixels;
    pixels.reserve(static_cast<size_t>(rgba->h) *
                   (1 + static_cast<size_t>(rgba->w) * 4));
    for (int y = 0; y < rgba->h; ++y) {
        pixels.push_back(0); // PNG filter: None
        const auto* row = static_cast<const uint8_t*>(rgba->pixels) +
                          static_cast<size_t>(y) * rgba->pitch;
        pixels.insert(pixels.end(), row, row + static_cast<size_t>(rgba->w) * 4);
    }

    std::vector<uint8_t> png = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
    };
    std::vector<uint8_t> header;
    appendBigEndian(header, static_cast<uint32_t>(rgba->w));
    appendBigEndian(header, static_cast<uint32_t>(rgba->h));
    header.insert(header.end(), {8, 6, 0, 0, 0});
    appendChunk(png, "IHDR", header);
    appendChunk(png, "IDAT", deflateUncompressed(pixels));
    appendChunk(png, "IEND", {});

    SDL_FreeSurface(rgba);

    std::filesystem::path output(path);
    std::error_code error;
    if (output.has_parent_path()) {
        std::filesystem::create_directories(output.parent_path(), error);
        if (error) return false;
    }
    std::ofstream file(output, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(png.data()),
               static_cast<std::streamsize>(png.size()));
    return file.good();
}
