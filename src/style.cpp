#include "style.h"
#include <tinyxml2.h>
#include <cstdio>

// --- Table-driven attribute setter ---

template<auto Member, auto Convert>
void parseAttr(PresentationStyle& s, const char* v) {
    using T = std::remove_reference_t<decltype(s.*Member)>;
    s.*Member = (T)Convert(v);
}

struct AttrEntry {
    const char* elem;   // child element name (fonts/colors/layout)
    const char* attr;   // attribute name
    void (*set)(PresentationStyle&, const char*);
};

static const AttrEntry STYLE_ATTRS[] = {
    // fonts
    {"fonts", "title",    parseAttr<&PresentationStyle::titleFontSize,    atof>},
    {"fonts", "subtitle", parseAttr<&PresentationStyle::subtitleFontSize, atof>},
    {"fonts", "content",  parseAttr<&PresentationStyle::contentFontSize,  atof>},
    {"fonts", "small",    parseAttr<&PresentationStyle::smallFontSize,    atof>},
    // colors — Color has implicit constructor from "const char*"
    {"colors", "bg",       [](PresentationStyle& s, const char* v) { s.bgColor = v; }},
    {"colors", "text",     [](PresentationStyle& s, const char* v) { s.textColor = v; }},
    {"colors", "title",    [](PresentationStyle& s, const char* v) { s.titleColor = v; }},
    {"colors", "subtitle", [](PresentationStyle& s, const char* v) { s.subtitleColor = v; }},
    {"colors", "accent",   [](PresentationStyle& s, const char* v) { s.accentColor = v; }},
    {"colors", "dim",      [](PresentationStyle& s, const char* v) { s.dimColor = v; }},
    {"colors", "line",     [](PresentationStyle& s, const char* v) { s.lineColor = v; }},
    // layout
    {"layout", "margin",          parseAttr<&PresentationStyle::slideMargin,     atoi>},
    {"layout", "padding",         parseAttr<&PresentationStyle::partPadding,     atoi>},
    {"layout", "gap",             parseAttr<&PresentationStyle::partGap,         atoi>},
    {"layout", "columnGap",       parseAttr<&PresentationStyle::columnGap,       atoi>},
    {"layout", "linePadding",     parseAttr<&PresentationStyle::linePadding,     atoi>},
    {"layout", "presenterMargin", parseAttr<&PresentationStyle::presenterMargin, atoi>},
};

void PresentationStyle::applyXmlElement(const void* el) {
    auto* root = static_cast<const tinyxml2::XMLElement*>(el);
    if (!root) return;
    for (auto& [elem, attr, set] : STYLE_ATTRS) {
        auto* child = root->FirstChildElement(elem);
        if (!child) continue;
        const char* v = child->Attribute(attr);
        if (v) set(*this, v);
    }
}

PresentationStyle PresentationStyle::defaults() {
    return PresentationStyle{};
}

PresentationStyle PresentationStyle::load(const std::string& xmlPath) {
    using namespace tinyxml2;
    PresentationStyle s;
    XMLDocument doc;
    if (doc.LoadFile(xmlPath.c_str()) != XML_SUCCESS) return s;
    XMLElement* root = doc.FirstChildElement("style");
    if (root) s.applyXmlElement(root);
    return s;
}

static PresentationStyle makeTheme(
    const char* name,
    float titleFs, float subtitleFs, float contentFs, float smallFs,
    Color bg, Color text, Color title, Color subtitle, Color accent, Color dim, Color line,
    Color codeBg, Color codeBorder, Color codeText,
    Color codeKeyword, Color codeType, Color codeString,
    Color codeComment, Color codeNumber, Color codeBuiltin, Color codePunctuation,
    int margin, int padding, int gap, int colGap, int linePad, int presMargin)
{
    PresentationStyle s;
    s.name = name;
    s.titleFontSize = titleFs;
    s.subtitleFontSize = subtitleFs;
    s.contentFontSize = contentFs;
    s.smallFontSize = smallFs;
    s.bgColor = bg;
    s.textColor = text;
    s.titleColor = title;
    s.subtitleColor = subtitle;
    s.accentColor = accent;
    s.dimColor = dim;
    s.lineColor = line;
    s.codeBg = codeBg;
    s.codeBorder = codeBorder;
    s.codeText = codeText;
    s.codeKeyword = codeKeyword;
    s.codeType = codeType;
    s.codeString = codeString;
    s.codeComment = codeComment;
    s.codeNumber = codeNumber;
    s.codeBuiltin = codeBuiltin;
    s.codePunctuation = codePunctuation;
    s.slideMargin = margin;
    s.partPadding = padding;
    s.partGap = gap;
    s.columnGap = colGap;
    s.linePadding = linePad;
    s.presenterMargin = presMargin;
    return s;
}

