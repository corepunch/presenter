// Reference scenarios: orca/tests/test_{layout,stack_layout,grid_layout,text_layout}.lua.
// Presenter deliberately uses explicit grid cells and content-sized auto tracks.
// Most cases use synthetic leaves: expected geometry is independent of font,
// renderer, and platform. Checks remain enabled in builds defining NDEBUG.
#include "ui.hpp"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0, cases = 0;
const char* current = "";
void equal(int actual, int expected, const char* expression, int line) {
    ++checks;
    if (actual == expected) return;
    ++failures;
    fprintf(stderr, "FAIL %s:%d: %s: expected %d, got %d\n",
            current, line, expression, expected, actual);
}
#define EQ(actual, expected) equal((actual), (expected), #actual, __LINE__)
#define YES(value) EQ(bool(value), true)
#define NO(value) EQ(bool(value), false)
void size(ui::Size actual, int w, int h, int line) {
    equal(actual.width, w, "width", line); equal(actual.height, h, "height", line);
}
void rect(ui::Rect actual, int x, int y, int w, int h, int line) {
    equal(actual.x, x, "x", line); equal(actual.y, y, "y", line);
    equal(actual.width, w, "width", line); equal(actual.height, h, "height", line);
}
#define SIZE(value, w, h) size((value), (w), (h), __LINE__)
#define RECT(value, x, y, w, h) rect((value), (x), (y), (w), (h), __LINE__)
template<class F> void test(const char* name, F body) {
    current = name; ++cases;
    int before = failures;
    body();
    printf("%s %s\n", before == failures ? "PASS" : "FAIL", name);
}

class Probe : public ui::Element {
public:
    explicit Probe(int w = 80, int h = 30) : natural{w, h} {}
    ui::Size natural, measured{}, arranged{}, overrideResult{-1, -1};
    std::vector<ui::Size> measurements;
    int arranges = 0, renders = 0;
    bool wrapping = false;
protected:
    ui::Size measureOverride(ui::LayoutContext&, ui::Size available) override {
        measured = available; measurements.push_back(available);
        if (!wrapping) return natural;
        int width = std::max(1, std::min(natural.width, available.width));
        return {width, ((natural.width + width - 1) / width) * natural.height};
    }
    ui::Size arrangeOverride(ui::LayoutContext&, ui::Size available) override {
        ++arranges; arranged = available;
        return overrideResult.width == -1 ? available : overrideResult;
    }
    void renderOverride(ui::LayoutContext&) override { ++renders; }
};

Probe& add(ui::Stack& stack, int w, int h, float grow = 0) {
    auto p = std::make_unique<Probe>(w, h);
    Probe& result = *p; stack.add(std::move(p), grow); return result;
}
Probe& add(ui::Grid& grid, int w, int h, int row = 0, int column = 0,
           int rowSpan = 1, int columnSpan = 1) {
    auto p = std::make_unique<Probe>(w, h);
    Probe& result = *p; grid.add(std::move(p), row, column, rowSpan, columnSpan); return result;
}
ui::Track px(int n) { return {ui::Track::Unit::Pixel, double(n)}; }
ui::Track star(double n = 1) { return {ui::Track::Unit::Star, n}; }
ui::Track automatic() { return {ui::Track::Unit::Auto, 0}; }
void layout(ui::Element& node, ui::LayoutContext& c, int w, int h, int x = 0, int y = 0) {
    node.measure(c, {w, h}); node.arrange(c, {x, y, w, h});
}

