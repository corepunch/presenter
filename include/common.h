#pragma once
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

enum class SlideType {
    Title,       // Large centered title + subtitle
    Content,     // Title + bullet points
    Image,       // Title + image + optional caption
    Section,     // Section divider (centered, medium-large text)
    TwoColumn    // Title + left/right text columns
};

struct Slide {
    SlideType type = SlideType::Title;
    std::string title;
    std::vector<std::string> bullets;
    std::string imagePath;
    std::string imageAlt;
    std::string leftContent;
    std::string rightContent;
    std::string notes;  // presenter-only
};

struct Presentation {
    std::vector<Slide> slides;
    int current = 0;

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
