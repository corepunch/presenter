#pragma once
#include <string>

struct Color {
    unsigned char r = 0, g = 0, b = 0, a = 255;
    // No SDL dependency here — callers convert to SDL_Color explicitly
};

struct PresentationStyle {
    // Font sizes in pixels
    float titleFontSize = 48.0f;
    float subtitleFontSize = 28.0f;
    float contentFontSize = 28.0f;
    float smallFontSize = 20.0f;

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

    // Layout in pixels
    int slideMargin     = 40;
    int partPadding     = 20;
    int partGap         = 12;
    int columnGap       = 24;
    int linePadding     = 6;
    int presenterMargin = 20;

    void applyXmlElement(const void* el);  // tinyxml2::XMLElement*

    static PresentationStyle defaults();
    static PresentationStyle load(const std::string& xmlPath);
};
