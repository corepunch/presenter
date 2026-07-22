#pragma once
#include "common.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdint>
#include <array>

const char* getFontPath(FontType type);

class Font {
public:
    Font() = default;
    ~Font();

    bool load(const std::string& ttfPath, float fontSize);

    float getKerning(char a, char b) const;
    float getFontSize() const { return m_fontSize; }

    // Render a single glyph directly to a 32-bit RGBA surface with alpha blending
    void drawGlyph(SDL_Surface* surface, char c, float x, float y, SDL_Color color) const;

    // Get pixel width of a string
    float measureString(const std::string& text) const;

private:
    std::vector<uint8_t> m_ttfData;
    float m_fontSize = 0;
    float m_scale = 0;
    void* m_fontInfo = nullptr;  // stbtt_fontinfo*
};

struct FontSet {
    std::array<Font, 5> fonts; // indexed by FontType

    bool load(float fontSize) {
        for (int i = 0; i < 5; i++) {
            if (!fonts[i].load(getFontPath(static_cast<FontType>(i)), fontSize))
                return false;
        }
        return true;
    }

    const Font& get(FontType type) const {
        return fonts[static_cast<int>(type)];
    }
};
