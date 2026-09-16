#include "check.h"
#include "layout.h"
#include "renderer.h"
#include "image.h"
#include "charts.h"
#include "stb_image.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>

int CheckReport::count(IssueSeverity severity) const {
    return static_cast<int>(std::count_if(issues.begin(), issues.end(),
        [&](const CheckIssue& i) { return i.severity == severity; }));
}
bool CheckReport::failsStrict() const {
    return count(IssueSeverity::Error) || count(IssueSeverity::Warning);
}

namespace {
const char* severityName(IssueSeverity s) {
    return s == IssueSeverity::Error ? "error" : s == IssueSeverity::Warning ? "warning" : "info";
}
std::string attr(const LayoutNode& n, const char* key) {
    auto it = n.attributes.find(key); return it == n.attributes.end() ? "" : it->second;
}
bool hasText(const std::string& s) { return s.find_first_not_of(" \t\r\n") != std::string::npos; }
ui::Rect intersect(ui::Rect a, ui::Rect b) {
    int x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0, std::min(a.x + a.width, b.x + b.width) - x),
                  std::max(0, std::min(a.y + a.height, b.y + b.height) - y)};
}
bool clipped(ui::Rect a, ui::Rect b) {
    return a.x < b.x || a.y < b.y || a.x + a.width > b.x + b.width ||
           a.y + a.height > b.y + b.height;
}
double luminance(Color c) {
    auto linear = [](double v) { v /= 255; return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b);
}
std::string decimal(double n) {
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::fixed << std::setprecision(1) << n; return out.str();
}

struct Checker {
    CheckReport& report;
    Renderer& renderer;
    const FontSet& fonts;
    LayoutElements& elements;
    int slide;
    int visibleLeaves = 0;
    const ui::Rect canvas{0, 0, SLIDE_CANVAS_WIDTH, SLIDE_CANVAS_HEIGHT};