void elements(ui::LayoutContext& c) {
    test("element / initial state and override lifecycle", [&] {
        Probe p; SIZE(p.desiredSize(), 0, 0); SIZE(p.renderSize(), 0, 0);
        RECT(p.bounds(), 0, 0, 0, 0);
        p.measure(c, {200, 100});
        SIZE(p.measured, 200, 100); SIZE(p.desiredSize(), 80, 30);
        EQ(p.arranges, 0); EQ(p.renders, 0);
        p.arrange(c, {13, 17, 200, 100});
        SIZE(p.arranged, 200, 100); RECT(p.bounds(), 13, 17, 200, 100);
        EQ(p.measurements.size(), 1); EQ(p.arranges, 1); EQ(p.renders, 0);
    });
    test("element / asymmetric margin belongs to desired size and slot", [&] {
        Probe p; p.margin = {3, 5, 7, 11}; layout(p, c, 200, 100, 10, 20);
        SIZE(p.measured, 190, 84); SIZE(p.desiredSize(), 90, 46);
        RECT(p.layoutSlot(), 10, 20, 200, 100); RECT(p.bounds(), 13, 25, 190, 84);
    });
    test("element / all 16 alignment combinations", [&] {
        const ui::Alignment a[] = {ui::Alignment::Stretch, ui::Alignment::Start,
                                  ui::Alignment::Center, ui::Alignment::End};
        const int xs[] = {13, 13, 68, 123}, ys[] = {25, 25, 52, 79};
        for (int h = 0; h < 4; ++h) for (int v = 0; v < 4; ++v) {
            Probe p; p.margin = {3, 5, 7, 11};
            p.horizontalAlignment = a[h]; p.verticalAlignment = a[v];
            layout(p, c, 200, 100, 10, 20);
            RECT(p.bounds(), xs[h], ys[v], h == 0 ? 190 : 80, v == 0 ? 84 : 30);
            NO(p.overflow);
        }
    });
    test("element / explicit size disables stretch", [&] {
        Probe p; p.width = 50; p.height = 20; p.margin = ui::Thickness(4);
        layout(p, c, 200, 100);
        SIZE(p.measured, 50, 20); SIZE(p.desiredSize(), 58, 28);
        RECT(p.bounds(), 4, 4, 50, 20);
    });
    test("element / min and max clamp both explicit and intrinsic sizes", [&] {
        struct Case { int request, minimum, maximum, expected; };
        for (auto item : {Case{-1, 0, 500, 80}, Case{-1, 100, 500, 100},
                          Case{-1, 0, 40, 40}, Case{20, 50, 100, 50},
                          Case{200, 50, 100, 100}, Case{75, 50, 100, 75}}) {
            Probe p(80, 80); p.width = p.height = item.request;
            p.minWidth = p.minHeight = item.minimum; p.maxWidth = p.maxHeight = item.maximum;
            p.horizontalAlignment = p.verticalAlignment = ui::Alignment::Start;
            layout(p, c, 500, 500);
            SIZE(p.desiredSize(), item.expected, item.expected);
            SIZE(p.renderSize(), item.expected, item.expected);
        }
    });
    test("element / max constraint limits stretch and centers resulting box", [&] {
        Probe p; p.maxWidth = 100; p.maxHeight = 40;
        p.horizontalAlignment = p.verticalAlignment = ui::Alignment::Center;
        p.width = 180; p.height = 90; layout(p, c, 300, 200);
        RECT(p.bounds(), 100, 80, 100, 40);
    });
    test("element / zero, negative and margin-exhausted available sizes", [&] {
        for (auto available : {ui::Size{0, 0}, ui::Size{-10, -20}, ui::Size{5, 5}}) {
            Probe p; p.margin = ui::Thickness(10);
            layout(p, c, available.width, available.height);
            SIZE(p.measured, 0, 0); SIZE(p.renderSize(), 0, 0);
            YES(p.overflow); SIZE(p.desiredSize(), 100, 50);
        }
    });
    test("element / negative override return is sanitized", [&] {
        Probe p(-20, -10); p.overrideResult = {-2, -3};
        layout(p, c, 100, 100);
        SIZE(p.desiredSize(), 0, 0); SIZE(p.renderSize(), 0, 0);
    });
    test("element / arrange override cannot escape allocated size", [&] {
        Probe p; p.overrideResult = {1000, 1000};
        layout(p, c, 100, 50); SIZE(p.renderSize(), 100, 50);
    });
    test("element / smaller arrange override is retained", [&] {
        Probe p; p.overrideResult = {20, 10};
        layout(p, c, 100, 50, 5, 7); RECT(p.bounds(), 5, 7, 20, 10);
        RECT(p.layoutSlot(), 5, 7, 100, 50);
    });
    test("element / overflow clears after arranging in a larger slot", [&] {
        Probe p; p.measure(c, {200, 100});
        p.arrange(c, {0, 0, 10, 10}); YES(p.overflow);
        p.arrange(c, {0, 0, 200, 100}); NO(p.overflow);
    });
    test("element / repeat measurement uses changed properties and content", [&] {
        Probe p; layout(p, c, 10, 10); YES(p.overflow);
        p.natural = {5, 4}; p.margin = {1, 2, 3, 4};
        layout(p, c, 100, 100);
        SIZE(p.desiredSize(), 9, 10); NO(p.overflow);
    });
    test("element / unbounded constraints stay nonnegative", [&] {
        Probe p; p.margin = {5, 10, 15, 20}; p.measure(c, {ui::Unbounded, ui::Unbounded});
        SIZE(p.measured, ui::Unbounded - 20, ui::Unbounded - 30);
        SIZE(p.desiredSize(), 100, 60);
    });
}