const std::vector<PresentationStyle>& PresentationStyle::builtInThemes() {
    static const std::vector<PresentationStyle> themes = {
        // 1. Dracula (default, dark)
        makeTheme("Dracula",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#282A36", "#F8F8F2", "#FF79C6",
            "#6272A4", "#BD93F9", "#6272A4", "#44475A",
            "#21222C", "#44475A", "#F8F8F2",
            "#FF79C6", "#8BE9FD", "#F1FA8C",
            "#6272A4", "#BD93F9", "#FFB86C",
            "#F8F8F2",
            40, 20, 12, 24, 6, 20),
        // 2. Monokai (dark)
        makeTheme("Monokai",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#272822", "#F8F8F2", "#F92672",
            "#75715E", "#A6E22E", "#75715E", "#3E3D32",
            "#1E1F1C", "#3E3D32", "#F8F8F2",
            "#F92672", "#66D9EF", "#E6DB74",
            "#75715E", "#AE81FF", "#FD971F",
            "#F8F8F2",
            40, 20, 12, 24, 6, 20),
        // 3. Solarized Dark
        makeTheme("Solarized Dark",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#002B36", "#93A1A1", "#FDF6E3",
            "#586E75", "#268BD2", "#586E75", "#073642",
            "#073642", "#002B36", "#93A1A1",
            "#CB4B16", "#2AA198", "#B58900",
            "#586E75", "#6C71C4", "#D33682",
            "#93A1A1",
            40, 20, 12, 24, 6, 20),
        // 4. GitHub Light
        makeTheme("GitHub Light",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#FFFFFF", "#24292E", "#0366D6",
            "#6A737D", "#0366D6", "#6A737D", "#E1E4E8",
            "#F6F8FA", "#E1E4E8", "#24292E",
            "#D73A49", "#005CC5", "#032F62",
            "#6A737D", "#005CC5", "#0550AE",
            "#24292E",
            40, 20, 12, 24, 6, 20),
        // 5. Solarized Light
        makeTheme("Solarized Light",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#FDF6E3", "#586E75", "#073642",
            "#93A1A1", "#268BD2", "#93A1A1", "#EEE8D5",
            "#EEE8D5", "#FDF6E3", "#586E75",
            "#CB4B16", "#2AA198", "#B58900",
            "#93A1A1", "#6C71C4", "#D33682",
            "#586E75",
            40, 20, 12, 24, 6, 20),
        // 6. Nord
        makeTheme("Nord",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#2E3440", "#D8DEE9", "#88C0D0",
            "#4C566A", "#88C0D0", "#4C566A", "#3B4252",
            "#3B4252", "#2E3440", "#D8DEE9",
            "#BF616A", "#A3BE8C", "#EBCB8B",
            "#4C566A", "#B48EAD", "#D08770",
            "#D8DEE9",
            40, 20, 12, 24, 6, 20),
        // 7. Sunset
        makeTheme("Sunset",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#1A1412", "#E8D5C4", "#FF8C42",
            "#8B7355", "#FF6B6B", "#8B7355", "#2D1F18",
            "#2D1F18", "#1A1412", "#E8D5C4",
            "#FF6B6B", "#FFA500", "#FFD700",
            "#8B7355", "#FF8C42", "#FF4500",
            "#E8D5C4",
            40, 20, 12, 24, 6, 20),
        // 8. Arc
        makeTheme("Arc",
            48.0f, 28.0f, 28.0f, 20.0f,
            "#383C4A", "#C8CCD4", "#61AFEF",
            "#545862", "#61AFEF", "#545862", "#4B4F5A",
            "#2C313A", "#383C4A", "#C8CCD4",
            "#E06C75", "#98C379", "#E5C07B",
            "#545862", "#C678DD", "#D19A66",
            "#C8CCD4",
            40, 20, 12, 24, 6, 20)
    };
    return themes;
}
