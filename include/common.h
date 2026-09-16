#pragma once
#include "style.h"
#include <string>
#include <vector>
#include <cstdint>
#include <map>

enum class FontType {
    Regular,
    Bold,
    Italic,
    BoldItalic,
    Monospace
};


enum class ImageFit {
    Fit,   // scale = min(sx, sy), centered (default)
    Fill   // scale = max(sx, sy), center-crop
};

struct CodeBlock {
    std::string code;
    std::string lang;
};

enum class ChartType {
    Bar,
    Line,
    Pie,
    Donut
};

struct ChartPoint {
    std::string label;
    double value = 0.0;
};

struct Chart {
    ChartType type = ChartType::Bar;
    std::string title;
    std::string icon;
    std::vector<ChartPoint> points;
    int height = 340;
    bool showValues = true;
};

struct IconBlock {
    std::string name;
    std::string text;
};

// Ordered visual tree. Container and leaf properties are interpreted by the
// layout builder; slide metadata (title and notes) stays outside this tree.
struct LayoutNode {
    std::string kind;
    std::string text;
    std::map<std::string, std::string> attributes;
    std::vector<LayoutNode> children;
    Chart chart;
};

struct Slide {
    std::vector<LayoutNode> elements;
    std::string title;
    std::string notes;
};

struct Presentation {
    std::string name;
    std::vector<Slide> slides;
    int current = 0;
    PresentationStyle style = PresentationStyle::defaults();

    bool empty() const { return slides.empty(); }
    int size() const { return static_cast<int>(slides.size()); }
    const Slide& currentSlide() const { return slides[current]; }
    bool canGoNext() const { return current < size() - 1; }
    bool canGoPrev() const { return current > 0; }
    void next() { if (canGoNext()) current++; }
    void prev() { if (canGoPrev()) current--; }
    void first() { current = 0; }
    void last() { current = size() - 1; }
};
