#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font.h"
#include <fstream>
#include <stdexcept>
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

FontAtlas::~FontAtlas() {
    delete static_cast<stbtt_fontinfo*>(m_fontInfo);
}

bool FontAtlas::load(const std::string& ttfPath, float fontSize) {
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

    m_bitmapWidth = ATLAS_SIZE;
    m_bitmapHeight = ATLAS_SIZE;
    m_bitmap.resize(ATLAS_SIZE * ATLAS_SIZE, 0);

    m_glyphs.resize(NUM_CHARS);
    m_rendered.resize(NUM_CHARS, false);
    m_cursorX = 0;
    m_cursorY = 0;
    m_rowHeight = 0;

    return true;
}

void FontAtlas::rasterizeGlyph(int index) const {
    if (m_rendered[index]) return;
    if (!m_fontInfo) return;

    auto* fontInfo = static_cast<stbtt_fontinfo*>(m_fontInfo);
    int codepoint = index + FIRST_CHAR;

    int width, height, xoff, yoff;
    uint8_t* bitmap = stbtt_GetCodepointBitmap(
        fontInfo, m_scale, m_scale, codepoint, &width, &height, &xoff, &yoff);

    if (!bitmap || width <= 0 || height <= 0) {
        stbtt_FreeBitmap(bitmap, nullptr);
        // Store empty glyph
        GlyphInfo& g = m_glyphs[index];
        g.x0 = g.y0 = g.x1 = g.y1 = 0;
        g.xadvance = 0;
        g.width = g.height = 0;
        g.xoffset = g.yoffset = 0;
        m_rendered[index] = true;
        return;
    }

    // Advance cursor, wrap to next row if needed
    int padding = 1;
    if (m_cursorX + width + padding > ATLAS_SIZE) {
        m_cursorX = 0;
        m_cursorY += m_rowHeight + padding;
        m_rowHeight = 0;
    }
    if (m_cursorY + height + padding > ATLAS_SIZE) {
        // Atlas full - can't render more glyphs
        stbtt_FreeBitmap(bitmap, nullptr);
        GlyphInfo& g = m_glyphs[index];
        g.x0 = g.y0 = g.x1 = g.y1 = 0;
        g.xadvance = 0;
        g.width = g.height = 0;
        g.xoffset = g.yoffset = 0;
        m_rendered[index] = true;
        return;
    }

    // Copy bitmap into atlas
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int dstX = m_cursorX + x;
            int dstY = m_cursorY + y;
            m_bitmap[dstY * ATLAS_SIZE + dstX] = bitmap[y * width + x];
        }
    }

    // Fill in GlyphInfo with UV coords
    GlyphInfo& g = m_glyphs[index];
    g.x0 = static_cast<float>(m_cursorX) / static_cast<float>(ATLAS_SIZE);
    g.y0 = static_cast<float>(m_cursorY) / static_cast<float>(ATLAS_SIZE);
    g.x1 = static_cast<float>(m_cursorX + width) / static_cast<float>(ATLAS_SIZE);
    g.y1 = static_cast<float>(m_cursorY + height) / static_cast<float>(ATLAS_SIZE);
    g.width = width;
    g.height = height;
    g.xoffset = xoff;
    g.yoffset = yoff;

    // Get advance width
    int advanceWidth;
    stbtt_GetCodepointHMetrics(fontInfo, codepoint, &advanceWidth, nullptr);
    g.xadvance = static_cast<float>(advanceWidth) * m_scale;

    stbtt_FreeBitmap(bitmap, nullptr);

    // Advance cursor
    m_cursorX += width + padding;
    if (height > m_rowHeight) m_rowHeight = height;

    m_rendered[index] = true;
}

const GlyphInfo& FontAtlas::getGlyph(char c) const {
    int index = static_cast<int>(c) - FIRST_CHAR;
    if (index < 0 || index >= NUM_CHARS) {
        // Render space if not done yet
        if (!m_rendered[0]) rasterizeGlyph(0);
        return m_glyphs[0];
    }
    if (!m_rendered[index]) {
        rasterizeGlyph(index);
    }
    return m_glyphs[index];
}

float FontAtlas::getKerning(char a, char b) const {
    if (!m_fontInfo) return 0;
    auto* fontInfo = static_cast<stbtt_fontinfo*>(m_fontInfo);
    int kern = stbtt_GetCodepointKernAdvance(fontInfo, a, b);
    return static_cast<float>(kern) * m_scale;
}

float FontAtlas::measureString(const std::string& text) const {
    float width = 0;
    for (size_t i = 0; i < text.size(); i++) {
        width += getGlyph(text[i]).xadvance;
        if (i > 0) {
            width += getKerning(text[i - 1], text[i]);
        }
    }
    return width;
}
