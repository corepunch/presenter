#include "layout.h"
#include <sstream>
#include <cstdlib>

namespace {
std::string attr(const LayoutNode& node, const char* key, const std::string& fallback = "") {
    auto it = node.attributes.find(key);
    return it == node.attributes.end() ? fallback : it->second;
}
int number(const LayoutNode& node, const char* key, int fallback = 0) {
    auto it = node.attributes.find(key);
    return it == node.attributes.end() ? fallback : std::atoi(it->second.c_str());
}
ui::Thickness thickness(const std::string& value) {
    std::istringstream input(value);
    std::vector<int> v; int n;
    while (input >> n) v.push_back(n);
    if (v.size() == 4) return {v[0], v[1], v[2], v[3]};
    if (v.size() == 2) return {v[0], v[1]};
    return ui::Thickness(v.empty() ? 0 : v[0]);
}
ui::Alignment alignment(const std::string& value) {
    if (value == "start") return ui::Alignment::Start;
    if (value == "center") return ui::Alignment::Center;
    if (value == "end") return ui::Alignment::End;
    return ui::Alignment::Stretch;
}
Color color(const std::string& value, const PresentationStyle& s) {
    if (value == "background") return s.bgColor;
    if (value == "panel") return s.codeBg;
    if (value == "accent") return s.accentColor;
    if (value == "title") return s.titleColor;
    if (value == "muted") return s.dimColor;
    if (value == "line") return s.lineColor;
    if (!value.empty() && value[0] == '#') return Color(value.c_str());
    return s.textColor;
}
std::vector<ui::Track> tracks(const std::string& value) {
    std::vector<ui::Track> result;
    std::istringstream input(value); std::string token;
    while (input >> token) {
        if (token == "auto") result.push_back({ui::Track::Unit::Auto, 0});
        else if (token.back() == '*') result.push_back({ui::Track::Unit::Star, token == "*" ? 1 : std::strtod(token.c_str(), nullptr)});
        else result.push_back({ui::Track::Unit::Pixel, std::strtod(token.c_str(), nullptr)});
    }
    if (result.empty()) result.push_back({});
    return result;
}

}

std::unique_ptr<ui::Element> buildSlideLayout(const LayoutNode& node,
        const FontSet& fonts, const PresentationStyle& style) {
    std::unique_ptr<ui::Element> result;
    if (node.kind == "stack") {
        auto stack = std::make_unique<ui::Stack>(attr(node, "orientation") == "horizontal" ?
            ui::Stack::Orientation::Horizontal : ui::Stack::Orientation::Vertical);
        stack->gap = number(node, "gap");
        for (const auto& child : node.children) stack->add(buildSlideLayout(child, fonts, style));
        result = std::move(stack);
    } else if (node.kind == "grid") {
        auto grid = std::make_unique<ui::Grid>();
        grid->rows = tracks(attr(node, "rows", "*"));
        grid->columns = tracks(attr(node, "columns", "*"));
        grid->gap = number(node, "gap");
        for (const auto& child : node.children) grid->add(buildSlideLayout(child, fonts, style),
            number(child, "row"), number(child, "column"), number(child, "rowSpan", 1), number(child, "columnSpan", 1));
        result = std::move(grid);
    } else if (node.kind == "border") {
        auto border = std::make_unique<ui::Border>();
        border->style.padding = thickness(attr(node, "padding"));
        border->style.hasBackground = node.attributes.count("background");
        border->style.background = color(attr(node, "background"), style);
        border->style.hasBorder = node.attributes.count("borderColor");
        border->style.borderColor = color(attr(node, "borderColor"), style);
        border->style.cornerRadius = number(node, "cornerRadius");
        if (!node.children.empty()) border->setChild(buildSlideLayout(node.children[0], fonts, style));
        result = std::move(border);
    } else if (node.kind == "text") {
        std::string role = attr(node, "role", "body");
        auto fv = role == "title" ? fonts.titleVariants() : role == "subtitle" ? fonts.subtitleVariants() :
            role == "heading" ? fonts.childTitleVariants() : role == "small" ? fonts.smallVariants() : fonts.variants();
        auto text = std::make_unique<ui::Text>(node.text, fv,
            color(attr(node, "color", role == "title" || role == "heading" ? "title" : "text"), style));
        text->wrap = attr(node, "wrap", "true") == "true";
        text->textAlignment = alignment(attr(node, "textAlignment", "start"));
        result = std::move(text);
    } else result = createVisualLeaf(node, fonts);
    result->margin = thickness(attr(node, "margin"));
    result->width = number(node, "width", -1);
    result->height = number(node, "height", -1);
    result->minWidth = number(node, "minWidth");
    result->minHeight = number(node, "minHeight");
    result->maxWidth = number(node, "maxWidth", ui::Unbounded);
    result->maxHeight = number(node, "maxHeight", ui::Unbounded);
    result->horizontalAlignment = alignment(attr(node, "horizontalAlignment", "stretch"));
    result->verticalAlignment = alignment(attr(node, "verticalAlignment", "stretch"));
    return result;
}
