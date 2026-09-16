#include "style.h"
#include <tinyxml2.h>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

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
    {"fonts", "titleFamily", [](PresentationStyle& s, const char* v) { s.titleFamily = v; }},
    {"fonts", "bodyFamily", [](PresentationStyle& s, const char* v) { s.bodyFamily = v; }},
    {"fonts", "codeFamily", [](PresentationStyle& s, const char* v) { s.codeFamily = v; }},
    {"fonts", "childTitle", parseAttr<&PresentationStyle::childTitleFontSize, atof>},
    {"syntax", "bg", [](PresentationStyle& s, const char* v) { s.codeBg = v; }},
    {"syntax", "border", [](PresentationStyle& s, const char* v) { s.codeBorder = v; }},
    {"syntax", "text", [](PresentationStyle& s, const char* v) { s.codeText = v; }},
    {"syntax", "keyword", [](PresentationStyle& s, const char* v) { s.codeKeyword = v; }},
    {"syntax", "type", [](PresentationStyle& s, const char* v) { s.codeType = v; }},
    {"syntax", "string", [](PresentationStyle& s, const char* v) { s.codeString = v; }},
    {"syntax", "comment", [](PresentationStyle& s, const char* v) { s.codeComment = v; }},
    {"syntax", "number", [](PresentationStyle& s, const char* v) { s.codeNumber = v; }},
    {"syntax", "builtin", [](PresentationStyle& s, const char* v) { s.codeBuiltin = v; }},
    {"syntax", "punctuation", [](PresentationStyle& s, const char* v) { s.codePunctuation = v; }},
    // fonts
    {"fonts", "title",    parseAttr<&PresentationStyle::titleFontSize,    atof>},
    {"fonts", "subtitle", parseAttr<&PresentationStyle::subtitleFontSize, atof>},
    {"fonts", "content",  parseAttr<&PresentationStyle::contentFontSize,  atof>},
    {"fonts", "bullet",  parseAttr<&PresentationStyle::bulletFontSize,  atof>},
    {"fonts", "small",    parseAttr<&PresentationStyle::smallFontSize,    atof>},
    // colors — Color has implicit constructor from "const char*"
    {"colors", "bg",       [](PresentationStyle& s, const char* v) { s.bgColor = s.bgColor2 = v; }},
    {"colors", "bg2",      [](PresentationStyle& s, const char* v) { s.bgColor2 = v; }},
    {"colors", "text",     [](PresentationStyle& s, const char* v) { s.textColor = v; }},
    {"colors", "title",    [](PresentationStyle& s, const char* v) { s.titleColor = v; }},
    {"colors", "subtitle", [](PresentationStyle& s, const char* v) { s.subtitleColor = v; }},
    {"colors", "accent",   [](PresentationStyle& s, const char* v) { s.accentColor = v; }},
    {"colors", "dim",      [](PresentationStyle& s, const char* v) { s.dimColor = v; }},
    {"colors", "presenterNext", [](PresentationStyle& s, const char* v) { s.presenterNextColor = v; }},
    {"colors", "line",     [](PresentationStyle& s, const char* v) { s.lineColor = v; }},
    // charts
    {"charts", "series1", [](PresentationStyle& s, const char* v) { s.chartSeries1 = v; }},
    {"charts", "series2", [](PresentationStyle& s, const char* v) { s.chartSeries2 = v; }},
    {"charts", "series3", [](PresentationStyle& s, const char* v) { s.chartSeries3 = v; }},
    {"charts", "series4", [](PresentationStyle& s, const char* v) { s.chartSeries4 = v; }},
    {"charts", "series5", [](PresentationStyle& s, const char* v) { s.chartSeries5 = v; }},
    {"charts", "series6", [](PresentationStyle& s, const char* v) { s.chartSeries6 = v; }},
    {"charts", "grid",    [](PresentationStyle& s, const char* v) { s.chartGrid = v; }},
    {"charts", "label",   [](PresentationStyle& s, const char* v) { s.chartLabel = v; }},
    // layout
    {"layout", "margin",          parseAttr<&PresentationStyle::slideMargin,     atoi>},
    {"layout", "padding",         parseAttr<&PresentationStyle::partPadding,     atoi>},
    {"layout", "gap",             parseAttr<&PresentationStyle::partGap,         atoi>},
    {"layout", "columnGap",       parseAttr<&PresentationStyle::columnGap,       atoi>},
    {"layout", "linePadding",     parseAttr<&PresentationStyle::linePadding,     atoi>},
    {"layout", "presenterMargin", parseAttr<&PresentationStyle::presenterMargin, atoi>},
    {"layout", "cornerRadius",    parseAttr<&PresentationStyle::cornerRadius,    atoi>},
    {"layout", "imageCornerRadius", [](PresentationStyle& s, const char* v) {
        s.imageCornerRadius = std::max(0, std::atoi(v));
    }},
    {"layout", "presenterCornerRadius", parseAttr<&PresentationStyle::presenterCornerRadius, atoi>},
    {"layout", "bulletGap",       parseAttr<&PresentationStyle::bulletGap,       atoi>},
};