void stacks(ui::LayoutContext& c) {
    test("stack / empty and single child have no phantom gaps", [&] {
        ui::Stack s; s.gap = 99; layout(s, c, 200, 100); SIZE(s.desiredSize(), 0, 0);
        auto& p = add(s, 40, 20); layout(s, c, 200, 100);
        SIZE(s.desiredSize(), 40, 20); RECT(p.bounds(), 0, 0, 200, 20);
    });
    test("stack / vertical margins spacing and translated coordinates", [&] {
        ui::Stack s; s.margin = ui::Thickness(10); s.gap = 5;
        auto& a = add(s, 30, 50); auto& b = add(s, 60, 50); b.margin = ui::Thickness(5);
        layout(s, c, 300, 200, 20, 30);
        SIZE(s.desiredSize(), 90, 135); SIZE(a.measured, 280, ui::Unbounded);
        RECT(a.bounds(), 30, 40, 280, 50); RECT(b.bounds(), 35, 100, 270, 50);
    });
    test("stack / horizontal cross axis stretch", [&] {
        ui::Stack s(ui::Stack::Orientation::Horizontal); s.gap = 5;
        auto& a = add(s, 60, 20); auto& b = add(s, 60, 30); b.margin = ui::Thickness(5);
        layout(s, c, 300, 100, 10, 20);
        SIZE(s.desiredSize(), 135, 40); SIZE(a.measured, ui::Unbounded, 100);
        RECT(a.bounds(), 10, 20, 60, 100); RECT(b.bounds(), 80, 25, 60, 90);
    });
    test("stack / implicit cross-axis center start end", [&] {
        for (auto align : {ui::Alignment::Start, ui::Alignment::Center, ui::Alignment::End}) {
            ui::Stack s(ui::Stack::Orientation::Horizontal);
            add(s, 40, 64); auto& p = add(s, 40, 16); p.verticalAlignment = align;
            layout(s, c, 200, 64);
            EQ(p.bounds().y, align == ui::Alignment::Start ? 0 : align == ui::Alignment::Center ? 24 : 48);
            EQ(p.bounds().height, 16);
        }
    });
    test("stack / nested same-axis stacks size to content", [&] {
        ui::Stack outer; outer.gap = 3;
        auto inner = std::make_unique<ui::Stack>(); auto* ip = inner.get(); inner->gap = 7;
        add(*inner, 40, 20); add(*inner, 80, 30);
        outer.add(std::move(inner)); auto& tail = add(outer, 10, 5);
        layout(outer, c, 300, 200);
        SIZE(ip->desiredSize(), 80, 57); EQ(tail.bounds().y, 60); EQ(outer.desiredSize().height, 65);
    });
    test("stack / oversized fixed content is not shrunk", [&] {
        ui::Stack s; s.gap = 10;
        auto& a = add(s, 20, 50); auto& b = add(s, 20, 60);
        layout(s, c, 100, 80); YES(s.overflow);
        EQ(a.bounds().height, 50); EQ(b.bounds().height, 60); EQ(b.bounds().y, 60);
    });
    test("stack / grow divides remainder after fixed children and gaps", [&] {
        for (auto orientation : {ui::Stack::Orientation::Vertical, ui::Stack::Orientation::Horizontal}) {
            ui::Stack s(orientation); s.gap = 10;
            add(s, 20, 20); auto& a = add(s, 0, 0, 1); auto& b = add(s, 0, 0, 2);
            layout(s, c, 340, 340);
            EQ(orientation == ui::Stack::Orientation::Vertical ? a.layoutSlot().height : a.layoutSlot().width, 100);
            EQ(orientation == ui::Stack::Orientation::Vertical ? b.layoutSlot().height : b.layoutSlot().width, 200);
        }
    });
    test("stack / insufficient grow space cannot create negative sizes", [&] {
        ui::Stack s; s.gap = 10; add(s, 10, 100); auto& p = add(s, 0, 0, 1);
        layout(s, c, 100, 50); EQ(p.layoutSlot().height, 0); EQ(p.bounds().height, 0);
    });
    test("stack / grow rounding conserves every pixel", [&] {
        for (int extent = 0; extent < 101; ++extent) {
            ui::Stack s; auto& a = add(s, 0, 0, 1); auto& b = add(s, 0, 0, 2); auto& d = add(s, 0, 0, 3);
            layout(s, c, 100, extent);
            EQ(a.layoutSlot().height + b.layoutSlot().height + d.layoutSlot().height, extent);
            EQ(d.layoutSlot().y + d.layoutSlot().height, extent);
        }
    });
    test("stack / orientation and gap changes take effect on next layout", [&] {
        ui::Stack s; add(s, 20, 10); auto& b = add(s, 30, 15);
        layout(s, c, 200, 100); EQ(b.bounds().y, 10);
        s.orientation = ui::Stack::Orientation::Horizontal; s.gap = 8;
        layout(s, c, 200, 100); RECT(b.bounds(), 28, 0, 30, 100); SIZE(s.desiredSize(), 58, 15);
    });
}

