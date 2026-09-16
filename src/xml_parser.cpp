#include "parser.h"
#include <tinyxml2.h>
#include <filesystem>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <set>
#include <cstdlib>
#include <cctype>

using namespace tinyxml2;

static std::string formatted(XMLNode* node) {
    std::string result;
    for (auto* c = node->FirstChild(); c; c = c->NextSibling()) {
        if (c->ToText()) result += c->Value();
        else if (auto* e = c->ToElement()) {
            std::string tag = e->Name();
            std::string attributes;
            if (tag == "code" && e->Attribute("lang")) attributes = " lang=\"" + std::string(e->Attribute("lang")) + "\"";
            result += "<" + tag + attributes + ">" + formatted(e) + "</" + tag + ">";
        }
    }
    return result;
}

static bool fail(XMLElement* el, const std::string& message) {
    fprintf(stderr, "[xml_parser] line %d <%s>: %s\n", el->GetLineNum(), el->Name(), message.c_str());
    return false;
}

static bool numbers(const std::string& value, bool tracks = false) {
    std::istringstream input(value);
    std::string token;
    int count = 0;
    while (input >> token) {
        ++count;
        if (tracks && token == "auto") continue;
        bool star = tracks && token.back() == '*';
        if (star) token.pop_back();
        if (star && token.empty()) continue;
        char* end = nullptr;
        double n = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end || !std::isfinite(n) || n < 0 || n > 100000 ||
            (star && n <= 0) || (!star && n != std::floor(n))) return false;
    }
    return count > 0 && count <= 256;
}

static bool parseNode(XMLElement* el, const std::filesystem::path& base, LayoutNode& node, int depth = 0) {
    if (depth > 64) return fail(el, "layout nesting exceeds 64 levels");
    node.kind = el->Name();
    const std::set<std::string> kinds = {"stack", "grid", "border", "text", "image", "code", "chart", "icon"};
    if (!kinds.count(node.kind)) return fail(el, "unknown layout element");
    const std::set<std::string> common = {"width", "height", "minWidth", "minHeight", "maxWidth", "maxHeight",
        "margin", "horizontalAlignment", "verticalAlignment", "row", "column", "rowSpan", "columnSpan"};
    const std::map<std::string, std::set<std::string>> specific = {
        {"stack", {"orientation", "gap"}}, {"grid", {"rows", "columns", "gap"}},
        {"border", {"padding", "background", "borderColor", "cornerRadius"}},
        {"text", {"role", "color", "wrap", "textAlignment"}},
        {"image", {"src", "alt", "fit"}}, {"code", {"lang"}},
        {"icon", {"name"}}, {"chart", {"type", "title", "icon", "showValues"}}
    };
    const std::set<std::string> numeric = {"width", "height", "minWidth", "minHeight", "maxWidth", "maxHeight",
        "gap", "cornerRadius", "row", "column", "rowSpan", "columnSpan"};
    for (auto* a = el->FirstAttribute(); a; a = a->Next()) {
        std::string name = a->Name(), value = a->Value();
        if (!common.count(name) && !specific.at(node.kind).count(name))
            return fail(el, "unknown attribute: " + name);
        if (numeric.count(name) && (!numbers(value) || value.find_first_of(" \t\n") != std::string::npos))
            return fail(el, name + " must be a nonnegative integer");
        if ((name == "rowSpan" || name == "columnSpan") && std::atoi(value.c_str()) < 1)
            return fail(el, "spans must be positive");
        if ((name == "rows" || name == "columns") && !numbers(value, true))
            return fail(el, "tracks must be space-separated auto, pixels, *, or weighted stars");
        if (name == "margin" || name == "padding") {
            std::istringstream input(value); int v, count = 0;
            while (input >> v) ++count;
            if (!numbers(value) || (count != 1 && count != 2 && count != 4))
                return fail(el, name + " needs 1, 2, or 4 nonnegative integers");
        }
        if ((name == "horizontalAlignment" || name == "verticalAlignment" || name == "textAlignment") &&
            value != "start" && value != "center" && value != "end" && value != "stretch")
            return fail(el, "alignment must be start, center, end, or stretch");
        if (name == "orientation" && value != "vertical" && value != "horizontal") return fail(el, "invalid orientation");
        if (name == "fit" && value != "fit" && value != "fill") return fail(el, "invalid image fit");
        if ((name == "wrap" || name == "showValues") && value != "true" && value != "false") return fail(el, "expected true or false");
        if (name == "role" && value != "title" && value != "subtitle" && value != "body" && value != "small" && value != "heading")
            return fail(el, "unknown text role");
        if (name == "type" && value != "bar" && value != "line" && value != "pie" && value != "donut")
            return fail(el, "unknown chart type");
        if (name == "color" || name == "background" || name == "borderColor") {
            const std::set<std::string> palette = {"background", "panel", "accent", "title", "muted", "line", "text"};
            bool hex = value.size() == 7 && value[0] == '#';
            for (size_t i = 1; hex && i < value.size(); ++i)
                hex = std::isxdigit(static_cast<unsigned char>(value[i]));
            if (!hex && !palette.count(value)) return fail(el, "unknown color: " + value);
        }
        node.attributes[name] = value;
    }
    if (node.kind == "image") {
        if (!el->Attribute("src")) return fail(el, "image requires src");
        node.attributes["src"] = (base / el->Attribute("src")).lexically_normal().string();
    }
    if (node.kind == "text" || node.kind == "icon") node.text = formatted(el);
    if (node.kind == "code") node.text = el->GetText() ? el->GetText() : "";
    if (node.kind == "chart") {
        auto& chart = node.chart;
        std::string type = el->Attribute("type") ? el->Attribute("type") : "bar";
        chart.type = type == "line" ? ChartType::Line : type == "pie" ? ChartType::Pie :
                     type == "donut" ? ChartType::Donut : ChartType::Bar;
        if (el->Attribute("title")) chart.title = el->Attribute("title");
        if (el->Attribute("icon")) chart.icon = el->Attribute("icon");
        chart.height = el->IntAttribute("height", 340);
        chart.showValues = el->BoolAttribute("showValues", true);
    }
    bool container = node.kind == "stack" || node.kind == "grid" || node.kind == "border";
    if (container) for (auto* c = el->FirstChild(); c; c = c->NextSibling()) {
        if (c->ToText() && std::string(c->Value()).find_first_not_of(" \t\r\n") != std::string::npos)
            return fail(el, "put visible text inside a text element");
    }
    for (auto* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        if (container) {
            LayoutNode child;
            if (!parseNode(c, base, child, depth + 1)) return false;
            if (node.kind != "grid") for (const char* key : {"row", "column", "rowSpan", "columnSpan"})
                if (child.attributes.count(key)) return fail(c, "cell properties require a grid parent");
            node.children.push_back(std::move(child));
        } else if (node.kind == "chart" && std::string(c->Name()) == "point") {
            double value;
            if (c->QueryDoubleAttribute("value", &value) != XML_SUCCESS || !std::isfinite(value))
                return fail(c, "point requires a finite value");
            node.chart.points.push_back({c->Attribute("label") ? c->Attribute("label") : "", value});
        } else if ((node.kind == "text" || node.kind == "icon") &&
                   (std::string(c->Name()) == "b" || std::string(c->Name()) == "i" || std::string(c->Name()) == "code")) {
            // Inline formatting is serialized, not a layout child.
        } else return fail(c, "unexpected child element");
    }
    if (node.kind == "border" && node.children.size() != 1) return fail(el, "border requires exactly one child");
    for (const char* dimension : {"Width", "Height"}) {
        std::string minKey = std::string("min") + dimension, maxKey = std::string("max") + dimension;
        if (node.attributes.count(minKey) && node.attributes.count(maxKey) &&
            std::atoi(node.attributes[minKey].c_str()) > std::atoi(node.attributes[maxKey].c_str()))
            return fail(el, minKey + " exceeds " + maxKey);
    }
    if (node.kind == "grid") {
        auto trackCount = [&](const char* key) {
            std::istringstream input(node.attributes.count(key) ? node.attributes[key] : "*");
            std::string token; int count = 0; while (input >> token) ++count; return count;
        };
        for (const auto& child : node.children) {
            auto number = [&](const char* key, int fallback) {
                auto it = child.attributes.find(key);
                return it == child.attributes.end() ? fallback : std::atoi(it->second.c_str());
            };
            if (number("row", 0) + number("rowSpan", 1) > trackCount("rows") ||
                number("column", 0) + number("columnSpan", 1) > trackCount("columns"))
                return fail(el, "child cell/span extends beyond declared grid tracks");
        }
    }
    return true;
}

