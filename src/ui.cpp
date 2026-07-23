#include "ui.hpp"
#include "renderer.h"
#include <algorithm>
#include <cmath>

namespace ui {

static Size clampSize(Size size) {
    return {std::max(0, size.width), std::max(0, size.height)};
}

static Rect inset(Rect rect, const Thickness& thickness) {
    return {
        rect.x + thickness.left,
        rect.y + thickness.top,
        std::max(0, rect.width - thickness.horizontal()),
        std::max(0, rect.height - thickness.vertical()),
    };
}

void Element::measure(LayoutContext& context, Size availableSize) {
    availableSize = clampSize(availableSize);
    Size inner = {
        std::max(0, availableSize.width - margin.horizontal()),
        std::max(0, availableSize.height - margin.vertical()),
    };
    Size desired = clampSize(measureOverride(context, inner));
    m_desiredSize = {
        std::min(availableSize.width, desired.width + margin.horizontal()),
        std::min(availableSize.height, desired.height + margin.vertical()),
    };
}

void Element::arrange(LayoutContext& context, Rect finalRect) {
    finalRect.width = std::max(0, finalRect.width);
    finalRect.height = std::max(0, finalRect.height);
    m_layoutSlot = finalRect;
    m_bounds = inset(finalRect, margin);
    m_renderSize = clampSize(arrangeOverride(
        context, {m_bounds.width, m_bounds.height}));
    m_renderSize.width = std::min(m_renderSize.width, m_bounds.width);
    m_renderSize.height = std::min(m_renderSize.height, m_bounds.height);
    m_bounds.width = m_renderSize.width;
    m_bounds.height = m_renderSize.height;
}

void Element::render(LayoutContext& context) {
    if (m_bounds.width <= 0 || m_bounds.height <= 0) return;
    renderOverride(context);
}

Size Element::measureOverride(LayoutContext&, Size) {
    return {};
}

Size Element::arrangeOverride(LayoutContext&, Size finalSize) {
    return finalSize;
}

void Element::renderOverride(LayoutContext&) {}

Element& Stack::add(std::unique_ptr<Element> child, float grow) {
    m_children.push_back({std::move(child), std::max(0.0f, grow)});
    return *m_children.back().element;
}

Size Stack::measureOverride(LayoutContext& context, Size availableSize) {
    int main = 0;
    int cross = 0;
    for (auto& child : m_children) {
        child.element->measure(context, availableSize);
        const Size& desired = child.element->desiredSize();
        if (orientation == Orientation::Vertical) {
            main += desired.height;
            cross = std::max(cross, desired.width);
        } else {
            main += desired.width;
            cross = std::max(cross, desired.height);
        }
    }
    if (!m_children.empty()) {
        main += gap * static_cast<int>(m_children.size() - 1);
    }
    return orientation == Orientation::Vertical
        ? Size{cross, main}
        : Size{main, cross};
}

Size Stack::arrangeOverride(LayoutContext& context, Size finalSize) {
    int availableMain = orientation == Orientation::Vertical
        ? finalSize.height : finalSize.width;
    int gaps = m_children.empty()
        ? 0 : gap * static_cast<int>(m_children.size() - 1);
    int fixedMain = 0;
    float totalGrow = 0.0f;

    for (const auto& child : m_children) {
        const Size& desired = child.element->desiredSize();
        if (child.grow > 0.0f) {
            totalGrow += child.grow;
        } else {
            fixedMain += orientation == Orientation::Vertical
                ? desired.height : desired.width;
        }
    }

    int growSpace = std::max(0, availableMain - gaps - fixedMain);
    int cursor = orientation == Orientation::Vertical ? bounds().y : bounds().x;
    int growUsed = 0;
    float growSeen = 0.0f;

    for (size_t i = 0; i < m_children.size(); ++i) {
        auto& child = m_children[i];
        const Size& desired = child.element->desiredSize();
        int mainSize;
        if (child.grow > 0.0f && totalGrow > 0.0f) {
            growSeen += child.grow;
            int targetUsed = static_cast<int>(std::lround(
                growSpace * (growSeen / totalGrow)));
            mainSize = targetUsed - growUsed;
            growUsed = targetUsed;
        } else {
            mainSize = orientation == Orientation::Vertical
                ? desired.height : desired.width;
        }

        Rect slot;
        if (orientation == Orientation::Vertical) {
            slot = {bounds().x, cursor, finalSize.width, mainSize};
        } else {
            slot = {cursor, bounds().y, mainSize, finalSize.height};
        }
        child.element->arrange(context, slot);
        cursor += mainSize + gap;
    }
    return finalSize;
}

void Stack::renderOverride(LayoutContext& context) {
    for (auto& child : m_children) {
        child.element->render(context);
    }
}

Size Border::measureOverride(LayoutContext& context, Size availableSize) {
    if (!m_child) {
        return {style.padding.horizontal(), style.padding.vertical()};
    }
    Size inner = {
        std::max(0, availableSize.width - style.padding.horizontal()),
        std::max(0, availableSize.height - style.padding.vertical()),
    };
    m_child->measure(context, inner);
    return {
        m_child->desiredSize().width + style.padding.horizontal(),
        m_child->desiredSize().height + style.padding.vertical(),
    };
}

Size Border::arrangeOverride(LayoutContext& context, Size finalSize) {
    if (m_child) {
        Rect content = inset(bounds(), style.padding);
        m_child->arrange(context, content);
    }
    return finalSize;
}

void Border::renderOverride(LayoutContext& context) {
    SDL_Rect rect = {bounds().x, bounds().y, bounds().width, bounds().height};
    if (style.hasBackground) {
        context.renderer.fillRect(
            rect, style.background, style.cornerRadius);
    }
    if (style.hasBorder) {
        context.renderer.drawRectOutline(
            rect, style.borderColor, style.cornerRadius);
    }
    if (m_child) m_child->render(context);
}

Size Text::measureOverride(LayoutContext& context, Size availableSize) {
    const Font& regular = fonts.get(FontType::Regular);
    int lineHeight = context.renderer.textHeight(regular);
    std::vector<std::string> lines = wrap
        ? context.renderer.wordWrap(text, fonts, availableSize.width)
        : std::vector<std::string>{text};
    int width = 0;
    for (const auto& line : lines) {
        width = std::max(width, static_cast<int>(
            std::ceil(regular.measureString(line))));
    }
    return {
        std::min(availableSize.width, width),
        std::min(availableSize.height,
                 lineHeight * static_cast<int>(lines.size())),
    };
}

void Text::renderOverride(LayoutContext& context) {
    const Font& regular = fonts.get(FontType::Regular);
    int lineHeight = context.renderer.textHeight(regular);
    int baselineOffset = static_cast<int>(std::ceil(regular.getAscent()));
    std::vector<std::string> lines = wrap
        ? context.renderer.wordWrap(text, fonts, bounds().width)
        : std::vector<std::string>{text};
    int visibleLines = lineHeight > 0 ? bounds().height / lineHeight : 0;
    visibleLines = std::min(visibleLines, static_cast<int>(lines.size()));
    for (int i = 0; i < visibleLines; ++i) {
        context.renderer.renderFormatted(
            lines[i],
            static_cast<float>(bounds().x),
            static_cast<float>(bounds().y + baselineOffset + i * lineHeight),
            fonts, color.toSDLColor());
    }
}

} // namespace ui