void borders(ui::LayoutContext& c) {
    test("border / empty border measures padding", [&] {
        ui::Border b; b.style.padding = {3, 5, 7, 11}; layout(b, c, 100, 100);
        SIZE(b.desiredSize(), 10, 16);
    });
    test("border / padding margin and child margin compose once", [&] {
        auto p = std::make_unique<Probe>(40, 20); auto* leaf = p.get(); p->margin = {1, 2, 3, 4};
        ui::Border b(std::move(p)); b.margin = {5, 6, 7, 8}; b.style.padding = {10, 11, 12, 13};
        layout(b, c, 200, 120, 10, 20);
        SIZE(b.desiredSize(), 78, 64); SIZE(leaf->measured, 162, 76);
        RECT(leaf->bounds(), 26, 39, 162, 76);
    });
    test("border / nested borders accumulate offsets", [&] {
        auto p = std::make_unique<Probe>(20, 10); auto* leaf = p.get();
        auto inner = std::make_unique<ui::Border>(std::move(p)); inner->style.padding = ui::Thickness(7);
        ui::Border outer(std::move(inner)); outer.style.padding = ui::Thickness(5);
        layout(outer, c, 100, 80, 3, 4);
        SIZE(outer.desiredSize(), 44, 34); RECT(leaf->bounds(), 15, 16, 76, 56);
    });
    test("border / exhausted padding gives child zero constraint", [&] {
        auto p = std::make_unique<Probe>(); auto* leaf = p.get();
        ui::Border b(std::move(p)); b.style.padding = ui::Thickness(30);
        layout(b, c, 20, 10); SIZE(leaf->measured, 0, 0); SIZE(leaf->renderSize(), 0, 0);
        YES(b.overflow); YES(leaf->overflow);
    });
    test("border / child replacement invalidates intrinsic dimensions on remeasure", [&] {
        ui::Border b(std::make_unique<Probe>(20, 30)); b.style.padding = ui::Thickness(4);
        layout(b, c, 200, 200); SIZE(b.desiredSize(), 28, 38);
        b.setChild(std::make_unique<Probe>(60, 10)); layout(b, c, 200, 200); SIZE(b.desiredSize(), 68, 18);
        b.setChild(nullptr); layout(b, c, 200, 200); SIZE(b.desiredSize(), 8, 8);
    });
}

