#include "style.h"
#include <tinyxml2.h>
#include <cstdio>

static Color parseHexColor(const char* s) {
    Color c = {0, 0, 0, 255};
    if (!s || !*s || s[0] != '#') return c;
    unsigned int r, g, b;
    if (std::sscanf(s, "#%02x%02x%02x", &r, &g, &b) == 3) {
        c.r = (unsigned char)r; c.g = (unsigned char)g; c.b = (unsigned char)b;
    }
    return c;
}

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
    // colors
    {"colors", "bg",       parseAttr<&PresentationStyle::bgColor,       parseHexColor>},
    {"colors", "text",     parseAttr<&PresentationStyle::textColor,     parseHexColor>},
    {"colors", "title",    parseAttr<&PresentationStyle::titleColor,    parseHexColor>},
    {"colors", "subtitle", parseAttr<&PresentationStyle::subtitleColor, parseHexColor>},
    {"colors", "accent",   parseAttr<&PresentationStyle::accentColor,   parseHexColor>},
    {"colors", "dim",      parseAttr<&PresentationStyle::dimColor,      parseHexColor>},
    {"colors", "line",     parseAttr<&PresentationStyle::lineColor,     parseHexColor>},
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
