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

    // Try each path in order until one loads; returns false if none work
    bool loadCandidates(const std::vector<std::string>& paths, float fontSize);

    // Set a shared fallback font (not owned) used for glyphs this font lacks
    void setFallback(const Font* fallback) { m_fallback = fallback; }

    float getKerning(uint32_t a, uint32_t b) const;
    float getFontSize() const { return m_fontSize; }

    // Render a single glyph directly to a 32-bit RGBA surface with alpha
    // blending. Returns the horizontal advance in pixels.
    float drawGlyph(SDL_Surface* surface, uint32_t codepoint, float x, float y, SDL_Color color) const;

    // Get pixel width of a UTF-8 string
    float measureString(const std::string& text) const;

private:
    // Returns the stbtt_fontinfo that contains this codepoint (primary or
    // fallback) and sets *outScale accordingly; nullptr if not found anywhere
    const void* resolveFont(uint32_t codepoint, float* outScale) const;

    std::vector<uint8_t> m_ttfData;
    float m_fontSize = 0;
    float m_scale = 0;
    void* m_fontInfo = nullptr;  // stbtt_fontinfo*
    const Font* m_fallback = nullptr;
};

struct FontSet {
    std::array<Font, 5> fonts; // indexed by FontType
    Font fallback;

    bool load(float fontSize) {
        for (int i = 0; i < 5; i++) {
            if (!fonts[i].load(getFontPath(static_cast<FontType>(i)), fontSize))
                return false;
        }
        // Fallback is best-effort: the app still works without it
        if (fallback.loadCandidates(fallbackFontCandidates(), fontSize)) {
            for (auto& f : fonts) f.setFallback(&fallback);
        }
        return true;
    }

    const Font& get(FontType type) const {
        return fonts[static_cast<int>(type)];
    }

private:
    static std::vector<std::string> fallbackFontCandidates() {
#if defined(_WIN32)
        return {
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\tahoma.ttf",
        };
#elif defined(__APPLE__)
        return {
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
            "/Library/Fonts/Arial.ttf",
        };
#else
        return {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        };
#endif
    }
};
