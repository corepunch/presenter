#include "font.h"
#include "renderer.h"
#include "constants.h"
#include "common.h"
#include <SDL2/SDL.h>
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
    test_formatted_text_rendering(sdlR, fonts, renderer, style);
    test_empty_title(sdlR, fonts, renderer, style);

    renderer.cleanup();
    SDL_DestroyRenderer(sdlR);
    SDL_DestroyWindow(window);
    SDL_Quit();

    fprintf(stderr, "\n%d/%d assertions passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
