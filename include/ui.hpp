#pragma once

#include "font.h"
#include "style.h"
#include <memory>
#include <string>
#include <vector>

class Renderer;

namespace ui {

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
};

class Element {
public:
    virtual ~Element() = default;

    void measure(LayoutContext& context, Size availableSize);
    void arrange(LayoutContext& context, Rect finalRect);
    void render(LayoutContext& context);

    const Size& desiredSize() const { return m_desiredSize; }
    const Size& renderSize() const { return m_renderSize; }
    const Rect& layoutSlot() const { return m_layoutSlot; }
    const Rect& bounds() const { return m_bounds; }

    Thickness margin;

protected:
    virtual Size measureOverride(LayoutContext& context, Size availableSize);
    virtual Size arrangeOverride(LayoutContext& context, Size finalSize);
    virtual void renderOverride(LayoutContext& context);

private:
    Size m_desiredSize;
    Size m_renderSize;
    Rect m_layoutSlot;
    Rect m_bounds;
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

class Border final : public Element {
public:
    Border() = default;
    explicit Border(std::unique_ptr<Element> child) : m_child(std::move(child)) {}

    void setChild(std::unique_ptr<Element> child) { m_child = std::move(child); }
    Element* child() { return m_child.get(); }
    const Element* child() const { return m_child.get(); }

    Thickness padding;
    Color background;
    Color borderColor;
    bool hasBackground = false;
    bool hasBorder = false;
    int cornerRadius = 0;

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

protected:
    Size measureOverride(LayoutContext& context, Size availableSize) override;
    void renderOverride(LayoutContext& context) override;
};

} // namespace ui