void PresentationStyle::applyXmlElement(const void* el) {
    auto* root = static_cast<const tinyxml2::XMLElement*>(el);
    if (!root) return;
    if (const char* theme = root->Attribute("theme")) {
        for (const auto& preset : builtInThemes()) {
            if (preset.name == theme) { *this = preset; break; }
        }
    }
    if (auto* fonts = root->FirstChildElement("fonts")) {
        if (fonts->Attribute("boldTitles"))
            boldTitles = fonts->BoolAttribute("boldTitles", boldTitles);
    }
    if (const char* customName = root->Attribute("name")) name = customName;
    for (auto& [elem, attr, set] : STYLE_ATTRS) {
        auto* child = root->FirstChildElement(elem);
        if (!child) continue;
        const char* v = child->Attribute(attr);
        if (v) set(*this, v);
    }

    // A custom slide radius keeps the presenter radius at half unless the
    // presenter value is explicitly configured.
    auto* layout = root->FirstChildElement("layout");
    if (layout && layout->Attribute("cornerRadius") &&
        !layout->Attribute("presenterCornerRadius")) {
        presenterCornerRadius = cornerRadius / 2;
    }
}

PresentationStyle PresentationStyle::defaults() {
    return builtInThemes().front();
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
    float titleFs, float subtitleFs, float contentFs, float smallFs, float childTitleFs,
    Color bg, Color text, Color title, Color subtitle, Color accent, Color dim, Color line,
    Color codeBg, Color codeBorder, Color codeText,
    Color codeKeyword, Color codeType, Color codeString,
    Color codeComment, Color codeNumber, Color codeBuiltin, Color codePunctuation,
    int margin, int padding, int gap, int colGap, int linePad, int presMargin, int cornerRad)
{
    PresentationStyle s;
    s.name = name;
    s.titleFontSize = titleFs;
    s.subtitleFontSize = subtitleFs;
    s.contentFontSize = contentFs;
    s.smallFontSize = smallFs;
    s.childTitleFontSize = childTitleFs;
    s.bgColor = bg;
    s.bgColor2 = bg;
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
    s.chartSeries1 = accent;
    s.chartSeries2 = codeType;
    s.chartSeries3 = codeString;
    s.chartSeries4 = codeNumber;
    s.chartSeries5 = codeKeyword;
    s.chartSeries6 = codeBuiltin;
    s.chartGrid = line;
    s.chartLabel = text;
    s.slideMargin = margin;
    s.partPadding = padding;
    s.partGap = gap;
    s.columnGap = colGap;
    s.linePadding = linePad;
    s.presenterMargin = presMargin;
    s.cornerRadius = cornerRad;
    s.presenterCornerRadius = cornerRad / 2;
    return s;
}

// Editorial presets share bundled Inter Bold headings and JetBrains Mono code.
// Separate chart colors avoid repeating the accent as a second data series.
static PresentationStyle editorialTheme(const char* name, Color bg, Color glow,
    Color title, Color text, Color muted, Color accent, Color panel, Color border,
    Color teal, Color rose, Color violet, Color orange, Color green) {
    auto s = makeTheme(name, 88, 36, 32, 20, 48,
        bg, text, title, muted, accent, muted, border,
        panel, border, text, rose, teal, green, muted, orange, violet, text,
        56, 24, 28, 32, 8, 20, 28);
    s.boldTitles = true;
    if (std::string(name) == "Porcelain") {
        s.titleFamily = "Source Serif 4";
        s.bodyFamily = "Source Sans 3";
    } else if (std::string(name) == "Tidal") {
        s.titleFamily = s.bodyFamily = "Source Sans 3";
    } else if (std::string(name) == "Ember") {
        s.titleFamily = "Source Serif 4";
    }
    s.subtitleColor = accent;
    s.bulletFontSize = 40;
    s.bgColor2 = glow;
    s.chartSeries1 = accent;
    s.chartSeries2 = teal;
    s.chartSeries3 = rose;
    s.chartSeries4 = violet;
    s.chartSeries5 = orange;
    s.chartSeries6 = green;
    return s;
}

