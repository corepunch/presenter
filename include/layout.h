#pragma once
#include "common.h"
#include <vector>

enum class LayoutKind {
    Title,
    Section,
    HeaderBody,
    Columns,
    HeaderImage
};

enum class PartRole {
    FullSlide,
    Header,
    Body,
    Slot,
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
    int childIndex = 0;
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

LayoutKind layoutFromSlide(const Slide& slide);
std::vector<SlidePart> computeParts(LayoutKind kind, const Slide& slide,
                                    const LayoutMetrics& metrics,
                                    const PresentationStyle& style);

