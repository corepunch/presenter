#pragma once
#include "ui.hpp"
#include "common.h"

std::unique_ptr<ui::Element> buildSlideLayout(const LayoutNode& node,
    const FontSet& fonts, const PresentationStyle& style);

// Leaf drawing remains in renderer.cpp; containers use the shared UI engine.
std::unique_ptr<ui::Element> createVisualLeaf(const LayoutNode& node, const FontSet& fonts);
