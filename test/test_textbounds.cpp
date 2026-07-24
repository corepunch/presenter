#include "font.h"
#include "renderer.h"
#include "layout.h"
#include "ui.hpp"
#include "constants.h"
#include "common.h"
#include "charts.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(expr, msg) \
    do { ++tests_run; if (!(expr)) { fprintf(stderr, "FAIL [%d]: %s\n", __LINE__, msg); } else ++tests_passed; } while(0)

static int initRenderer(SDL_Renderer** r, SDL_Window** w, int width, int height) {
    *w = SDL_CreateWindow("test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                          width, height, SDL_WINDOW_HIDDEN);
    if (!*w) return -1;
    *r = SDL_CreateRenderer(*w, -1, SDL_RENDERER_SOFTWARE);
    return *r ? 0 : -1;
}

static Slide makeSlide(SlideLayout layout, const std::string& title,
                       const std::vector<std::string>& texts = {},
                       const std::string& notes = "") {
    Slide s;
    s.layout = layout;
    s.title = title;
    s.texts = texts;
    s.notes = notes;
    return s;
}

static void test_content(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Content, "Content Title", {"First bullet", "Second", "Third"});
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "content slide renders");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_title(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Title, "My Title");
    slide.subtitle = "Subtitle";
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "title slide renders");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_section(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Section, "Section Break");
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "section slide renders");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_columns(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Columns, "Two Columns");
    slide.cols = 2;
    Slide left; left.texts = {"Left item 1", "Left item 2"}; left.layout = SlideLayout::Content;
    Slide right; right.texts = {"Right item 1"}; right.layout = SlideLayout::Content;
    slide.children = {left, right};
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "columns slide renders");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_image(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Image, "Image Slide");
    slide.imagePath = "nope.png";
    slide.caption = "Image caption";
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "image slide renders");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_all_types(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    std::vector<Slide> slides = {
        makeSlide(SlideLayout::Title, "T"),
        makeSlide(SlideLayout::Content, "C", {"a","b"}),
        makeSlide(SlideLayout::Section, "S"),
        makeSlide(SlideLayout::Image, "I"),
    };
    slides[3].imagePath = "nope.png";

    bool allOk = true;
    for (auto& s : slides) {
        SDL_Texture* tex = renderer.renderSlide(s, fonts, style);
        if (!tex) allOk = false;
        else SDL_DestroyTexture(tex);
    }
    TEST_ASSERT(allOk, "all slide types render without crash");
}

static void test_presenter_view(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer) {
    Presentation pres;
    pres.style = PresentationStyle::defaults();
    pres.slides = {
        makeSlide(SlideLayout::Title, "Slide 1", {}, "Note 1"),
        makeSlide(SlideLayout::Content, "Slide 2", {"a","b"}, "Note 2"),
        makeSlide(SlideLayout::Section, "Slide 3"),
    };
    SDL_Texture* tex = renderer.renderPresenterView(pres, fonts);
    TEST_ASSERT(tex != nullptr, "presenter view renders");
    if (tex) SDL_DestroyTexture(tex);
}

class FixedElement final : public ui::Element {
public:
    FixedElement(int width, int height) : m_size{width, height} {}

protected:
    ui::Size measureOverride(ui::LayoutContext&, ui::Size) override {
        return m_size;
    }

private:
    ui::Size m_size;
};

static void test_vertical_stack_grow(Renderer& renderer) {
    ui::LayoutContext context{renderer};
    ui::Stack stack;
    stack.margin = ui::Thickness(10);
    stack.gap = 5;

    auto header = std::make_unique<FixedElement>(100, 30);
    FixedElement* headerPtr = header.get();
    stack.add(std::move(header));

    auto notes = std::make_unique<FixedElement>(100, 80);
    FixedElement* notesPtr = notes.get();
    stack.add(std::move(notes), 1.0f);

    auto next = std::make_unique<FixedElement>(100, 40);
    FixedElement* nextPtr = next.get();
    stack.add(std::move(next));

    stack.measure(context, {300, 200});
    stack.arrange(context, {0, 0, 300, 200});

    TEST_ASSERT(headerPtr->layoutSlot().height == 30,
                "vertical stack preserves fixed header height");
    TEST_ASSERT(notesPtr->layoutSlot().height == 100,
                "vertical stack gives remaining height to grow child");
    TEST_ASSERT(nextPtr->layoutSlot().y == 150,
                "vertical stack keeps trailing child inside final bounds");
}

