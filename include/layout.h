#pragma once
#include "ui.hpp"
#include "common.h"

using LayoutElements = std::map<const LayoutNode*, ui::Element*>;
std::unique_ptr<ui::Element> buildSlideLayout(const LayoutNode& node,
    const FontSet& fonts, const PresentationStyle& style, LayoutElements* elements = nullptr);

// Leaf drawing remains in renderer.cpp; containers use the shared UI engine.
std::unique_ptr<ui::Element> createVisualLeaf(const LayoutNode& node, const FontSet& fonts);
