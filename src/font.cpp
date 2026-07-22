#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font.h"
#include <fstream>
#include <stdexcept>

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

    const int bitmapW = 1024;
    const int bitmapH = 1024;
    m_bitmapWidth = bitmapW;
    m_bitmapHeight = bitmapH;
    m_bitmap.resize(bitmapW * bitmapH);

    stbtt_bakedchar bakedChars[NUM_CHARS];
    int result = stbtt_BakeFontBitmap(
        m_ttfData.data(),
        stbtt_GetFontOffsetForIndex(m_ttfData.data(), 0),
        fontSize,
        m_bitmap.data(),
        bitmapW, bitmapH,
        FIRST_CHAR, NUM_CHARS,
        bakedChars
    );

    if (result <= 0) {
        delete fontInfo;
        m_fontInfo = nullptr;
        return false;
    }

    m_glyphs.resize(NUM_CHARS);
    for (int i = 0; i < NUM_CHARS; i++) {
        const stbtt_bakedchar& bc = bakedChars[i];
        GlyphInfo& g = m_glyphs[i];
        g.x0 = static_cast<float>(bc.x0) / static_cast<float>(bitmapW);
        g.y0 = static_cast<float>(bc.y0) / static_cast<float>(bitmapH);
        g.x1 = static_cast<float>(bc.x1) / static_cast<float>(bitmapW);
        g.y1 = static_cast<float>(bc.y1) / static_cast<float>(bitmapH);
        g.xadvance = bc.xadvance;
        g.width = bc.x1 - bc.x0;
        g.height = bc.y1 - bc.y0;
        g.xoffset = bc.xoff;
        g.yoffset = bc.yoff;
    }

    return true;
}

const GlyphInfo& FontAtlas::getGlyph(char c) const {
    int index = static_cast<int>(c) - FIRST_CHAR;
    if (index < 0 || index >= NUM_CHARS) {
        return m_glyphs[0];
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
