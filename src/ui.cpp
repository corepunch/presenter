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
    inner.width = std::min(inner.width, std::clamp(width >= 0 ? width : inner.width,
        minWidth, std::max(minWidth, maxWidth)));
    inner.height = std::min(inner.height, std::clamp(height >= 0 ? height : inner.height,
        minHeight, std::max(minHeight, maxHeight)));
    Size desired = clampSize(measureOverride(context, inner));
    m_contentDesiredSize = desired;
    desired.width = std::clamp(width >= 0 ? width : desired.width, minWidth, std::max(minWidth, maxWidth));
    desired.height = std::clamp(height >= 0 ? height : desired.height, minHeight, std::max(minHeight, maxHeight));
    m_naturalSize = desired;
    overflow = desired.width > inner.width || desired.height > inner.height;
    m_desiredSize = {
        desired.width + margin.horizontal(),
        desired.height + margin.vertical(),
    };
}

void Element::arrange(LayoutContext& context, Rect finalRect) {
    finalRect.width = std::max(0, finalRect.width);
    finalRect.height = std::max(0, finalRect.height);
    m_layoutSlot = finalRect;
    m_bounds = inset(finalRect, margin);
    auto align = [](int& origin, int& extent, int natural, int explicitSize,
                    int minimum, int maximum, Alignment alignment) {
        int size = explicitSize >= 0 ? explicitSize
            : alignment == Alignment::Stretch ? extent : natural;
        size = std::min(extent, std::clamp(size, minimum, std::max(minimum, maximum)));
        if (alignment == Alignment::Center) origin += (extent - size) / 2;
        if (alignment == Alignment::End) origin += extent - size;
        extent = size;
    };
    // Arrangement diagnostics describe the current slot, not an earlier,
    // smaller allocation. Constraints must agree with the measure pass.
    overflow = m_naturalSize.width > m_bounds.width || m_naturalSize.height > m_bounds.height;
    align(m_bounds.x, m_bounds.width, m_naturalSize.width, width, minWidth, maxWidth, horizontalAlignment);
    align(m_bounds.y, m_bounds.height, m_naturalSize.height, height, minHeight, maxHeight, verticalAlignment);
    m_renderSize = clampSize(arrangeOverride(
        context, {m_bounds.width, m_bounds.height}));
    m_renderSize.width = std::min(m_renderSize.width, m_bounds.width);
    m_renderSize.height = std::min(m_renderSize.height, m_bounds.height);
    m_bounds.width = m_renderSize.width;
    m_bounds.height = m_renderSize.height;
}

void Element::render(LayoutContext& context) {
    if (overflow) ++context.overflowCount;
    if (overflow) fprintf(stderr, "[layout] overflow at (%d,%d): desired %dx%d, arranged %dx%d\n",
        m_bounds.x, m_bounds.y, m_naturalSize.width, m_naturalSize.height, m_bounds.width, m_bounds.height);
    if (m_bounds.width <= 0 || m_bounds.height <= 0) return;
    SDL_Surface* surface = context.renderer.surface();
    SDL_Rect previous{}, clip, intersection{};
    if (surface) {
        // Bounds are logical; the surface is device pixels (logical*ratio).
        clip = context.renderer.toDeviceRect(m_bounds.x, m_bounds.y, m_bounds.width, m_bounds.height);
        SDL_GetClipRect(surface, &previous);
        SDL_IntersectRect(&previous, &clip, &intersection);
        SDL_SetClipRect(surface, &intersection);
    }
    renderOverride(context);
    if (surface) SDL_SetClipRect(surface, &previous);
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
        Size constraint = availableSize;
        if (orientation == Orientation::Vertical) constraint.height = Unbounded;
        else constraint.width = Unbounded;
        child.element->measure(context, constraint);
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
    std::vector<std::string> lines = context.renderer.wordWrap(text, fonts,
        wrap ? std::max(1, availableSize.width) : Unbounded);
    int width = 0;
    for (const auto& line : lines) {
        width = std::max(width, static_cast<int>(
            std::ceil(context.renderer.formattedWidth(line, fonts))));
    }
    return {
        width,
        lineHeight * static_cast<int>(lines.size()),
    };
}

void Text::renderOverride(LayoutContext& context) {
    const Font& regular = fonts.get(FontType::Regular);
    int lineHeight = context.renderer.textHeight(regular);
    int baselineOffset = static_cast<int>(std::ceil(regular.getAscent()));
    std::vector<std::string> lines = context.renderer.wordWrap(text, fonts,
        wrap ? std::max(1, bounds().width) : Unbounded);
    int visibleLines = lineHeight > 0 ? bounds().height / lineHeight : 0;
    visibleLines = std::min(visibleLines, static_cast<int>(lines.size()));
    for (int i = 0; i < visibleLines; ++i) {
        int x = bounds().x;
        int extra = bounds().width - static_cast<int>(std::ceil(context.renderer.formattedWidth(lines[i], fonts)));
        if (textAlignment == Alignment::Center) x += extra / 2;
        if (textAlignment == Alignment::End) x += extra;
        context.renderer.renderFormatted(
            lines[i],
            static_cast<float>(x),
            static_cast<float>(bounds().y + baselineOffset + i * lineHeight),
            fonts, color.toSDLColor());
    }
}

