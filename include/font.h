#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <unordered_map>

const char* getFontPath(FontType type);

struct GlyphInfo {
    float x0, y0, x1, y1;  // UV coords in atlas
    float xadvance;          // horizontal advance
    int width, height;       // pixel dimensions
    int xoffset, yoffset;    // baseline offset
};

class FontAtlas {
public:
    FontAtlas() = default;
    ~FontAtlas();

    bool load(const std::string& ttfPath, float fontSize);

    const uint8_t* bitmapData() const { return m_bitmap.data(); }
    int bitmapWidth() const { return m_bitmapWidth; }
    int bitmapHeight() const { return m_bitmapHeight; }

    const GlyphInfo& getGlyph(char c) const;
    float getKerning(char a, char b) const;
    float getFontSize() const { return m_fontSize; }

    // Get pixel width of a string
    float measureString(const std::string& text) const;

private:
    void rasterizeGlyph(int index) const;

    std::vector<uint8_t> m_ttfData;
    mutable std::vector<uint8_t> m_bitmap;
    mutable std::vector<GlyphInfo> m_glyphs;
    mutable std::vector<bool> m_rendered;  // tracks which glyphs are in the atlas
    mutable int m_cursorX = 0;
    mutable int m_cursorY = 0;
    mutable int m_rowHeight = 0;
    int m_bitmapWidth = 0;
    int m_bitmapHeight = 0;
    float m_fontSize = 0;
    float m_scale = 0;
    void* m_fontInfo = nullptr;  // stbtt_fontinfo*
    static constexpr int FIRST_CHAR = 32;
    static constexpr int NUM_CHARS = 96;  // ASCII 32-127
    static constexpr int ATLAS_SIZE = 1024;
};

struct FontSet {
    std::array<FontAtlas, 5> fonts; // indexed by FontType

    bool load(float fontSize) {
        for (int i = 0; i < 5; i++) {
            if (!fonts[i].load(getFontPath(static_cast<FontType>(i)), fontSize))
                return false;
        }
        return true;
    }

    const FontAtlas& get(FontType type) const {
        return fonts[static_cast<int>(type)];
    }
};
