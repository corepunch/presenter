#include "charts.h"

#include "font.h"
#include "renderer.h"
#include "style.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

constexpr double PI = 3.14159265358979323846;

Color seriesColor(const PresentationStyle& style, size_t index) {
    switch (index % 6) {
        case 0: return style.chartSeries1;
        case 1: return style.chartSeries2;
        case 2: return style.chartSeries3;
        case 3: return style.chartSeries4;
        case 4: return style.chartSeries5;
        default: return style.chartSeries6;
    }
}

std::string formatValue(double value) {
    char buffer[32];
    if (std::fabs(value - std::round(value)) < 0.001)
        std::snprintf(buffer, sizeof(buffer), "%.0f", value);
    else
        std::snprintf(buffer, sizeof(buffer), "%.1f", value);
    return buffer;
}

double niceAxisMaximum(double value) {
    if (value <= 0.0) return 1.0;
    double magnitude = std::pow(10.0, std::floor(std::log10(value)));
    double normalized = value / magnitude;
    double rounded = normalized <= 1.0 ? 1.0 :
                     normalized <= 2.0 ? 2.0 :
                     normalized <= 5.0 ? 5.0 : 10.0;
    return rounded * magnitude;
}

std::string fitLabel(const std::string& label, const Font& font, int width) {
    if (font.measureString(label) <= width) return label;
    std::string result = label;
    while (!result.empty() &&
           font.measureString(result + "\xE2\x80\xA6") > width) {
        result.pop_back();
    }
    return result + "\xE2\x80\xA6";
}

void drawCentered(Renderer* renderer, const std::string& text, int centerX,
                  float baseline, const Font& font, Color color) {
    renderer->drawText(text, centerX - font.measureString(text) / 2.0f,
                       baseline, font, color.toSDLColor());
}

