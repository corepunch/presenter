#pragma once
#include "common.h"
#include "constants.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdint>
#include <array>

const char* getFontPath(FontType type);
std::string getFamilyFontPath(const std::string& family, FontType type);

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

    // Get font vertical metrics (ascent above baseline, descent below)
    float getAscent() const;
    float getDescent() const;

    // Render a single glyph directly to a 32-bit RGBA surface with alpha
    // blending. Returns the horizontal advance in pixels.
    float drawGlyph(SDL_Surface* surface, uint32_t codepoint, float x, float y, SDL_Color color) const;

    // Get pixel width of a UTF-8 string
    float measureString(const std::string& text) const;
    float measureGlyph(uint32_t codepoint) const;
    bool hasGlyph(uint32_t codepoint) const;

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

// The five style variants of one size class
struct FontVariants {
    const Font* fonts[5]; // indexed by FontType

    const Font& get(FontType type) const {
        return *fonts[static_cast<int>(type)];
    }
};

struct FontSet {
    bool boldTitles = false;
    std::string titleFamily = "Inter", bodyFamily = "Inter", codeFamily = "JetBrains Mono";
    std::array<Font, 5> fonts;             // content size
    std::array<Font, 5> titleFonts;        // title size
    std::array<Font, 5> subtitleFonts;     // subtitle size
    std::array<Font, 5> bulletFonts;       // bullet size
    std::array<Font, 5> smallFonts;        // small size
    std::array<Font, 5> childTitleFonts;   // child slide title size
    Font fallback;
    Font titleFallback;
    Font subtitleFallback;
    Font bulletFallback;
    Font smallFallback;
    Font childTitleFallback;
    Font iconFont;
    Font smallIconFont;
    Font bulletIconFont;

    bool load(float contentSize, float titleSize, float subtitleSize,
              float bulletSize, float smallSize, float childTitleSize) {
        return loadGroup(fonts, fallback, contentSize, bodyFamily)
            && loadGroup(titleFonts, titleFallback, titleSize, titleFamily)
            && loadGroup(subtitleFonts, subtitleFallback, subtitleSize, bodyFamily)
            && loadGroup(bulletFonts, bulletFallback, bulletSize, bodyFamily)
            && loadGroup(smallFonts, smallFallback, smallSize, bodyFamily)
            && loadGroup(childTitleFonts, childTitleFallback, childTitleSize, titleFamily)
            && iconFont.load("assets/FontAwesome-Free-Solid-900.otf", contentSize)
            && smallIconFont.load("assets/FontAwesome-Free-Solid-900.otf", smallSize)
            && bulletIconFont.load("assets/FontAwesome-Free-Solid-900.otf", bulletSize);
    }

    bool load(const struct PresentationStyle& style) {
        boldTitles = style.boldTitles;
        titleFamily = style.titleFamily;
        bodyFamily = style.bodyFamily;
        codeFamily = style.codeFamily;
        return load(style.contentFontSize, style.titleFontSize,
                    style.subtitleFontSize, style.bulletFontSize,
                    style.smallFontSize, style.childTitleFontSize);
    }

    const Font& get(FontType type) const {
        return fonts[static_cast<int>(type)];
    }

    FontVariants variants() const          { return makeVariants(fonts); }
    FontVariants titleVariants() const     { return headingVariants(titleFonts); }
    FontVariants subtitleVariants() const  { return makeVariants(subtitleFonts); }
    FontVariants bulletVariants() const    { return makeVariants(bulletFonts); }
    FontVariants smallVariants() const     { return makeVariants(smallFonts); }
    FontVariants childTitleVariants() const { return headingVariants(childTitleFonts); }
    const Font& icons() const { return iconFont; }
    const Font& smallIcons() const { return smallIconFont; }
    const Font& bulletIcons() const { return bulletIconFont; }

private:
    FontVariants headingVariants(const std::array<Font, 5>& group) const {
        auto v = makeVariants(group);
        if (boldTitles) {
            v.fonts[static_cast<int>(FontType::Regular)] = &group[static_cast<int>(FontType::Bold)];
            v.fonts[static_cast<int>(FontType::Italic)] = &group[static_cast<int>(FontType::BoldItalic)];
        }
        return v;
    }
    bool loadGroup(std::array<Font, 5>& group, Font& fb, float size, const std::string& family) {
        for (int i = 0; i < 5; i++) {
            auto type = static_cast<FontType>(i);
            auto path = getFamilyFontPath(type == FontType::Monospace ? codeFamily : family,
                                          type == FontType::Monospace ? FontType::Regular : type);
            if (path.empty() || !group[i].load(path, size))
                return false;
        }
        // Fallback is best-effort: the app still works without it
        if (fb.loadCandidates(fallbackFontCandidates(), size)) {
            for (auto& f : group) f.setFallback(&fb);
        }
        return true;
    }

    static FontVariants makeVariants(const std::array<Font, 5>& group) {
        FontVariants v;
        for (int i = 0; i < 5; i++) v.fonts[i] = &group[i];
        return v;
    }

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
