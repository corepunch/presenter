#pragma once
#include <string>
#include <vector>
#include <cstdint>

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
    std::vector<uint8_t> m_ttfData;
    std::vector<uint8_t> m_bitmap;
    std::vector<GlyphInfo> m_glyphs;  // indexed by char - 32
    int m_bitmapWidth = 0;
    int m_bitmapHeight = 0;
    float m_fontSize = 0;
    float m_scale = 0;
    void* m_fontInfo = nullptr;  // stbtt_fontinfo*, opaque to avoid stb includes in header
    static constexpr int FIRST_CHAR = 32;
    static constexpr int NUM_CHARS = 96;  // ASCII 32-127
};