void grids(ui::LayoutContext& c) {
    test("grid / empty default grid fills finite constraints", [&] {
        ui::Grid g; layout(g, c, 300, 200); SIZE(g.desiredSize(), 300, 200);
        g.measure(c, {ui::Unbounded, ui::Unbounded}); SIZE(g.desiredSize(), 0, 0);
    });
    test("grid / fixed header star content fixed footer with margins", [&] {
        ui::Grid g; g.rows = {px(64), star(), px(48)};
        auto& a = add(g, 0, 0, 0); auto& b = add(g, 0, 0, 1); auto& d = add(g, 0, 0, 2);
        a.margin = b.margin = d.margin = ui::Thickness(8);
        layout(g, c, 300, 400, 10, 20);
        RECT(a.bounds(), 18, 28, 284, 48); RECT(b.bounds(), 18, 92, 284, 272);
        RECT(d.bounds(), 18, 380, 284, 32);
    });
    test("grid / mixed pixel auto weighted star columns", [&] {
        ui::Grid g; g.columns = {px(100), automatic(), star(), star(2)}; g.gap = 10;
        auto& a = add(g, 0, 0, 0, 0); auto& b = add(g, 80, 10, 0, 1);
        auto& d = add(g, 0, 0, 0, 2); auto& e = add(g, 0, 0, 0, 3);
        layout(g, c, 510, 100, 20, 30);
        RECT(a.bounds(), 20, 30, 100, 100); RECT(b.bounds(), 130, 30, 80, 100);
        RECT(d.bounds(), 220, 30, 100, 100); RECT(e.bounds(), 330, 30, 200, 100);
    });
    test("grid / unequal auto rows retain individual heights", [&] {
        ui::Grid g; g.columns = {star(), star()}; g.rows = {automatic(), automatic()}; g.gap = 5;
        auto& a = add(g, 20, 20); add(g, 20, 10, 0, 1);
        auto& b = add(g, 20, 60, 1); add(g, 20, 40, 1, 1);
        layout(g, c, 205, 300);
        SIZE(g.desiredSize(), 205, 85);
        RECT(a.bounds(), 0, 0, 100, 20); RECT(b.bounds(), 0, 25, 100, 60);
    });
    test("grid / auto columns use content maxima including margins", [&] {
        ui::Grid g; g.columns = {automatic(), automatic()}; g.rows = {automatic(), automatic()};
        auto& a = add(g, 20, 10); a.margin = ui::Thickness(5);
        auto& b = add(g, 70, 20, 1); auto& d = add(g, 30, 10, 0, 1);
        layout(g, c, 500, 500);
        EQ(a.layoutSlot().width, 70); EQ(b.layoutSlot().width, 70);
        EQ(d.bounds().x, 70); EQ(d.bounds().width, 30); SIZE(g.desiredSize(), 100, 40);
    });
    test("grid / nested in vertical stack derives height from children", [&] {
        ui::Stack outer; outer.gap = 10;
        auto g = std::make_unique<ui::Grid>(); auto* gp = g.get(); g->columns = {star(), star()};
        auto inner = std::make_unique<ui::Stack>(); inner->gap = 5;
        add(*inner, 20, 30); add(*inner, 20, 40); g->add(std::move(inner));
        outer.add(std::move(g)); auto& tail = add(outer, 10, 15);
        layout(outer, c, 400, 300);
        EQ(gp->bounds().height, 75); EQ(tail.bounds().y, 85); EQ(outer.desiredSize().height, 100);
    });
    test("grid / nested in horizontal stack derives width from children", [&] {
        ui::Stack outer(ui::Stack::Orientation::Horizontal); outer.gap = 5;
        auto g = std::make_unique<ui::Grid>(); auto* gp = g.get(); g->columns = {star(), star()};
        add(*g, 20, 10); add(*g, 50, 10, 0, 1);
        outer.add(std::move(g)); auto& tail = add(outer, 10, 10);
        layout(outer, c, 400, 100);
        EQ(gp->bounds().width, 70); EQ(tail.bounds().x, 75); EQ(outer.desiredSize().width, 85);
    });
    test("grid / default row uses fixed parent slot for child centering", [&] {
        ui::Grid parent; parent.rows = {px(52)};
        auto header = std::make_unique<ui::Grid>(); header->columns = {px(32), star()};
        auto& icon = add(*header, 32, 32); icon.verticalAlignment = ui::Alignment::Center;
        auto& label = add(*header, 80, 28, 0, 1); label.verticalAlignment = ui::Alignment::Center;
        parent.add(std::move(header)); layout(parent, c, 300, 200);
        RECT(icon.bounds(), 0, 10, 32, 32); RECT(label.bounds(), 32, 12, 268, 28);
    });
    test("grid / spans include intervening gaps on both axes", [&] {
        ui::Grid g; g.columns = {px(40), px(60), px(80)}; g.rows = {px(20), px(30), px(40)}; g.gap = 7;
        auto& a = add(g, 0, 0, 1, 1, 2, 2);
        layout(g, c, 300, 200, 11, 13);
        RECT(a.bounds(), 58, 40, 147, 77);
    });
    test("grid / span grows auto tracks after fixed tracks", [&] {
        ui::Grid g; g.columns = {px(40), automatic(), automatic()}; g.gap = 5;
        auto& a = add(g, 150, 10, 0, 0, 1, 3);
        layout(g, c, 300, 100);
        EQ(g.desiredSize().width, 150); EQ(a.bounds().width, 150);
    });
    test("grid / auto span constraints are independent of declaration order", [&] {
        for (bool spanFirst : {false, true}) {
            ui::Grid g; g.columns = {automatic(), automatic()};
            if (spanFirst) add(g, 100, 10, 0, 0, 1, 2);
            auto& a = add(g, 80, 10);
            if (!spanFirst) add(g, 100, 10, 0, 0, 1, 2);
            layout(g, c, 300, 100);
            EQ(g.desiredSize().width, 100); EQ(a.bounds().width, 90);
        }
    });
    test("grid / fixed tracks survive deficit and stars collapse to zero", [&] {
        ui::Grid g; g.columns = {px(100), star()}; g.gap = 10;
        auto& a = add(g, 0, 0); auto& b = add(g, 0, 0, 0, 1);
        layout(g, c, 50, 100);
        EQ(a.bounds().width, 100); EQ(b.bounds().width, 0); EQ(b.bounds().x, 110); YES(g.overflow);
    });
    test("grid / child constraints do not redistribute its track", [&] {
        ui::Grid g; g.columns = {star(), star()};
        auto& a = add(g, 20, 20); a.width = 30;
        auto& b = add(g, 20, 20, 0, 1); b.maxWidth = 50; b.horizontalAlignment = ui::Alignment::End;
        layout(g, c, 200, 100);
        RECT(a.bounds(), 0, 0, 30, 100); RECT(b.bounds(), 180, 0, 20, 100);
        EQ(a.layoutSlot().width, 100); EQ(b.layoutSlot().width, 100);
    });
    test("grid / wrapping height is measured after column allocation", [&] {
        ui::Grid g; g.columns = {star(), star()}; g.rows = {automatic(), star()};
        auto& a = add(g, 400, 10); a.wrapping = true; auto& b = add(g, 0, 0, 1);
        layout(g, c, 200, 200);
        SIZE(a.desiredSize(), 100, 40); EQ(a.bounds().height, 40); EQ(b.bounds().y, 40);
    });
    test("grid / changed arrange width reflows auto rows", [&] {
        ui::Grid g; g.rows = {automatic(), star()};
        auto& a = add(g, 400, 10); a.wrapping = true; auto& b = add(g, 0, 0, 1);
        layout(g, c, 200, 200); EQ(b.bounds().y, 20);
        g.arrange(c, {10, 20, 100, 200}); EQ(b.bounds().y, 60); EQ(a.bounds().height, 40);
        g.arrange(c, {0, 0, 400, 200}); EQ(b.bounds().y, 10);
    });
    test("grid / content changes recompute auto tracks", [&] {
        ui::Grid g; g.columns = {automatic(), star()}; auto& a = add(g, 30, 20);
        auto& b = add(g, 0, 0, 0, 1);
        layout(g, c, 200, 100); EQ(b.bounds().x, 30);
        a.natural.width = 90; layout(g, c, 200, 100);
        EQ(b.bounds().x, 90); EQ(b.bounds().width, 110);
        a.natural.width = 10; layout(g, c, 200, 100); EQ(b.bounds().x, 10);
    });
    test("grid / weighted star rounding conserves pixels in both dimensions", [&] {
        for (bool rows : {false, true}) for (int extent = 0; extent <= 257; ++extent) {
            ui::Grid g; auto definitions = std::vector<ui::Track>{star(0.5), star(1.5), star(3)};
            if (rows) g.rows = definitions; else g.columns = definitions;
            auto& a = add(g, 0, 0); auto& b = add(g, 0, 0, rows ? 1 : 0, rows ? 0 : 1);
            auto& d = add(g, 0, 0, rows ? 2 : 0, rows ? 0 : 2);
            layout(g, c, rows ? 100 : extent, rows ? extent : 100, 7, 11);
            int aw = rows ? a.bounds().height : a.bounds().width;
            int bw = rows ? b.bounds().height : b.bounds().width;
            int dw = rows ? d.bounds().height : d.bounds().width;
            EQ(aw + bw + dw, extent); YES(aw >= 0 && bw >= 0 && dw >= 0);
            YES(std::abs(aw - extent * 0.1) <= 1);
            YES(std::abs(bw - extent * 0.3) <= 1);
            YES(std::abs(dw - extent * 0.6) <= 1);
            EQ(rows ? d.bounds().y + dw : d.bounds().x + dw, extent + (rows ? 11 : 7));
        }
    });
    test("grid / deterministic repeated layout and translated descendants", [&] {
        ui::Grid g; g.columns = {star(), star(2)}; g.gap = 9;
        auto& a = add(g, 0, 0); auto& b = add(g, 0, 0, 0, 1);
        layout(g, c, 309, 100); auto before = b.bounds();
        for (int i = 0; i < 20; ++i) {
            layout(g, c, 309, 100, i, 2*i);
            RECT(a.bounds(), i, 2*i, 100, 100);
            RECT(b.bounds(), before.x + i, before.y + 2*i, before.width, before.height);
        }
    });
}

