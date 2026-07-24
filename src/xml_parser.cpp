#include "parser.h"
#include <tinyxml2.h>
#include <filesystem>
#include <algorithm>
#include <cstdio>

static SlideLayout parseSlideLayout(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "title")   return SlideLayout::Title;
    if (lower == "content") return SlideLayout::Content;
    if (lower == "image")   return SlideLayout::Image;
    if (lower == "columns") return SlideLayout::Columns;
    if (lower == "section") return SlideLayout::Section;
    if (lower == "blank")   return SlideLayout::Blank;
    return SlideLayout::Content;
}

static ChartType parseChartType(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "line") return ChartType::Line;
    if (lower == "pie") return ChartType::Pie;
    if (lower == "donut" || lower == "circular") return ChartType::Donut;
    return ChartType::Bar;
}

// --- Table-driven slide attribute setters ---

template<auto Member, auto Convert>
void parseSlideAttr(Slide& s, const char* v) {
    using T = std::remove_reference_t<decltype(s.*Member)>;
    s.*Member = (T)Convert(v);
}

static int atoiMin1(const char* v) { return std::max(1, std::atoi(v)); }
static int atoiMin0(const char* v) { return std::max(0, std::atoi(v)); }

struct SlideAttr { const char* name; void (*set)(Slide&, const char*); };
static const SlideAttr SLIDE_ATTRS[] = {
    {"cols", parseSlideAttr<&Slide::cols, atoiMin1>},
    {"gap",  parseSlideAttr<&Slide::gap, atoiMin0>},
};

// ---

// Recursively serialize formatted text content: preserves inline tags as
// XML markers, but outputs decoded text so &lt; &gt; inside <code> render correctly.
static std::string serializeFormatted(tinyxml2::XMLNode* node) {
    std::string out;
    for (tinyxml2::XMLNode* child = node->FirstChild(); child; child = child->NextSibling()) {
        if (auto* txt = child->ToText()) {
            out += txt->Value();
        } else if (auto* el = child->ToElement()) {
            std::string name = el->Name();
            if (name == "b" || name == "i") {
                out += "<" + name + ">";
                out += serializeFormatted(el);
                out += "</" + name + ">";
            } else if (name == "code") {
                const char* lang = el->Attribute("lang");
                out += "<code";
                if (lang) out += " lang=\"" + std::string(lang) + "\"";
                out += ">";
                out += serializeFormatted(el);
                out += "</code>";
            }
        }
    }
    return out;
}

static Slide parseSlideElement(tinyxml2::XMLElement* el, const std::filesystem::path& baseDir);

Presentation parseXml(const std::string& filePath) {
    using namespace tinyxml2;

    std::filesystem::path sourcePath(filePath);
    std::error_code statusError;
    if (std::filesystem::is_directory(sourcePath, statusError)) {
        sourcePath /= "presentation.xml";
    }

    XMLDocument doc;
    XMLError error = doc.LoadFile(sourcePath.string().c_str());
    if (error != XML_SUCCESS) {
        fprintf(stderr, "[xml_parser] Failed to load %s: %s\n",
                sourcePath.string().c_str(), doc.ErrorStr());
        return {};
    }

    XMLElement* root = doc.FirstChildElement("presentation");
    if (!root) return {};

    Presentation pres;
    std::filesystem::path base = sourcePath.parent_path();
    if (const char* name = root->Attribute("name")) pres.name = name;

    const char* stylePath = root->Attribute("style");
    if (stylePath) {
        pres.style = PresentationStyle::load((base / stylePath).string());
    }

    XMLElement* inlineStyle = root->FirstChildElement("style");
    if (inlineStyle) {
        pres.style.applyXmlElement(inlineStyle);
    }

    for (XMLElement* s = root->FirstChildElement("slide"); s; s = s->NextSiblingElement("slide")) {
        pres.slides.push_back(parseSlideElement(s, base));
    }

    return pres;
}

static Slide parseSlideElement(tinyxml2::XMLElement* el, const std::filesystem::path& baseDir) {
    using namespace tinyxml2;
    Slide slide;

    const char* layoutAttr = el->Attribute("layout");
    slide.layout = layoutAttr ? parseSlideLayout(layoutAttr) : SlideLayout::Content;

    const char* titleAttr = el->Attribute("title");
    if (titleAttr) slide.title = titleAttr;

    const char* slotAttr = el->Attribute("slot");
    if (slotAttr) slide.slot = slotAttr;

    for (auto& [name, set] : SLIDE_ATTRS) {
        const char* v = el->Attribute(name);
        if (v) set(slide, v);
    }

    const char* fitAttr = el->Attribute("fit");
    if (fitAttr && std::string(fitAttr) == "fill") slide.imageFit = ImageFit::Fill;

    for (XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        std::string name = c->Name();
        const char* text = c->GetText();
        std::string val = text ? text : "";

        if (name == "notes")      { slide.notes = val; }
        else if (name == "subtitle") { slide.subtitle = val; }
        else if (name == "text")  {
            slide.texts.push_back(serializeFormatted(c));
            const char* icon = c->Attribute("icon");
            slide.textIcons.push_back(icon ? icon : "");
        }
        else if (name == "image") {
            const char* src = c->Attribute("src");
            const char* alt = c->Attribute("alt");
            if (src) slide.imagePath = (baseDir / src).lexically_normal().string();
            if (alt) slide.imageAlt = alt;
        }
        else if (name == "code") {
            const char* langAttr = c->Attribute("lang");
            std::string lang = langAttr ? langAttr : "";
            const char* codeText = c->GetText();
            slide.codeBlocks.push_back({codeText ? codeText : "", lang});
        }
        else if (name == "chart") {
            Chart chart;
            if (const char* type = c->Attribute("type"))
                chart.type = parseChartType(type);
            if (const char* title = c->Attribute("title"))
                chart.title = title;
            if (const char* icon = c->Attribute("icon"))
                chart.icon = icon;
            chart.height = std::max(160, c->IntAttribute("height", chart.height));
            chart.showValues = c->BoolAttribute("showValues", true);
            for (XMLElement* p = c->FirstChildElement("point"); p;
                 p = p->NextSiblingElement("point")) {
                ChartPoint point;
                if (const char* label = p->Attribute("label")) point.label = label;
                point.value = p->DoubleAttribute("value", 0.0);
                chart.points.push_back(point);
            }
            slide.charts.push_back(chart);
        }
        else if (name == "icon") {
            IconBlock icon;
            if (const char* iconName = c->Attribute("name")) icon.name = iconName;
            icon.text = serializeFormatted(c);
            slide.icons.push_back(icon);
        }
        else if (name == "slide") {
            slide.children.push_back(parseSlideElement(c, baseDir));
        }
    }

    return slide;
}