void drawLine(SDL_Surface* surface, int x0, int y0, int x1, int y1,
              Color color, int thickness = 1) {
    if (!surface) return;
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    Uint32 pixel = color.toUint32(surface->format);
    int half = thickness / 2;
    while (true) {
        SDL_Rect dot = {x0 - half, y0 - half, thickness, thickness};
        SDL_FillRect(surface, &dot, pixel);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fillCircle(SDL_Surface* surface, int cx, int cy, int radius, Color color) {
    Uint32 pixel = color.toUint32(surface->format);
    for (int dy = -radius; dy <= radius; ++dy) {
        int halfWidth = static_cast<int>(
            std::sqrt(static_cast<double>(radius * radius - dy * dy)));
        SDL_Rect line = {cx - halfWidth, cy + dy, 2 * halfWidth + 1, 1};
        SDL_FillRect(surface, &line, pixel);
    }
}

void renderChartHeading(Renderer* renderer, SDL_Surface* surface,
                        const Chart& chart, const FontSet& fonts,
                        int x, int y, int width) {
    const auto& style = renderer->style();
    const Font& titleFont = fonts.get(FontType::Bold);
    int cursorX = x;
    uint32_t codepoint = iconCodepoint(chart.icon);
    if (codepoint && fonts.icons().hasGlyph(codepoint)) {
        fonts.icons().drawGlyph(surface, codepoint, static_cast<float>(cursorX),
                                y + titleFont.getAscent(),
                                style.chartSeries1.toSDLColor());
        cursorX += static_cast<int>(fonts.icons().getFontSize()) + style.partGap / 2;
    }
    std::string title = fitLabel(chart.title, titleFont, width - (cursorX - x));
    renderer->drawText(title, static_cast<float>(cursorX),
                       y + titleFont.getAscent(), titleFont,
                       style.titleColor.toSDLColor());
}

void renderAxesChart(Renderer* renderer, SDL_Surface* surface,
                     const Chart& chart, const FontSet& fonts,
                     int x, int y, int width, int height) {
    const auto& style = renderer->style();
    const Font& labelFont = fonts.smallVariants().get(FontType::Regular);
    const Font& valueFont = fonts.smallVariants().get(FontType::Bold);
    int headingH = chart.title.empty() ? 0 :
        static_cast<int>(fonts.get(FontType::Bold).getAscent() -
                         fonts.get(FontType::Bold).getDescent()) + style.partGap;
    int left = x + 48;
    int right = x + width - 18;
    int top = y + headingH + 14;
    int bottom = y + height - 44;
    if (right <= left || bottom <= top || chart.points.empty()) return;

    double maxValue = 0.0;
    for (const auto& point : chart.points)
        maxValue = std::max(maxValue, point.value);
    maxValue = niceAxisMaximum(maxValue);

    for (int i = 0; i <= 5; ++i) {
        int gy = bottom - (bottom - top) * i / 5;
        drawLine(surface, left, gy, right, gy, style.chartGrid);
        std::string value = formatValue(maxValue * i / 5.0);
        renderer->drawText(value,
                           left - labelFont.measureString(value) - 8,
                           gy + labelFont.getAscent() / 2.0f,
                           labelFont, style.chartLabel.toSDLColor());
    }

    int plotW = right - left;
    int plotH = bottom - top;
    if (chart.type == ChartType::Bar) {
        int slotW = std::max(1, plotW / static_cast<int>(chart.points.size()));
        int barW = std::max(6, std::min(slotW * 3 / 5, 90));
        for (size_t i = 0; i < chart.points.size(); ++i) {
            const ChartPoint& point = chart.points[i];
            int centerX = left + slotW * static_cast<int>(i) + slotW / 2;
            int barH = static_cast<int>(
                std::max(0.0, point.value) / maxValue * plotH);
            renderer->fillRect({centerX - barW / 2, bottom - barH, barW, barH},
                               seriesColor(style, i),
                               std::min(style.cornerRadius, barW / 3));
            if (chart.showValues) {
                drawCentered(renderer, formatValue(point.value), centerX,
                             bottom - barH - 7, valueFont,
                             style.chartLabel);
            }
            std::string label = fitLabel(point.label, labelFont, slotW - 4);
            drawCentered(renderer, label, centerX,
                         bottom + labelFont.getAscent() + 9,
                         labelFont, style.chartLabel);
        }
        return;
    }

    int count = static_cast<int>(chart.points.size());
    int previousX = 0;
    int previousY = 0;
    for (int i = 0; i < count; ++i) {
        const ChartPoint& point = chart.points[static_cast<size_t>(i)];
        int px = count == 1 ? left + plotW / 2 :
            left + plotW * i / (count - 1);
        int py = bottom - static_cast<int>(
            std::max(0.0, point.value) / maxValue * plotH);
        if (i > 0)
            drawLine(surface, previousX, previousY, px, py,
                     style.chartSeries1, 4);
        fillCircle(surface, px, py, 6, style.chartSeries2);
        fillCircle(surface, px, py, 3, style.chartSeries1);
        if (chart.showValues) {
            drawCentered(renderer, formatValue(point.value), px,
                         py - 10, valueFont, style.chartLabel);
        }
        std::string label = fitLabel(point.label, labelFont,
                                     std::max(30, plotW / count - 4));
        drawCentered(renderer, label, px,
                     bottom + labelFont.getAscent() + 9,
                     labelFont, style.chartLabel);
        previousX = px;
        previousY = py;
    }
}

void renderCircularChart(Renderer* renderer, SDL_Surface* surface,
                         const Chart& chart, const FontSet& fonts,
                         int x, int y, int width, int height) {
    const auto& style = renderer->style();
    const Font& labelFont = fonts.smallVariants().get(FontType::Regular);
    const Font& valueFont = fonts.smallVariants().get(FontType::Bold);
    int headingH = chart.title.empty() ? 0 :
        static_cast<int>(fonts.get(FontType::Bold).getAscent() -
                         fonts.get(FontType::Bold).getDescent()) + style.partGap;
    int bodyTop = y + headingH;
    int bodyH = height - headingH;
    int legendW = std::min(230, width * 2 / 5);
    int chartW = width - legendW;
    int radius = std::max(12, std::min(chartW, bodyH) / 2 - 12);
    int cx = x + chartW / 2;
    int cy = bodyTop + bodyH / 2;

    double total = 0.0;
    for (const auto& point : chart.points)
        total += std::max(0.0, point.value);
    if (total <= 0.0) return;

    SDL_LockSurface(surface);
    auto* pixels = static_cast<Uint32*>(surface->pixels);
    int pitch = surface->pitch / 4;
    Uint32 mappedSeries[6];
    for (size_t i = 0; i < 6; ++i)
        mappedSeries[i] = seriesColor(style, i).toUint32(surface->format);
    for (int dy = -radius; dy <= radius; ++dy) {
        int py = cy + dy;
        if (py < 0 || py >= surface->h) continue;
        for (int dx = -radius; dx <= radius; ++dx) {
            int px = cx + dx;
            if (px < 0 || px >= surface->w ||
                dx * dx + dy * dy > radius * radius) continue;
            double angle = std::atan2(static_cast<double>(dy),
                                      static_cast<double>(dx)) + PI / 2.0;
            if (angle < 0) angle += 2.0 * PI;
            double position = angle / (2.0 * PI) * total;
            double cumulative = 0.0;
            size_t index = 0;
            for (; index + 1 < chart.points.size(); ++index) {
                cumulative += std::max(0.0, chart.points[index].value);
                if (position <= cumulative) break;
            }
            pixels[py * pitch + px] = mappedSeries[index % 6];
        }
    }
    SDL_UnlockSurface(surface);

    if (chart.type == ChartType::Donut) {
        fillCircle(surface, cx, cy, radius * 11 / 20, style.codeBg);
        std::string totalText = formatValue(total);
        drawCentered(renderer, totalText, cx,
                     cy + valueFont.getAscent() / 2.0f,
                     valueFont, style.titleColor);
    }

    int legendX = x + chartW + 8;
    int lineH = std::max(25, static_cast<int>(
        labelFont.getAscent() - labelFont.getDescent()) + 7);
    int legendY = cy - lineH * static_cast<int>(chart.points.size()) / 2;
    for (size_t i = 0; i < chart.points.size(); ++i) {
        int rowY = legendY + static_cast<int>(i) * lineH;
        renderer->fillRect({legendX, rowY + 4, 12, 12},
                           seriesColor(style, i), 3);
        std::string label = chart.points[i].label;
        if (chart.showValues) {
            double percent = std::max(0.0, chart.points[i].value) / total * 100.0;
            label += "  " + formatValue(percent) + "%";
        }
        label = fitLabel(label, labelFont, legendW - 24);
        renderer->drawText(label, static_cast<float>(legendX + 22),
                           rowY + labelFont.getAscent(),
                           labelFont, style.chartLabel.toSDLColor());
    }
}

} // namespace

int chartNaturalHeight(const Chart& chart) {
    return std::max(160, chart.height);
}

int iconBlockNaturalHeight(Renderer* renderer, const FontSet& fonts) {
    int fontH = std::max(renderer->textHeight(fonts.get(FontType::Regular)),
                         renderer->textHeight(fonts.icons()));
    return fontH + 2 * std::max(8, renderer->style().partPadding / 2);
}

uint32_t iconCodepoint(const std::string& name) {
    static const std::unordered_map<std::string, uint32_t> icons = [] {
        std::unordered_map<std::string, uint32_t> result;
        std::ifstream css("assets/FontAwesome-Free.css");
        std::string line;
        std::string currentName;
        while (std::getline(css, line)) {
            if (line.rfind(".fa-", 0) == 0) {
                size_t end = line.find(" {");
                currentName = end == std::string::npos
                    ? std::string()
                    : line.substr(4, end - 4);
                continue;
            }
            if (currentName.empty()) continue;
            size_t property = line.find("--fa:");
            size_t slash = line.find('\\', property);
            size_t quote = line.find('"', slash);
            if (property == std::string::npos || slash == std::string::npos ||
                quote == std::string::npos) continue;
            std::string hex = line.substr(slash + 1, quote - slash - 1);
            char* end = nullptr;
            unsigned long value = std::strtoul(hex.c_str(), &end, 16);
            if (end && *end == '\0' && value > 0)
                result[currentName] = static_cast<uint32_t>(value);
            currentName.clear();
        }

        // Keep the core examples working if the optional name map cannot be
        // opened (for example, in a partially copied development build).
        if (result.empty()) {
            result = {
                {"arrow-trend-up", 0xE098}, {"chart-bar", 0xF080},
                {"chart-line", 0xF201}, {"chart-pie", 0xF200},
                {"circle-check", 0xF058}, {"lightbulb", 0xF0EB},
                {"rocket", 0xF135}, {"trophy", 0xF091},
                {"users", 0xF0C0},
            };
        }
        return result;
    }();
    auto found = icons.find(name);
    return found == icons.end() ? 0 : found->second;
}

void renderChart(Renderer* renderer, SDL_Surface* surface,
                 const Chart& chart, const FontSet& fonts,
                 int x, int y, int width, int height) {
    if (!renderer || !surface || width <= 0 || height <= 0) return;
    const auto& style = renderer->style();
    renderer->fillRect({x, y, width, height}, style.codeBg, style.cornerRadius);
    renderer->drawRectOutline({x, y, width, height},
                              style.codeBorder, style.cornerRadius);
    int pad = std::max(12, style.partPadding / 2);
    int innerX = x + pad;
    int innerY = y + pad;
    int innerW = std::max(0, width - 2 * pad);
    int innerH = std::max(0, height - 2 * pad);
    if (!chart.title.empty())
        renderChartHeading(renderer, surface, chart, fonts,
                           innerX, innerY, innerW);
    if (chart.type == ChartType::Pie || chart.type == ChartType::Donut)
        renderCircularChart(renderer, surface, chart, fonts,
                            innerX, innerY, innerW, innerH);
    else
        renderAxesChart(renderer, surface, chart, fonts,
                        innerX, innerY, innerW, innerH);
}

void renderIconBlock(Renderer* renderer, SDL_Surface* surface,
                     const IconBlock& icon, const FontSet& fonts,
                     int x, int y, int width) {
    if (!renderer || !surface || width <= 0) return;
    const auto& style = renderer->style();
    int height = iconBlockNaturalHeight(renderer, fonts);
    int pad = std::max(8, style.partPadding / 2);
    renderer->fillRect({x, y, width, height}, style.codeBg,
                       style.cornerRadius);
    renderer->drawRectOutline({x, y, width, height}, style.codeBorder,
                              style.cornerRadius);
    uint32_t codepoint = iconCodepoint(icon.name);
    int iconX = x + pad;
    float baseline = y + (height - renderer->textHeight(fonts.icons())) / 2.0f
        + fonts.icons().getAscent();
    if (codepoint && fonts.icons().hasGlyph(codepoint))
        fonts.icons().drawGlyph(surface, codepoint, static_cast<float>(iconX),
                                baseline, style.chartSeries1.toSDLColor());

    int textX = iconX + static_cast<int>(fonts.icons().getFontSize()) + pad;
    FontVariants variants = fonts.variants();
    const Font& textFont = variants.get(FontType::Regular);
    renderer->renderFormattedBlock(
        icon.text, textX,
        y + (height - renderer->textHeight(textFont)) / 2 +
            static_cast<int>(textFont.getAscent()),
        variants, style.textColor.toSDLColor(),
        std::max(0, x + width - pad - textX));
}