static void test_formatted_text_rendering(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Content, "Formatted Text");
    slide.texts = {
        "<b>Bold</b> and <i>italic</i> text",
        "Inline <code>monospace</code> here",
        "<b>Nested <i>bold-italic</i></b> formatting",
        "No formatting",
    };
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "formatted text slide renders without crash");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_empty_title(SDL_Renderer* sdlRenderer, const FontSet& fonts, Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Content, "", {"Body without title"});
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "empty title slide renders without crash");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_body_vertical_centering(SDL_Renderer*, const FontSet& fonts,
                                         Renderer& renderer, const PresentationStyle& style) {
    Slide slide = makeSlide(SlideLayout::Content, "Centered",
                            {"First item", "Second item", "Third item"});
    SDL_Texture* tex = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(tex != nullptr, "centered body slide renders");
    if (tex) SDL_DestroyTexture(tex);

    const Font& titleFont = fonts.titleVariants().get(FontType::Regular);
    const Font& bodyFont = fonts.variants().get(FontType::Regular);
    LayoutMetrics metrics = {
        renderer.width(), renderer.height(),
        titleFont.getAscent(), titleFont.getDescent(),
        titleFont.getAscent() - titleFont.getDescent(),
        bodyFont.getAscent(), bodyFont.getDescent(),
        bodyFont.getAscent() - bodyFont.getDescent()
    };
    auto parts = computeParts(LayoutKind::HeaderBody, slide, metrics, style);
    const Rect& body = parts[1].rect;
    Uint32 bg = style.bgColor.toUint32(renderer.surface()->format);
    auto* pixels = static_cast<Uint32*>(renderer.surface()->pixels);
    int pitch = renderer.surface()->pitch / 4;
    int minY = body.y + body.h;
    int maxY = body.y;
    for (int y = body.y; y < body.y + body.h; ++y) {
        for (int x = body.x; x < body.x + body.w; ++x) {
            if (pixels[y * pitch + x] != bg) {
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    int bodyCenter = body.y + body.h / 2;
    int inkCenter = (minY + maxY) / 2;
    TEST_ASSERT(minY <= maxY, "body text produces visible pixels");
    TEST_ASSERT(std::abs(inkCenter - bodyCenter) <= 20, "body text is vertically centered");
}

static void test_oversized_body_is_scaled_inside_bounds(SDL_Renderer*,
                                                        const FontSet& fonts,
                                                        Renderer& renderer,
                                                        const PresentationStyle& style) {
    Slide slide;
    slide.layout = SlideLayout::Content;
    slide.title = "Scaled code";
    CodeBlock first;
    first.lang = "cpp";
    CodeBlock second;
    second.lang = "cpp";
    for (int i = 0; i < 18; ++i) {
        first.code += "int first_value = 123456789;\n";
        second.code += "int second_value = 987654321;\n";
    }
    slide.codeBlocks = {first, second};

    SDL_Texture* texture = renderer.renderSlide(slide, fonts, style);
    TEST_ASSERT(texture != nullptr, "oversized body renders after scaling");
    if (texture) SDL_DestroyTexture(texture);

    const Font& titleFont = fonts.titleVariants().get(FontType::Regular);
    const Font& bodyFont = fonts.variants().get(FontType::Regular);
    LayoutMetrics metrics = {
        renderer.width(), renderer.height(),
        titleFont.getAscent(), titleFont.getDescent(),
        titleFont.getAscent() - titleFont.getDescent(),
        bodyFont.getAscent(), bodyFont.getDescent(),
        bodyFont.getAscent() - bodyFont.getDescent()
    };
    auto parts = computeParts(LayoutKind::HeaderBody, slide, metrics, style);
    const Rect& body = parts[1].rect;
    const Rect& footer = parts[2].rect;
    Uint32 bg = style.bgColor.toUint32(renderer.surface()->format);
    auto* pixels = static_cast<Uint32*>(renderer.surface()->pixels);
    int pitch = renderer.surface()->pitch / 4;
    bool gapIsClear = true;
    for (int y = body.y + body.h; y < footer.y && gapIsClear; ++y) {
        // The right-aligned slide number's glyph ascent can reach slightly
        // above the footer rectangle, so inspect the left half where only
        // overflowing body content could produce ink.
        for (int x = body.x; x < body.x + body.w / 2; ++x) {
            if (pixels[y * pitch + x] != bg) {
                gapIsClear = false;
                break;
            }
        }
    }
    TEST_ASSERT(gapIsClear, "scaled body does not overflow into footer gap");
}

static void test_charts_and_icons(SDL_Renderer*, const FontSet& fonts,
                                  Renderer& renderer,
                                  const PresentationStyle& style) {
    bool allRendered = true;
    const ChartType types[] = {
        ChartType::Bar, ChartType::Line, ChartType::Pie, ChartType::Donut
    };
    for (ChartType type : types) {
        Slide slide = makeSlide(SlideLayout::Content, "Chart");
        Chart chart;
        chart.type = type;
        chart.title = "Q3 results";
        chart.icon = "chart-line";
        chart.height = 260;
        chart.points = {{"Q1", 42}, {"Q2", 58}, {"Q3", 81}};
        slide.charts.push_back(chart);
        SDL_Texture* texture = renderer.renderSlide(slide, fonts, style);
        if (!texture) allRendered = false;
        else SDL_DestroyTexture(texture);
    }

    Slide icons = makeSlide(SlideLayout::Content, "Icon rows");
    icons.icons = {
        {"rocket", "Launch velocity"},
        {"users", "Customer growth"},
    };
    SDL_Texture* texture = renderer.renderSlide(icons, fonts, style);
    if (!texture) allRendered = false;
    else SDL_DestroyTexture(texture);

    Slide bullets = makeSlide(SlideLayout::Content, "Custom bullets",
                              {"Launch", "Source", "Standard"});
    bullets.textIcons = {"rocket", "none", ""};
    texture = renderer.renderSlide(bullets, fonts, style);
    if (!texture) allRendered = false;
    else SDL_DestroyTexture(texture);
    if (iconCodepoint("rocket") == 0 || iconCodepoint("camera") == 0)
        allRendered = false;
    TEST_ASSERT(allRendered, "bar, line, pie, donut, and icon rows render");
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "SDL_Init failed\n"); return 1; }

    SDL_Window* window;
    SDL_Renderer* sdlR;
    if (initRenderer(&sdlR, &window, 640, 480) < 0) {
        fprintf(stderr, "Failed to init renderer\n");
        SDL_Quit();
        return 1;
    }

    FontSet fonts;
    if (!fonts.load(PresentationStyle::defaults())) { fprintf(stderr, "Fonts failed\n"); SDL_Quit(); return 1; }

    Renderer renderer;
    renderer.init(sdlR, 640, 480);
    auto style = PresentationStyle::defaults();

    test_content(sdlR, fonts, renderer, style);
    test_title(sdlR, fonts, renderer, style);
    test_section(sdlR, fonts, renderer, style);
    test_columns(sdlR, fonts, renderer, style);
    test_image(sdlR, fonts, renderer, style);
    test_all_types(sdlR, fonts, renderer, style);
    test_presenter_view(sdlR, fonts, renderer);
    test_vertical_stack_grow(renderer);
    test_formatted_text_rendering(sdlR, fonts, renderer, style);
    test_empty_title(sdlR, fonts, renderer, style);
    test_body_vertical_centering(sdlR, fonts, renderer, style);
    test_oversized_body_is_scaled_inside_bounds(sdlR, fonts, renderer, style);
    test_charts_and_icons(sdlR, fonts, renderer, style);

    renderer.cleanup();
    SDL_DestroyRenderer(sdlR);
    SDL_DestroyWindow(window);
    SDL_Quit();

    fprintf(stderr, "\n%d/%d assertions passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