void textMetrics(ui::LayoutContext& c) {
    FontSet fonts;
    auto style = PresentationStyle::defaults(); c.renderer.setStyle(&style);
    if (!fonts.load(style)) { ++failures; fprintf(stderr, "Cannot load bundled test fonts\n"); return; }
    const auto variants = fonts.variants();
    int line = c.renderer.textHeight(fonts.get(FontType::Regular));
    test("text / whole phrase intrinsic measurement and explicit newlines", [&] {
        ui::Text one("Hello", variants, style.textColor), two("Hello World", variants, style.textColor);
        one.measure(c, {1000, 1000}); two.measure(c, {1000, 1000});
        YES(two.desiredSize().width > one.desiredSize().width); EQ(two.desiredSize().height, line);
        ui::Text multi("Hello\nWorld", variants, style.textColor); multi.measure(c, {1000, 1000});
        EQ(multi.desiredSize().height, 2 * line);
    });
    test("text / nowrap retains a single line under tight width", [&] {
        ui::Text t("99.9 %", variants, style.textColor); layout(t, c, 10, 200);
        EQ(t.desiredSize().height, line); YES(t.desiredSize().width > 10); YES(t.overflow);
    });
    test("text / predictable two-line wrap from independent font widths", [&] {
        int w = static_cast<int>(std::ceil(fonts.get(FontType::Regular).measureString("Hello")));
        ui::Text t("Hello Hello", variants, style.textColor); t.wrap = true;
        layout(t, c, w, 200); EQ(t.desiredSize().height, 2 * line);
        layout(t, c, 1000, 200); EQ(t.desiredSize().height, line);
    });
    test("text / formatted intrinsic width excludes markup", [&] {
        ui::Text t("<b>Hello</b>", variants, style.textColor); t.measure(c, {1000, 1000});
        EQ(t.desiredSize().width, static_cast<int>(std::ceil(fonts.get(FontType::Bold).measureString("Hello"))));
        EQ(t.desiredSize().height, line);
    });
    test("text / actual grid row height follows wrapped text", [&] {
        int w = static_cast<int>(std::ceil(fonts.get(FontType::Regular).measureString("Hello")));
        ui::Grid g; g.columns = {px(w), star()}; g.rows = {automatic(), star()};
        auto t = std::make_unique<ui::Text>("Hello Hello", variants, style.textColor); t->wrap = true;
        g.add(std::move(t)); auto& next = add(g, 0, 0, 1);
        layout(g, c, 300, 300); EQ(next.bounds().y, 2 * line);
    });
    test("text / empty content remains a single blank line", [&] {
        ui::Text empty("", variants, style.textColor); empty.wrap = true;
        layout(empty, c, 0, 100); SIZE(empty.desiredSize(), 0, line);
    });
    test("text / margin and border padding add to measured line size", [&] {
        auto t = std::make_unique<ui::Text>("Hello", variants, style.textColor);
        int w = static_cast<int>(std::ceil(fonts.get(FontType::Regular).measureString("Hello")));
        t->margin = {3, 4, 5, 6};
        ui::Border b(std::move(t)); b.style.padding = {7, 8, 9, 10};
        b.measure(c, {1000, 1000}); SIZE(b.desiredSize(), w + 24, line + 28);
    });
    test("text / width maximum controls measurement before wrapping", [&] {
        int w = static_cast<int>(std::ceil(fonts.get(FontType::Regular).measureString("Hello")));
        ui::Text t("Hello Hello", variants, style.textColor); t.wrap = true;
        t.width = 1000; t.maxWidth = w;
        layout(t, c, 1000, 300); EQ(t.bounds().width, w); EQ(t.desiredSize().height, 2 * line);
    });
    test("text / unbreakable word preserves intrinsic width and reports overflow", [&] {
        ui::Text t("Unbreakable", variants, style.textColor); t.wrap = true;
        layout(t, c, 10, 100);
        EQ(t.desiredSize().width, static_cast<int>(std::ceil(fonts.get(FontType::Regular).measureString("Unbreakable"))));
        EQ(t.desiredSize().height, line); YES(t.overflow);
    });
    c.renderer.setStyle(nullptr);
}

