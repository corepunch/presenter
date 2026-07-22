#include "font.h"
#include "renderer.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                         \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__);          \
            tests_failed++;                                            \
        } else {                                                       \
            printf("  PASS: %s\n", msg);                               \
            tests_passed++;                                            \
        }                                                              \
    } while (0)

#define TEST_ASSERT_NEAR(a, b, eps, msg)                               \
    TEST_ASSERT(std::fabs((a) - (b)) < (eps), msg)

// ── FontAtlas tests ────────────────────────────────────────────────────────

static void test_measureString_nonempty(const FontAtlas& font) {
    float w = font.measureString("Hello");
    TEST_ASSERT(w > 0, "measureString(\"Hello\") returns width > 0");
}

static void test_measureString_empty(const FontAtlas& font) {
    float w = font.measureString("");
    TEST_ASSERT(w == 0.0f, "measureString(\"\") returns 0");
}

static void test_measureString_additive(const FontAtlas& font) {
    float wA  = font.measureString("A");
    float wB  = font.measureString("B");
    float wAB = font.measureString("AB");
    TEST_ASSERT_NEAR(wAB, wA + wB, 2.0f,
        "measureString(\"AB\") ≈ measureString(\"A\") + measureString(\"B\")");
}

static void test_glyph_validity(const FontAtlas& font) {
    bool allValid = true;
    const char* ranges[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                             "abcdefghijklmnopqrstuvwxyz",
                             "0123456789" };
    for (const char* r : ranges) {
        for (const char* p = r; *p; ++p) {
            const GlyphInfo& g = font.getGlyph(*p);
            if (g.width <= 0 || g.height <= 0) {
                allValid = false;
                printf("    glyph '%c': width=%d height=%d\n", *p, g.width, g.height);
            }
        }
    }
    TEST_ASSERT(allValid,
        "getGlyph for A-Z, a-z, 0-9 has valid dimensions (width>0, height>0)");
}

static void test_space_glyph(const FontAtlas& font) {
    const GlyphInfo& g = font.getGlyph(' ');
    TEST_ASSERT(g.xadvance > 0, "getGlyph(' ').xadvance > 0");
}

// ── Renderer::wordWrap tests ──────────────────────────────────────────────

static void test_wordWrap_short_text(const FontAtlas& font, Renderer& renderer) {
    auto lines = renderer.wordWrap("Hello", font, 1920);
    TEST_ASSERT(lines.size() == 1,
        "wordWrap short text produces single line");
}

static void test_wordWrap_long_text(const FontAtlas& font, Renderer& renderer) {
    std::string longText = "This is a fairly long sentence that should definitely "
                           "wrap onto more than one line when given a narrow width";
    auto lines = renderer.wordWrap(longText, font, 200);
    TEST_ASSERT(lines.size() > 1,
        "wordWrap long text produces multiple lines");
}

static void test_wordWrap_preserves_words(const FontAtlas& font, Renderer& renderer) {
    std::string input = "alpha beta gamma delta epsilon";
    auto lines = renderer.wordWrap(input, font, 200);

    // Reconstruct all words from the wrapped output
    std::string reconstructed;
    for (size_t i = 0; i < lines.size(); i++) {
        if (!reconstructed.empty()) reconstructed += " ";
        reconstructed += lines[i];
    }

    TEST_ASSERT(reconstructed == input,
        "wordWrap preserves all input words in order");
}

// ── Rendering smoke tests ─────────────────────────────────────────────────

static Slide makeSlide(SlideType type, const std::string& title,
                       const std::vector<std::string>& bullets = {},
                       const std::string& left = "",
                       const std::string& right = "",
                       const std::string& notes = "") {
    Slide s;
    s.type = type;
    s.title = title;
    s.bullets = bullets;
    s.leftContent = left;
    s.rightContent = right;
    s.notes = notes;
    return s;
}

