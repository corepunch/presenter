#include "parser.h"
#include <tinyxml2.h>
#include <filesystem>
#include <algorithm>

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

static Slide parseSlideElement(tinyxml2::XMLElement* el, const std::filesystem::path& baseDir);

Presentation parseXml(const std::string& filePath) {
    using namespace tinyxml2;

    XMLDocument doc;
    if (doc.LoadFile(filePath.c_str()) != XML_SUCCESS) return {};

    XMLElement* root = doc.FirstChildElement("presentation");
    if (!root) return {};

    Presentation pres;
    std::filesystem::path base = std::filesystem::path(filePath).parent_path();

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
            tinyxml2::XMLPrinter printer(nullptr, true);
            for (tinyxml2::XMLNode* child = c->FirstChild(); child; child = child->NextSibling()) {
                child->Accept(&printer);
            }
            const char* formatted = printer.CStr();
            slide.texts.push_back(formatted ? formatted : "");
        }
        else if (name == "image") {
            const char* src = c->Attribute("src");
            const char* alt = c->Attribute("alt");
            if (src) slide.imagePath = (baseDir / src).lexically_normal().string();
            if (alt) slide.imageAlt = alt;
        }
        else if (name == "slide") {
            slide.children.push_back(parseSlideElement(c, baseDir));
        }
    }

    return slide;
}