void regressions(ui::LayoutContext& c) {
    test("grid / row span order invariance", [&] {
        for (bool spanFirst : {false, true}) {
            ui::Grid g; g.rows = {automatic(), automatic()};
            if (spanFirst) add(g, 10, 100, 0, 0, 2);
            auto& a = add(g, 10, 80);
            if (!spanFirst) add(g, 10, 100, 0, 0, 2);
            layout(g, c, 300, 300);
            EQ(g.desiredSize().height, 100); EQ(a.bounds().height, 90);
        }
    });
    test("grid / all declaration permutations of overlapping auto spans", [&] {
        // Permuting XML siblings changes paint order only, not track sizes.
        int order[] = {0, 1, 2, 3};
        ui::Rect expected[4]{};
        bool first = true;
        do {
            ui::Grid g; g.columns = {automatic(), automatic(), automatic()}; g.rows = {automatic()};
            Probe* nodes[4]{};
            for (int id : order) {
                if (id == 0) nodes[id] = &add(g, 80, 10, 0, 0);
                if (id == 1) nodes[id] = &add(g, 100, 20, 0, 0, 1, 2);
                if (id == 2) nodes[id] = &add(g, 120, 30, 0, 1, 1, 2);
                if (id == 3) nodes[id] = &add(g, 180, 40, 0, 0, 1, 3);
            }
            layout(g, c, 500, 500);
            for (int id = 0; id < 4; ++id) {
                if (first) expected[id] = nodes[id]->bounds();
                RECT(nodes[id]->bounds(), expected[id].x, expected[id].y, expected[id].width, expected[id].height);
                YES(nodes[id]->layoutSlot().width >= nodes[id]->natural.width);
            }
            first = false;
        } while (std::next_permutation(std::begin(order), std::end(order)));
    });
    test("grid / weighted shares subtract gaps before allocation", [&] {
        for (int gap : {0, 1, 7, 24}) for (int width = 2 * gap; width < 2 * gap + 80; ++width) {
            ui::Grid g; g.gap = gap; g.columns = {star(), star(2), star(3)};
            auto& a = add(g, 0, 0); auto& b = add(g, 0, 0, 0, 1); auto& d = add(g, 0, 0, 0, 2);
            layout(g, c, width, 100);
            EQ(b.bounds().x, a.bounds().width + gap);
            EQ(d.bounds().x, b.bounds().x + b.bounds().width + gap);
            EQ(a.bounds().width + b.bounds().width + d.bounds().width + 2 * gap, width);
            EQ(d.bounds().x + d.bounds().width, width);
        }
    });
    test("grid / zero-sized fixed track is distinct from auto and star", [&] {
        ui::Grid g; g.columns = {px(0), automatic(), star()};
        auto& a = add(g, 30, 20); auto& b = add(g, 50, 20, 0, 1);
        auto& d = add(g, 0, 0, 0, 2);
        layout(g, c, 200, 100);
        EQ(a.bounds().width, 0); YES(a.overflow);
        EQ(b.bounds().width, 50); EQ(d.bounds().width, 150);
    });
    test("grid / zero available space stays nonnegative", [&] {
        ui::Grid g; g.columns = {star(), star()}; g.rows = {star(), star()};
        auto& a = add(g, 0, 0, 0, 0); auto& b = add(g, 0, 0, 1, 1);
        layout(g, c, 0, 0);
        RECT(a.bounds(), 0, 0, 0, 0); RECT(b.bounds(), 0, 0, 0, 0);
    });
    test("grid / spanning wrapped leaf measures using the complete span width", [&] {
        ui::Grid g; g.columns = {px(50), px(70)}; g.rows = {automatic(), star()}; g.gap = 10;
        auto& a = add(g, 260, 15, 0, 0, 1, 2); a.wrapping = true;
        auto& b = add(g, 0, 0, 1);
        layout(g, c, 300, 200);
        EQ(a.measured.width, 130); EQ(a.bounds().height, 30); EQ(b.bounds().y, 40);
    });
    test("grid / margins are subtracted before wrapped height calculation", [&] {
        ui::Grid g; g.rows = {automatic(), star()};
        auto& a = add(g, 200, 10); a.wrapping = true; a.margin = {10, 5, 10, 7};
        auto& b = add(g, 0, 0, 1);
        layout(g, c, 120, 300);
        SIZE(a.measured, 100, 20); SIZE(a.desiredSize(), 120, 32);
        RECT(a.bounds(), 10, 5, 100, 20); EQ(b.bounds().y, 32);
    });
    test("nested / deep border chain preserves desired size and offsets", [&] {
        auto leaf = std::make_unique<Probe>(20, 10); auto* p = leaf.get();
        std::unique_ptr<ui::Element> root = std::move(leaf);
        for (int i = 0; i < 12; ++i) {
            auto b = std::make_unique<ui::Border>(std::move(root));
            b->style.padding = ui::Thickness(2); root = std::move(b);
        }
        layout(*root, c, 300, 200, 11, 13);
        SIZE(root->desiredSize(), 68, 58); RECT(p->bounds(), 35, 37, 252, 152);
    });
    test("nested / border grid stack composition exact geometry", [&] {
        auto grid = std::make_unique<ui::Grid>(); grid->rows = {px(30), star()}; grid->gap = 5;
        auto& heading = add(*grid, 10, 10);
        auto stack = std::make_unique<ui::Stack>(); stack->gap = 7;
        auto& a = add(*stack, 30, 20); auto& b = add(*stack, 40, 25);
        grid->add(std::move(stack), 1);
        ui::Border root(std::move(grid)); root.margin = ui::Thickness(10); root.style.padding = ui::Thickness(5);
        layout(root, c, 300, 200, 2, 4);
        RECT(heading.bounds(), 17, 19, 270, 30);
        RECT(a.bounds(), 17, 54, 270, 20); RECT(b.bounds(), 17, 81, 270, 25);
    });
    test("element / render does not call measure or arrange overrides", [&] {
        Probe p; layout(p, c, 200, 100); p.render(c);
        EQ(p.renders, 1); EQ(p.measurements.size(), 1); EQ(p.arranges, 1);
        p.width = p.height = 0; layout(p, c, 200, 100); p.render(c);
        EQ(p.renders, 1);
    });
}
}

int main() {
    Renderer renderer;
    ui::LayoutContext context{renderer};
    elements(context); stacks(context); borders(context); grids(context); regressions(context); textMetrics(context);
    printf("\nLayout: %d cases, %d checks, %d failures\n", cases, checks, failures);
    return failures ? 1 : 0;
}
