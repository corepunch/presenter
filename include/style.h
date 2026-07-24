#pragma once
#include "constants.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdio>

struct Color {
    unsigned char r = 0, g = 0, b = 0, a = 255;

    Color() = default;
    Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        : r(r), g(g), b(b), a(a) {}

    Color(const char* hex) {
        if (!hex || hex[0] != '#') return;
        unsigned int ri, gi, bi;
        if (std::sscanf(hex, "#%02x%02x%02x", &ri, &gi, &bi) == 3) {
            r = static_cast<unsigned char>(ri);
            g = static_cast<unsigned char>(gi);
            b = static_cast<unsigned char>(bi);
        }
    }

    SDL_Color toSDLColor() const { return {r, g, b, a}; }
    Uint32 toUint32(const SDL_PixelFormat* fmt) const {
        return SDL_MapRGBA(fmt, r, g, b, a);
    }
};

struct PresentationStyle {
    std::string name = "Dark";

    // Font sizes in pixels
    float titleFontSize = FONT_TITLE_SIZE;
    float subtitleFontSize = FONT_SUBTITLE_SIZE;
    float contentFontSize = FONT_CONTENT_SIZE;
    float bulletFontSize = FONT_BULLET_SIZE;
    float smallFontSize = FONT_SMALL_SIZE;
    float childTitleFontSize = FONT_CHILD_TITLE_SIZE;

    // Colors (RGBA)
    Color bgColor        = {30,  30,  40,  255};
    Color textColor      = {200, 200, 210, 255};
    Color titleColor     = {255, 255, 255, 255};
    Color subtitleColor  = {160, 160, 176, 255};
    Color accentColor    = {255, 204, 0,   255};
    Color dimColor       = {140, 140, 155, 255};
    Color lineColor      = {100, 100, 120, 255};

    // Code block colors
    Color codeBg         = {22,  22,  32,  255};
    Color codeBorder     = {60,  60,  80,  255};
    Color codeText       = {200, 200, 210, 255};
    Color codeKeyword    = {198, 120, 221, 255};
    Color codeType       = {86,  182, 194, 255};
    Color codeString     = {152, 195, 121, 255};
    Color codeComment    = {92,  99,  112, 255};
    Color codeNumber     = {209, 154, 102, 255};
    Color codeBuiltin    = {229, 192, 123, 255};
    Color codePunctuation = {171, 178, 191, 255};

    // Chart colors. Series colors are cycled in declaration order.
    Color chartSeries1   = {255, 204, 0,   255};
    Color chartSeries2   = {86,  182, 194, 255};
    Color chartSeries3   = {152, 195, 121, 255};
    Color chartSeries4   = {209, 154, 102, 255};
    Color chartSeries5   = {198, 120, 221, 255};
    Color chartSeries6   = {229, 192, 123, 255};
    Color chartGrid      = {100, 100, 120, 255};
    Color chartLabel     = {200, 200, 210, 255};

    // Layout in pixels
    int slideMargin     = SLIDE_MARGIN;
    int partPadding     = PART_PADDING;
    int partGap         = PART_GAP;
    int columnGap       = COLUMN_GAP;
    int linePadding     = LINE_PADDING;
    int presenterMargin = PRESENTER_MARGIN;
    int cornerRadius    = CORNER_RADIUS;
    int presenterCornerRadius = PRESENTER_CORNER_RADIUS;
    int bulletGap       = BULLET_GAP;

    void applyXmlElement(const void* el);  // tinyxml2::XMLElement*

    static PresentationStyle defaults();
    static PresentationStyle load(const std::string& xmlPath);
    static const std::vector<PresentationStyle>& builtInThemes();
};