const std::vector<PresentationStyle>& PresentationStyle::builtInThemes() {
    static const std::vector<PresentationStyle> themes = {
        editorialTheme("Studio", "#090C16", "#302943", "#F5F3EE", "#DDDCE5",
            "#AAA7B8", "#FFD166", "#171A2B", "#393C51",
            "#6EE7DF", "#FF91B6", "#B5A4FF", "#FFAD80", "#A3DCAD"),
        editorialTheme("Porcelain", "#FAF7F0", "#E9DFED", "#252332", "#41404E",
            "#696475", "#88502D", "#F0EBE3", "#D6CED0",
            "#167B7B", "#B43D63", "#7253AB", "#B15D26", "#46743D"),
        editorialTheme("Tidal", "#081E27", "#164956", "#EFFAF5", "#D0E4E4",
            "#9FBCC0", "#8AE6CC", "#102F3A", "#34525C",
            "#7DC9FF", "#F9A7B5", "#B7AFF4", "#F4C184", "#C3DA8B"),
        editorialTheme("Ember", "#21131C", "#59303C", "#FFF3E8", "#EAD8D7",
            "#C1A5AE", "#FFBE85", "#321E2A", "#62404C",
            "#9EDBD4", "#F298BA", "#CAAEF4", "#EBDD91", "#BADAAD"),
        // Classic palettes remain available after the editorial presets.
        makeTheme("Dracula",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#282A36", "#F8F8F2", "#FF79C6",
            "#6272A4", "#BD93F9", "#6272A4", "#44475A",
            "#21222C", "#44475A", "#F8F8F2",
            "#FF79C6", "#8BE9FD", "#F1FA8C",
            "#6272A4", "#BD93F9", "#FFB86C",
            "#F8F8F2",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 2. Monokai (dark)
        makeTheme("Monokai",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#272822", "#F8F8F2", "#F92672",
            "#75715E", "#A6E22E", "#75715E", "#3E3D32",
            "#1E1F1C", "#3E3D32", "#F8F8F2",
            "#F92672", "#66D9EF", "#E6DB74",
            "#75715E", "#AE81FF", "#FD971F",
            "#F8F8F2",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 3. Solarized Dark
        makeTheme("Solarized Dark",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#002B36", "#93A1A1", "#FDF6E3",
            "#586E75", "#268BD2", "#586E75", "#073642",
            "#073642", "#002B36", "#93A1A1",
            "#CB4B16", "#2AA198", "#B58900",
            "#586E75", "#6C71C4", "#D33682",
            "#93A1A1",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 4. GitHub Light
        makeTheme("GitHub Light",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#FFFFFF", "#24292E", "#0366D6",
            "#6A737D", "#0366D6", "#6A737D", "#E1E4E8",
            "#F6F8FA", "#E1E4E8", "#24292E",
            "#D73A49", "#005CC5", "#032F62",
            "#6A737D", "#005CC5", "#0550AE",
            "#24292E",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 5. Solarized Light
        makeTheme("Solarized Light",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#FDF6E3", "#586E75", "#073642",
            "#93A1A1", "#268BD2", "#93A1A1", "#EEE8D5",
            "#EEE8D5", "#FDF6E3", "#586E75",
            "#CB4B16", "#2AA198", "#B58900",
            "#93A1A1", "#6C71C4", "#D33682",
            "#586E75",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 6. Nord
        makeTheme("Nord",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#2E3440", "#D8DEE9", "#88C0D0",
            "#4C566A", "#88C0D0", "#4C566A", "#3B4252",
            "#3B4252", "#2E3440", "#D8DEE9",
            "#BF616A", "#A3BE8C", "#EBCB8B",
            "#4C566A", "#B48EAD", "#D08770",
            "#D8DEE9",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 7. Sunset
        makeTheme("Sunset",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#1A1412", "#E8D5C4", "#FF8C42",
            "#8B7355", "#FF6B6B", "#8B7355", "#2D1F18",
            "#2D1F18", "#1A1412", "#E8D5C4",
            "#FF6B6B", "#FFA500", "#FFD700",
            "#8B7355", "#FF8C42", "#FF4500",
            "#E8D5C4",
            SLIDE_MARGIN, PART_PADDING, PART_GAP, COLUMN_GAP, LINE_PADDING, PRESENTER_MARGIN, CORNER_RADIUS),
        // 8. Arc
        makeTheme("Arc",
            FONT_TITLE_SIZE, FONT_SUBTITLE_SIZE, FONT_CONTENT_SIZE, FONT_SMALL_SIZE, FONT_CHILD_TITLE_SIZE,
            "#383C4A", "#C8CCD4", "#61AFEF",
            "#545862", "#61AFEF", "#545862", "#4B4F5A",
            "#2C313A", "#383C4A", "#C8CCD4",
            "#E06C75", "#98C379", "#E5C07B",
            "#545862", "#C678DD", "#D19A66",
            "#C8CCD4",
            40, 20, 12, 24, 6, 20, CORNER_RADIUS)
    };
    return themes;
}
