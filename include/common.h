#pragma once
#include "style.h"
#include <string>
#include <vector>
#include <cstdint>

enum class FontType {
    Regular,
    Bold,
    Italic,
    BoldItalic,
    Monospace
};

enum class SlideLayout {
    Title,    // Centered title + optional subtitle
    Content,  // Title + vertical stack of text/images (default)
    Image,    // Title + full-width image + caption
    Columns,  // Title + horizontal slots with child slides
    Section,  // Centered title only (divider)
    Blank     // No header, children fill full area
};

enum class ImageFit {
    Fit,   // scale = min(sx, sy), centered (default)
    Fill   // scale = max(sx, sy), center-crop
};

struct Slide {
    SlideLayout layout = SlideLayout::Content;
    std::string slot;       // "left", "right", "0", "1", ... (for Columns parent)
    std::string title;
    std::string subtitle;
    std::vector<std::string> texts;  // bullet points / captions
    std::string notes;
    std::string imagePath;
    std::string imageAlt;
    std::string caption;      // used only by layout="image"
    ImageFit imageFit = ImageFit::Fit;
    int cols = 2;             // column count for layout="columns"
    int gap = 24;             // gap between slots (pixels)

    std::vector<Slide> children;  // recursively nested slides
};

struct Presentation {
    std::vector<Slide> slides;
    int current = 0;
    PresentationStyle style;

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