Element& Grid::add(std::unique_ptr<Element> child, int row, int column,
                   int rowSpan, int columnSpan) {
    row = std::clamp(row, 0, static_cast<int>(rows.size()) - 1);
    column = std::clamp(column, 0, static_cast<int>(columns.size()) - 1);
    m_children.push_back({std::move(child), row, column,
        std::clamp(rowSpan, 1, static_cast<int>(rows.size()) - row),
        std::clamp(columnSpan, 1, static_cast<int>(columns.size()) - column)});
    return *m_children.back().element;
}

static int total(const std::vector<int>& sizes, int start, int count, int gap) {
    int result = std::max(0, count - 1) * gap;
    for (int i = start; i < start + count; ++i) result += sizes[i];
    return result;
}

static std::vector<int> allocate(const std::vector<Track>& tracks,
                                  const std::vector<int>& natural, int available, int gap) {
    std::vector<int> result(tracks.size());
    int used = std::max(0, static_cast<int>(tracks.size()) - 1) * gap;
    double stars = 0;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].unit == Track::Unit::Star && available < Unbounded) stars += tracks[i].value;
        else {
            result[i] = tracks[i].unit == Track::Unit::Pixel ? static_cast<int>(tracks[i].value) : natural[i];
            used += result[i];
        }
    }
    double seen = 0;
    int assigned = 0;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].unit != Track::Unit::Star || available >= Unbounded || stars <= 0) continue;
        seen += tracks[i].value;
        int target = static_cast<int>(std::lround(std::max(0, available - used) * seen / stars));
        result[i] = target - assigned;
        assigned = target;
    }
    return result;
}

static void contribute(std::vector<int>& natural, const std::vector<Track>& tracks,
                       int start, int span, int desired, int gap) {
    int missing = desired - total(natural, start, span, gap);
    int flexible = 0;
    for (int i = start; i < start + span; ++i)
        if (tracks[i].unit != Track::Unit::Pixel) ++flexible;
    for (int i = start; missing > 0 && i < start + span; ++i) {
        if (tracks[i].unit == Track::Unit::Pixel) continue;
        int amount = (missing + flexible - 1) / flexible;
        natural[i] += amount;
        missing -= amount;
        --flexible;
    }
}

Size Grid::measureOverride(LayoutContext& context, Size available) {
    m_columns.assign(columns.size(), 0);
    m_rows.assign(rows.size(), 0);
    for (size_t i = 0; i < columns.size(); ++i)
        if (columns[i].unit == Track::Unit::Pixel) m_columns[i] = static_cast<int>(columns[i].value);
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].unit == Track::Unit::Pixel) m_rows[i] = static_cast<int>(rows[i].value);
    // Discover intrinsic widths, then measure heights using allocated widths.
    // This is essential: wrapping must happen after star columns are resolved.
    std::vector<Cell*> byColumnSpan, byRowSpan;
    for (auto& cell : m_children) {
        cell.element->measure(context, {Unbounded, Unbounded});
        byColumnSpan.push_back(&cell);
        byRowSpan.push_back(&cell);
    }
    // Establish each single-track minimum before distributing multi-track
    // requirements. Geometry must not depend on XML paint order.
    std::sort(byColumnSpan.begin(), byColumnSpan.end(), [](const Cell* a, const Cell* b) {
        if (a->columnSpan != b->columnSpan) return a->columnSpan < b->columnSpan;
        if (a->column != b->column) return a->column < b->column;
        return a->element->desiredSize().width < b->element->desiredSize().width;
    });
    for (auto* cell : byColumnSpan)
        contribute(m_columns, columns, cell->column, cell->columnSpan,
                   cell->element->desiredSize().width, gap);
    auto widths = allocate(columns, m_columns, available.width, gap);
    for (auto& cell : m_children) {
        cell.element->measure(context, {total(widths, cell.column, cell.columnSpan, gap), Unbounded});
    }
    std::sort(byRowSpan.begin(), byRowSpan.end(), [](const Cell* a, const Cell* b) {
        if (a->rowSpan != b->rowSpan) return a->rowSpan < b->rowSpan;
        if (a->row != b->row) return a->row < b->row;
        return a->element->desiredSize().height < b->element->desiredSize().height;
    });
    for (auto* cell : byRowSpan)
        contribute(m_rows, rows, cell->row, cell->rowSpan, cell->element->desiredSize().height, gap);
    auto heights = allocate(rows, m_rows, available.height, gap);
    for (auto& cell : m_children)
        cell.element->measure(context, {total(widths, cell.column, cell.columnSpan, gap),
                                       total(heights, cell.row, cell.rowSpan, gap)});
    return {total(widths, 0, static_cast<int>(widths.size()), gap),
            total(heights, 0, static_cast<int>(heights.size()), gap)};
}

Size Grid::arrangeOverride(LayoutContext& context, Size finalSize) {
    // Re-measure if a parent gives us a different width or height.
    measureOverride(context, finalSize);
    auto widths = allocate(columns, m_columns, finalSize.width, gap);
    auto heights = allocate(rows, m_rows, finalSize.height, gap);
    for (auto& cell : m_children) {
        int x = bounds().x, y = bounds().y;
        for (int i = 0; i < cell.column; ++i) x += widths[i] + gap;
        for (int i = 0; i < cell.row; ++i) y += heights[i] + gap;
        cell.element->arrange(context, {x, y, total(widths, cell.column, cell.columnSpan, gap),
                                      total(heights, cell.row, cell.rowSpan, gap)});
    }
    return finalSize;
}

void Grid::renderOverride(LayoutContext& context) {
    for (auto& cell : m_children) cell.element->render(context);
}

} // namespace ui
