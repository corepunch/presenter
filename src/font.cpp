#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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

void Font::drawGlyph(SDL_Surface* surface, char c, float x, float y, SDL_Color color) const {
    if (!m_fontInfo || !surface || c == ' ') return;
    auto* fontInfo = static_cast<stbtt_fontinfo*>(m_fontInfo);

    int width, height, xoff, yoff;
    uint8_t* bitmap = stbtt_GetCodepointBitmap(
        fontInfo, m_scale, m_scale, c, &width, &height, &xoff, &yoff);

    if (!bitmap || width <= 0 || height <= 0) {
        stbtt_FreeBitmap(bitmap, nullptr);
        return;
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
}

float Font::getKerning(char a, char b) const {
    if (!m_fontInfo) return 0;
    auto* fontInfo = static_cast<stbtt_fontinfo*>(m_fontInfo);
    int kern = stbtt_GetCodepointKernAdvance(fontInfo, a, b);
    return static_cast<float>(kern) * m_scale;
}

float Font::measureString(const std::string& text) const {
    if (!m_fontInfo) return 0;
    auto* fontInfo = static_cast<stbtt_fontinfo*>(m_fontInfo);
    float width = 0;
    for (size_t i = 0; i < text.size(); i++) {
        int advanceWidth;
        stbtt_GetCodepointHMetrics(fontInfo, text[i], &advanceWidth, nullptr);
        width += static_cast<float>(advanceWidth) * m_scale;
        if (i > 0) {
            width += getKerning(text[i - 1], text[i]);
        }
    }
    return width;
}