    void visit(const LayoutNode& node, const std::string& path, ui::Rect parentClip, Color background) {
        const auto& element = *elements.at(&node);
        ui::Rect bounds = element.bounds(), visible = intersect(bounds, parentClip);
        auto issue = [&](const char* code, IssueSeverity severity, std::string message,
                         std::string suggestion, std::map<std::string, double> values = {}) {
            report.issues.push_back({slide, path, attr(node, "src"), code, std::move(message),
                std::move(suggestion), severity, bounds, std::move(values)});
        };
        bool container = node.kind == "stack" || node.kind == "grid" || node.kind == "border";
        if (!container && (bounds.width <= 0 || bounds.height <= 0))
            issue("zero_size", IssueSeverity::Warning, "Element has no display area.",
                  "Allocate space with grid tracks or explicit dimensions.");
        else if (clipped(bounds, canvas))
            issue("outside_canvas", IssueSeverity::Warning, "Element extends outside the 1280×720 slide.",
                  "Reduce content or adjust the container dimensions.");
        else if (clipped(bounds, parentClip))
            issue("clipped_by_parent", IssueSeverity::Warning, "Element extends beyond its parent's visible area.",
                  "Increase its parent's space or reduce the content.");
        else if (element.overflow && container)
            issue("layout_overflow", IssueSeverity::Warning, "Container requires more space than its allocated slot.",
                  "Increase the allocated space or reduce child sizes, gaps, or padding.");

        if (node.kind == "border") {
            const auto& border = static_cast<const ui::Border&>(element);
            if (border.style.hasBackground) background = border.style.background;
        }
        bool content = false;
        if (node.kind == "text" || node.kind == "code" || node.kind == "icon") {
            content = hasText(node.text) || (node.kind == "icon" && !attr(node, "name").empty());
            auto desired = element.contentDesiredSize();
            if (node.kind == "text") {
                const auto& text = static_cast<const ui::Text&>(element);
                // Re-evaluate at the actual final width, including alignment/maxWidth.
                auto lines = renderer.wordWrap(text.text, text.fonts, text.wrap ? std::max(1, bounds.width) : ui::Unbounded);
                desired = {0, static_cast<int>(lines.size()) * renderer.textHeight(text.fonts.get(FontType::Regular))};
                for (const auto& line : lines)
                    desired.width = std::max(desired.width, static_cast<int>(std::ceil(renderer.formattedWidth(line, text.fonts))));
                double fontSize = text.fonts.get(FontType::Regular).getFontSize();
                if (content && fontSize < 18)
                    issue("small_text", IssueSeverity::Warning, "Text is " + decimal(fontSize) + " px on the slide canvas.",
                          "Use a larger font or reduce the amount of text.", {{"fontSizePx", fontSize}});
                double a = luminance(text.color), b = luminance(background);
                double contrast = (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
                if (content && contrast < 3)
                    issue("low_contrast", IssueSeverity::Warning, "Text/background contrast is " + decimal(contrast) + ":1.",
                          "Choose more distinct text and background colors; inspect overlapping backgrounds visually.",
                          {{"contrastRatio", contrast}});
            }
            if (content && (desired.width > bounds.width || desired.height > bounds.height))
                issue(node.kind == "code" ? "code_overflow" : "text_overflow", IssueSeverity::Warning,
                      "Content requires " + std::to_string(desired.width) + "×" + std::to_string(desired.height) +
                      " px; allocated " + std::to_string(bounds.width) + "×" + std::to_string(bounds.height) + " px.",
                      "Allocate more space, shorten the content, or split it across slides.",
                      {{"requiredWidth", desired.width}, {"requiredHeight", desired.height},
                       {"allocatedWidth", bounds.width}, {"allocatedHeight", bounds.height}});
        }
        if (node.kind == "icon" || node.kind == "chart") {
            std::string name = node.kind == "icon" ? attr(node, "name") : node.chart.icon;
            if (!name.empty() && (!iconCodepoint(name) || !fonts.icons().hasGlyph(iconCodepoint(name))))
                issue("unknown_icon", IssueSeverity::Error, "Unknown or unavailable icon: " + name,
                      "Choose a bundled Font Awesome Free Solid icon.");
        }
        if (node.kind == "chart") content = !node.chart.points.empty() || hasText(node.chart.title);
        if (node.kind == "image") {
            std::string source = attr(node, "src");
            int w = 0, h = 0, channels = 0;
            unsigned char* data = stbi_load(source.c_str(), &w, &h, &channels, 4);
            if (!data) {
                std::error_code error;
                bool exists = std::filesystem::exists(source, error);
                issue(exists ? "unreadable_image" : "missing_image", IssueSeverity::Error,
                      exists ? "Image cannot be decoded." : "Image file is missing.",
                      "Fix the package-relative path or replace the image.");
            } else {
                stbi_image_free(data);
                content = true;
                auto placement = placeImage(w, h, {bounds.x, bounds.y, bounds.width, bounds.height}, attr(node, "fit") == "fill");
                const auto& crop = placement.source;
                const auto& dest = placement.destination;
                if (crop.w > 0 && crop.h > 0) {
                    double sx = double(dest.w) / crop.w, sy = double(dest.h) / crop.h;
                    double scale = std::max(sx, sy);
                    double cropped = 1.0 - (double(crop.w) * crop.h) / (double(w) * h);
                    std::map<std::string, double> values = {
                        {"sourceWidth", w}, {"sourceHeight", h}, {"displayWidth", dest.w}, {"displayHeight", dest.h},
                        {"cropWidth", crop.w}, {"cropHeight", crop.h}, {"scaleX", sx}, {"scaleY", sy},
                        {"scalePercent", scale * 100}, {"croppedFraction", cropped}};
                    std::string geometry = "Source " + std::to_string(w) + "×" + std::to_string(h) +
                        " px; displayed " + std::to_string(dest.w) + "×" + std::to_string(dest.h) + " px. ";
                    if (scale >= 2)
                        issue("image_upscaled", IssueSeverity::Warning, geometry + "Enlarged to " +
                              decimal(scale * 100) + "% of sampled source dimensions; may appear blurry.",
                              "Reduce the image's display size or use a higher-resolution source.", values);
                    else if (scale <= 0.1)
                        issue("image_downscaled", IssueSeverity::Info, geometry + "Reduced to " +
                              decimal(scale * 100) + "% of source dimensions.",
                              "If this contains fine detail or labels, inspect readability or give it more space.", values);
                    if (cropped > 0.5)
                        issue("image_cropped", IssueSeverity::Warning, geometry + decimal(cropped * 100) + "% of source area is cropped.",
                              "Use fit instead of fill, or change the image/container aspect ratio if important details are lost.", values);
                }
            }
        }
        if (content && visible.width > 0 && visible.height > 0) ++visibleLeaves;
        std::map<std::string, int> siblings;
        for (const auto& child : node.children) {
            int index = ++siblings[child.kind];
            visit(child, path + "/" + child.kind + "[" + std::to_string(index) + "]", visible, background);
        }
    }
};

std::string quote(const std::string& s) {
    std::ostringstream out; out << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out << '\\' << char(c);
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << char(c);
    }
    out << '"'; return out.str();
}
}

