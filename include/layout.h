#pragma once
#include "common.h"
#include <vector>

enum class LayoutKind {
    Title,
    Section,
    HeaderBody,
    HeaderTwoColumn,
    HeaderImage
};

enum class PartRole {
    FullSlide,
    Header,
    Body,
    ColumnLeft,
    ColumnRight,
    Image,
    Caption,
    Footer
};

struct Rect {
    int x, y, w, h;
};

struct SlidePart {
    PartRole role;
    Rect rect;
    int offsetX = 0;
    int offsetY = 0;
    uint8_t alpha = 255;
};

struct LayoutMetrics {
    int slideW;
    int slideH;
    float titleAscent;
    float titleDescent;
    float titleLineH;
    float bodyAscent;
    float bodyDescent;
    float bodyLineH;
};

LayoutKind layoutFromType(SlideType type);
LayoutKind selectLayout(const Slide& slide);
std::vector<SlidePart> computeParts(LayoutKind kind, const Slide& slide, const LayoutMetrics& metrics);