#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "utf8.h"

#include "font.h"
#include <fstream>
#include <algorithm>

const char* getFontPath(FontType type) {
    switch (type) {
        case FontType::Regular:    return "assets/Inter-Regular.ttf";
        case FontType::Bold:       return "assets/Inter-Bold.ttf";
        case FontType::Italic:     return "assets/Inter-Italic.ttf";
        case FontType::BoldItalic: return "assets/Inter-BoldItalic.ttf";
        case FontType::Monospace:  return "assets/JetBrainsMono-Regular.ttf";
    }
    return "assets/Inter-Regular.ttf";
}

Font::~Font() {
    delete static_cast<stbtt_fontinfo*>(m_fontInfo);
}

bool Font::load(const std::string& ttfPath, float fontSize) {
    std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_ttfData.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(m_ttfData.data()), size)) return false;

    auto* fontInfo = new stbtt_fontinfo;
    if (!stbtt_InitFont(fontInfo, m_ttfData.data(),
            stbtt_GetFontOffsetForIndex(m_ttfData.data(), 0))) {
        delete fontInfo;
        return false;
    }
    m_fontInfo = fontInfo;
    m_fontSize = fontSize;
    m_scale = stbtt_ScaleForPixelHeight(fontInfo, fontSize);

    return true;
}

bool Font::loadCandidates(const std::vector<std::string>& paths, float fontSize) {
    for (const auto& path : paths) {
        if (load(path, fontSize)) return true;
    }
    return false;
}

const void* Font::resolveFont(uint32_t codepoint, float* outScale) const {
    auto* primary = static_cast<const stbtt_fontinfo*>(m_fontInfo);
    if (primary && stbtt_FindGlyphIndex(primary, static_cast<int>(codepoint)) != 0) {
        *outScale = m_scale;
        return primary;
    }
    if (m_fallback) {
        auto* fb = static_cast<const stbtt_fontinfo*>(m_fallback->m_fontInfo);
        if (fb && stbtt_FindGlyphIndex(fb, static_cast<int>(codepoint)) != 0) {
            *outScale = m_fallback->m_scale;
            return fb;
        }
    }
    return nullptr;
}

float Font::drawGlyph(SDL_Surface* surface, uint32_t codepoint, float x, float y, SDL_Color color) const {
    float scale = 0;
    const stbtt_fontinfo* info = static_cast<const stbtt_fontinfo*>(resolveFont(codepoint, &scale));

    // Missing everywhere: fall back to '?' so we don't render tofu
    if (!info) {
        codepoint = '?';
        info = static_cast<const stbtt_fontinfo*>(resolveFont(codepoint, &scale));
        if (!info) return 0;
    }

    int advanceWidth;
    stbtt_GetCodepointHMetrics(info, static_cast<int>(codepoint), &advanceWidth, nullptr);
    float advance = static_cast<float>(advanceWidth) * scale;

    if (!surface || codepoint == ' ') return advance;

    int width, height, xoff, yoff;
    uint8_t* bitmap = stbtt_GetCodepointBitmap(
        info, scale, scale, static_cast<int>(codepoint), &width, &height, &xoff, &yoff);

    if (!bitmap || width <= 0 || height <= 0) {
        stbtt_FreeBitmap(bitmap, nullptr);
        return advance;
    }

    int baseX = static_cast<int>(x + xoff);
    int baseY = static_cast<int>(y + yoff);

    SDL_LockSurface(surface);
    auto* pixels = static_cast<Uint32*>(surface->pixels);
    int pitch = surface->pitch / 4;

    for (int gy = 0; gy < height; gy++) {
        int py = baseY + gy;
        if (py < 0 || py >= surface->h) continue;
        for (int gx = 0; gx < width; gx++) {
            int px = baseX + gx;
            if (px < 0 || px >= surface->w) continue;

            uint8_t alpha = bitmap[gy * width + gx];
            if (alpha == 0) continue;

            // Blend: result = src * alpha + dst * (1 - alpha)
            Uint32 dst = pixels[py * pitch + px];
            Uint8 dr = dst & 0xFF;
            Uint8 dg = (dst >> 8) & 0xFF;
            Uint8 db = (dst >> 16) & 0xFF;

            Uint8 rr = static_cast<Uint8>((color.r * alpha + dr * (255 - alpha)) / 255);
            Uint8 rg = static_cast<Uint8>((color.g * alpha + dg * (255 - alpha)) / 255);
            Uint8 rb = static_cast<Uint8>((color.b * alpha + db * (255 - alpha)) / 255);

            pixels[py * pitch + px] = 0xFF000000 | (rb << 16) | (rg << 8) | rr;
        }
    }

    SDL_UnlockSurface(surface);
    stbtt_FreeBitmap(bitmap, nullptr);
    return advance;
}

float Font::getKerning(uint32_t a, uint32_t b) const {
    // Kerning is only meaningful within a single font
    auto* primary = static_cast<const stbtt_fontinfo*>(m_fontInfo);
    if (!primary) return 0;
    if (stbtt_FindGlyphIndex(primary, static_cast<int>(a)) == 0) return 0;
    if (stbtt_FindGlyphIndex(primary, static_cast<int>(b)) == 0) return 0;
    int kern = stbtt_GetCodepointKernAdvance(primary, static_cast<int>(a), static_cast<int>(b));
    return static_cast<float>(kern) * m_scale;
}

float Font::getAscent() const {
    auto* primary = static_cast<const stbtt_fontinfo*>(m_fontInfo);
    if (!primary) return 0;
    int ascent;
    stbtt_GetFontVMetrics(primary, &ascent, nullptr, nullptr);
    return static_cast<float>(ascent) * m_scale;
}

float Font::getDescent() const {
    auto* primary = static_cast<const stbtt_fontinfo*>(m_fontInfo);
    if (!primary) return 0;
    int descent;
    stbtt_GetFontVMetrics(primary, nullptr, &descent, nullptr);
    return static_cast<float>(descent) * m_scale;
}

float Font::measureString(const std::string& text) const {
    float width = 0;
    utf8_int32_t prev = 0;
    const utf8_int8_t* s = reinterpret_cast<const utf8_int8_t*>(text.c_str());
    while (*s) {
        utf8_int32_t cp = 0;
        s = utf8codepoint(s, &cp);
        if (prev) width += getKerning(static_cast<uint32_t>(prev), static_cast<uint32_t>(cp));
        float scale = 0;
        auto* info = static_cast<const stbtt_fontinfo*>(resolveFont(static_cast<uint32_t>(cp), &scale));
        if (info) {
            int advanceWidth;
            stbtt_GetCodepointHMetrics(info, cp, &advanceWidth, nullptr);
            width += static_cast<float>(advanceWidth) * scale;
        }
        prev = cp;
    }
    return width;
}
