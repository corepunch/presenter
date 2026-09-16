#pragma once

#include "font.h"
#include "style.h"
#include <memory>
#include <string>
#include <vector>

class Renderer;

namespace ui {

constexpr int Unbounded = 1000000;
enum class Alignment { Stretch, Start, Center, End };

struct Size {
    int width = 0;
    int height = 0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Thickness {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    Thickness() = default;
    explicit Thickness(int all) : left(all), top(all), right(all), bottom(all) {}
    Thickness(int horizontal, int vertical)
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
    Thickness(int left, int top, int right, int bottom)
        : left(left), top(top), right(right), bottom(bottom) {}

    int horizontal() const { return left + right; }
    int vertical() const { return top + bottom; }
};

struct LayoutContext {
    Renderer& renderer;
    int overflowCount = 0;
};

class Element {
public:
    virtual ~Element() = default;

    void measure(LayoutContext& context, Size availableSize);
    void arrange(LayoutContext& context, Rect finalRect);
    void render(LayoutContext& context);

    const Size& desiredSize() const { return m_desiredSize; }
    const Size& contentDesiredSize() const { return m_contentDesiredSize; }
    const Size& renderSize() const { return m_renderSize; }
    const Rect& layoutSlot() const { return m_layoutSlot; }
    const Rect& bounds() const { return m_bounds; }

    Thickness margin;
    int width = -1, height = -1;
    int minWidth = 0, minHeight = 0;
    int maxWidth = Unbounded, maxHeight = Unbounded;
    Alignment horizontalAlignment = Alignment::Stretch;
    Alignment verticalAlignment = Alignment::Stretch;
    bool overflow = false;

protected:
    virtual Size measureOverride(LayoutContext& context, Size availableSize);
    virtual Size arrangeOverride(LayoutContext& context, Size finalSize);
    virtual void renderOverride(LayoutContext& context);

private:
    Size m_desiredSize;
    Size m_renderSize;
    Rect m_layoutSlot;
    Rect m_bounds;
    Size m_naturalSize;
    Size m_contentDesiredSize;
};

struct Track {
    enum class Unit { Auto, Pixel, Star };
    Unit unit = Unit::Star;
    double value = 1;
};

class Grid final : public Element {
public:
    std::vector<Track> rows = {{}};
    std::vector<Track> columns = {{}};
    int gap = 0;
    Element& add(std::unique_ptr<Element> child, int row = 0, int column = 0,
                 int rowSpan = 1, int columnSpan = 1);
protected:
    Size measureOverride(LayoutContext&, Size) override;
    Size arrangeOverride(LayoutContext&, Size) override;
    void renderOverride(LayoutContext&) override;
private:
    struct Cell {
        std::unique_ptr<Element> element;
        int row, column, rowSpan, columnSpan;
    };
    std::vector<Cell> m_children;
    std::vector<int> m_rows, m_columns;
};

class Stack final : public Element {
public:
    enum class Orientation {
        Vertical,
        Horizontal,
    };

    explicit Stack(Orientation orientation = Orientation::Vertical)
        : orientation(orientation) {}

    Element& add(std::unique_ptr<Element> child, float grow = 0.0f);
    size_t size() const { return m_children.size(); }
    Element& child(size_t index) { return *m_children[index].element; }
    const Element& child(size_t index) const { return *m_children[index].element; }

    Orientation orientation;
    int gap = 0;

protected:
    Size measureOverride(LayoutContext& context, Size availableSize) override;
    Size arrangeOverride(LayoutContext& context, Size finalSize) override;
    void renderOverride(LayoutContext& context) override;

private:
    struct Child {
        std::unique_ptr<Element> element;
        float grow = 0.0f;
    };
    std::vector<Child> m_children;
};

struct BorderStyle {
    Thickness padding;
    Color background;
    Color borderColor;
    bool hasBackground = false;
    bool hasBorder = false;
    int cornerRadius = 0;
};

class Border final : public Element {
public:
    Border() = default;
    explicit Border(std::unique_ptr<Element> child)
        : m_child(std::move(child)) {}
    Border(std::unique_ptr<Element> child, BorderStyle style)
        : style(std::move(style)), m_child(std::move(child)) {}

    void setChild(std::unique_ptr<Element> child) { m_child = std::move(child); }
    Element* child() { return m_child.get(); }
    const Element* child() const { return m_child.get(); }

    BorderStyle style;

protected:
    Size measureOverride(LayoutContext& context, Size availableSize) override;
    Size arrangeOverride(LayoutContext& context, Size finalSize) override;
    void renderOverride(LayoutContext& context) override;

private:
    std::unique_ptr<Element> m_child;
};

class Text final : public Element {
public:
    Text(std::string text, FontVariants fonts, Color color)
        : text(std::move(text)), fonts(fonts), color(color) {}

    std::string text;
    FontVariants fonts;
    Color color;
    bool wrap = false;
    Alignment textAlignment = Alignment::Start;

protected:
    Size measureOverride(LayoutContext& context, Size availableSize) override;
    void renderOverride(LayoutContext& context) override;
};

} // namespace ui