static void test_renderSlide_content(SDL_Renderer* sdlRenderer,
                                     const FontSet& fonts, Renderer& renderer) {
    std::vector<std::string> bullets = {
        "First bullet point with some text",
        "Second bullet point with more text to test wrapping",
        "Third bullet",
        "Fourth bullet that is also quite long and should wrap in a narrow view",
        "Fifth"
    };
    Slide slide = makeSlide(SlideType::Content, "Content Title", bullets);
    SDL_Texture* tex = renderer.renderSlide(slide, fonts);
    TEST_ASSERT(tex != nullptr,
        "renderSlide content type returns valid texture (no crash)");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_renderSlide_title(SDL_Renderer* sdlRenderer,
                                   const FontSet& fonts, Renderer& renderer) {
    Slide slide = makeSlide(SlideType::Title, "My Title", {"Subtitle"});
    SDL_Texture* tex = renderer.renderSlide(slide, fonts);
    TEST_ASSERT(tex != nullptr,
        "renderSlide title type returns valid texture (no crash)");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_renderSlide_section(SDL_Renderer* sdlRenderer,
                                     const FontSet& fonts, Renderer& renderer) {
    Slide slide = makeSlide(SlideType::Section, "Section Break");
    SDL_Texture* tex = renderer.renderSlide(slide, fonts);
    TEST_ASSERT(tex != nullptr,
        "renderSlide section type returns valid texture (no crash)");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_renderSlide_twoColumn(SDL_Renderer* sdlRenderer,
                                       const FontSet& fonts, Renderer& renderer) {
    Slide slide = makeSlide(SlideType::TwoColumn, "Two Columns",
                            {}, "Left side content here",
                            "Right side content here");
    SDL_Texture* tex = renderer.renderSlide(slide, fonts);
    TEST_ASSERT(tex != nullptr,
        "renderSlide two-column type returns valid texture (no crash)");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_renderSlide_image(SDL_Renderer* sdlRenderer,
                                   const FontSet& fonts, Renderer& renderer) {
    Slide slide = makeSlide(SlideType::Image, "Image Slide");
    slide.imagePath = "nonexistent.png";
    slide.imageAlt = "Alt text";
    SDL_Texture* tex = renderer.renderSlide(slide, fonts);
    TEST_ASSERT(tex != nullptr,
        "renderSlide image type returns valid texture (no crash, missing image handled)");
    if (tex) SDL_DestroyTexture(tex);
}

static void test_renderSlide_all_types(SDL_Renderer* sdlRenderer,
                                       const FontSet& fonts, Renderer& renderer) {
    std::vector<Slide> slides = {
        makeSlide(SlideType::Title, "T", {"Sub"}),
        makeSlide(SlideType::Content, "C", {"a","b","c"}),
        makeSlide(SlideType::Section, "S"),
        makeSlide(SlideType::TwoColumn, "2C", {}, "L", "R"),
        makeSlide(SlideType::Image, "I"),
    };
    bool allOk = true;
    for (auto& s : slides) {
        if (s.type == SlideType::Image) {
            s.imagePath = "nope.png";
        }
        SDL_Texture* tex = renderer.renderSlide(s, fonts);
        if (!tex) {
            allOk = false;
        } else {
            SDL_DestroyTexture(tex);
        }
    }
    TEST_ASSERT(allOk, "renderSlide all slide types produce textures without crash");
}

static void test_presenter_view(SDL_Renderer* sdlRenderer,
                                const FontSet& fonts, Renderer& renderer) {
    Presentation pres;
    pres.slides = {
        makeSlide(SlideType::Title, "Slide 1", {"Sub 1"}, "", "", "Note 1"),
        makeSlide(SlideType::Content, "Slide 2", {"a","b"}, "", "", "Note 2"),
        makeSlide(SlideType::Section, "Slide 3"),
    };
    pres.current = 0;
    SDL_Texture* tex = renderer.renderPresenterView(pres, fonts);
    TEST_ASSERT(tex != nullptr,
        "renderPresenterView returns valid texture (no crash)");
    if (tex) SDL_DestroyTexture(tex);
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "test_textbounds", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_HIDDEN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    FontSet fonts;
    if (!fonts.load(32.0f)) {
        fprintf(stderr, "Failed to load fonts\n");
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Renderer renderer;
    if (!renderer.init(sdlRenderer, 1280, 720)) {
        fprintf(stderr, "Renderer::init failed\n");
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("=== FontAtlas tests ===\n");
    test_measureString_nonempty(fonts.get(FontType::Regular));
    test_measureString_empty(fonts.get(FontType::Regular));
    test_measureString_additive(fonts.get(FontType::Regular));
    test_glyph_validity(fonts.get(FontType::Regular));
    test_space_glyph(fonts.get(FontType::Regular));

    printf("\n=== Renderer::wordWrap tests ===\n");
    test_wordWrap_short_text(fonts.get(FontType::Regular), renderer);
    test_wordWrap_long_text(fonts.get(FontType::Regular), renderer);
    test_wordWrap_preserves_words(fonts.get(FontType::Regular), renderer);

    printf("\n=== Rendering smoke tests ===\n");
    test_renderSlide_content(sdlRenderer, fonts, renderer);
    test_renderSlide_title(sdlRenderer, fonts, renderer);
    test_renderSlide_section(sdlRenderer, fonts, renderer);
    test_renderSlide_twoColumn(sdlRenderer, fonts, renderer);
    test_renderSlide_image(sdlRenderer, fonts, renderer);
    test_renderSlide_all_types(sdlRenderer, fonts, renderer);
    test_presenter_view(sdlRenderer, fonts, renderer);

    renderer.cleanup();
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