Presentation parseXml(const std::string& filePath) {
    std::filesystem::path path(filePath);
    std::error_code error;
    if (std::filesystem::is_directory(path, error)) path /= "presentation.xml";
    XMLDocument doc;
    if (doc.LoadFile(path.string().c_str()) != XML_SUCCESS) {
        fprintf(stderr, "[xml_parser] %s: %s\n", path.string().c_str(), doc.ErrorStr());
        return {};
    }
    auto* root = doc.FirstChildElement("presentation");
    if (!root) return {};
    Presentation pres;
    if (root->Attribute("name")) pres.name = root->Attribute("name");
    if (root->Attribute("style")) pres.style = PresentationStyle::load((path.parent_path() / root->Attribute("style")).string());
    if (auto* style = root->FirstChildElement("style")) pres.style.applyXmlElement(style);
    for (auto* el = root->FirstChildElement(); el; el = el->NextSiblingElement()) {
        if (std::string(el->Name()) == "style") continue;
        if (std::string(el->Name()) != "slide") { fail(el, "presentation expects slides"); return {}; }
        Slide slide;
        bool hasNotes = false;
        for (auto* a = el->FirstAttribute(); a; a = a->Next()) {
            if (std::string(a->Name()) != "title") {
                fail(el, "slides accept only title metadata; replace presets with a stack, grid, or border root");
                return {};
            }
            slide.title = a->Value();
        }
        for (auto* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
            if (std::string(c->Name()) == "notes") {
                if (hasNotes || !slide.elements.empty() || c->FirstChildElement()) {
                    fail(c, "notes must be plain text, once, before the layout root"); return {};
                }
                hasNotes = true;
                slide.notes = c->GetText() ? c->GetText() : ""; continue;
            }
            if (!slide.elements.empty() || (std::string(c->Name()) != "stack" && std::string(c->Name()) != "grid" && std::string(c->Name()) != "border")) {
                fail(c, "slide requires exactly one stack, grid, or border root");
                return {};
            }
            LayoutNode node;
            if (!parseNode(c, path.parent_path(), node)) return {};
            for (const char* key : {"row", "column", "rowSpan", "columnSpan"})
                if (node.attributes.count(key)) { fail(c, "root has no grid parent"); return {}; }
            slide.elements.push_back(std::move(node));
        }
        if (slide.elements.empty()) { fail(el, "missing layout root"); return {}; }
        pres.slides.push_back(std::move(slide));
    }
    return pres;
}