CheckReport checkPresentation(const Presentation& pres, const FontSet& fonts, Renderer& renderer, int selectedSlide) {
    CheckReport report;
    renderer.setStyle(&pres.style);
    for (int i = 0; i < pres.size(); ++i) {
        if (selectedSlide && selectedSlide != i + 1) continue;
        ++report.slidesChecked;
        const auto& slide = pres.slides[i];
        LayoutElements elements;
        Checker checker{report, renderer, fonts, elements, i + 1};
        if (!slide.elements.empty()) {
            const auto& node = slide.elements.front();
            auto root = buildSlideLayout(node, fonts, pres.style, &elements);
            ui::LayoutContext context{renderer};
            root->measure(context, {SLIDE_CANVAS_WIDTH, SLIDE_CANVAS_HEIGHT});
            root->arrange(context, checker.canvas);
            checker.visit(node, "/slide[" + std::to_string(i + 1) + "]/" + node.kind + "[1]", checker.canvas, pres.style.bgColor);
        }
        if (checker.visibleLeaves == 0)
            report.issues.push_back({i + 1, "/slide[" + std::to_string(i + 1) + "]", "", "empty_slide",
                "Slide has no visible content.", "Add visible content or remove this slide.",
                IssueSeverity::Warning, checker.canvas, {}});
    }
    return report;
}

std::string formatCheckReport(const CheckReport& report, bool json) {
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(8);
    if (json) {
        out << "{\"schemaVersion\":1,\"slidesChecked\":" << report.slidesChecked
            << ",\"summary\":{\"errors\":" << report.count(IssueSeverity::Error)
            << ",\"warnings\":" << report.count(IssueSeverity::Warning)
            << ",\"info\":" << report.count(IssueSeverity::Info) << "},\"issues\":[";
        bool first = true;
        for (const auto& i : report.issues) {
            if (!first) out << ',';
            first = false;
            out << "{\"slide\":" << i.slide << ",\"element\":" << quote(i.element)
                << ",\"source\":" << quote(i.source) << ",\"code\":" << quote(i.code)
                << ",\"severity\":" << quote(severityName(i.severity))
                << ",\"message\":" << quote(i.message) << ",\"suggestion\":" << quote(i.suggestion)
                << ",\"bounds\":{\"x\":" << i.bounds.x << ",\"y\":" << i.bounds.y
                << ",\"width\":" << i.bounds.width << ",\"height\":" << i.bounds.height << "},\"measurements\":{";
            bool firstValue = true;
            for (const auto& value : i.measurements) {
                if (!firstValue) out << ',';
                firstValue = false;
                out << quote(value.first) << ':' << value.second;
            }
            out << "}}";
        }
        out << "]}\n";
    } else {
        out << "Checked " << report.slidesChecked << " slide(s): " << report.count(IssueSeverity::Error)
            << " error(s), " << report.count(IssueSeverity::Warning) << " warning(s), "
            << report.count(IssueSeverity::Info) << " informational issue(s).\n";
        for (const auto& i : report.issues) {
            out << "\n" << severityName(i.severity) << " [" << i.code << "] slide " << i.slide << " · " << i.element;
            if (!i.source.empty()) out << " · " << quote(i.source);
            out << "\n  " << i.message;
            if (!i.suggestion.empty()) out << "\n  Suggestion: " << i.suggestion;
            out << '\n';
        }
        if (report.issues.empty()) out << "No issues found.\n";
    }
    return out.str();
}
