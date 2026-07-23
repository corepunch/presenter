// Test suite for layout selection rules and part geometry
// No SDL needed - tests pure logic from layout.h/layout.cpp

#include "layout.h"
#include "constants.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg) \
    do { \
        ++tests_run; \
        if (!(expr)) { \
            std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, msg); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        ++tests_run; \
        if ((a) != (b)) { \
            std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, msg); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

// ---------------------------------------------------------------------------
// Layout selection tests
// ---------------------------------------------------------------------------

static int test_explicit_title_layout() {
    std::fprintf(stderr, "test_explicit_title_layout...\n");
    Slide slide;
    slide.type = SlideType::Title;
    slide.layoutSpecified = true;
    slide.title = "Hello";
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::Title), "explicit Title -> LayoutKind::Title");
    return 0;
}

static int test_explicit_section_layout() {
    std::fprintf(stderr, "test_explicit_section_layout...\n");
    Slide slide;
    slide.type = SlideType::Section;
    slide.layoutSpecified = true;
    slide.title = "Part Two";
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::Section), "explicit Section -> LayoutKind::Section");
    return 0;
}

static int test_auto_section_empty() {
    std::fprintf(stderr, "test_auto_section_empty...\n");
    Slide slide;
    slide.type = SlideType::Content; // default
    slide.layoutSpecified = false;
    slide.title = "Empty Slide";
    // No bullets, no image, no columns, no subtitle
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::Section), "empty content -> Section");
    return 0;
}

static int test_auto_header_body_with_bullets() {
    std::fprintf(stderr, "test_auto_header_body_with_bullets...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "Points";
    slide.bullets.push_back("A");
    slide.bullets.push_back("B");
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderBody), "bullets -> HeaderBody");
    return 0;
}

static int test_auto_header_body_with_subtitle() {
    std::fprintf(stderr, "test_auto_header_body_with_subtitle...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "Title";
    slide.subtitle = "A subtitle";
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderBody), "subtitle -> HeaderBody");
    return 0;
}

static int test_auto_two_column_with_two_blocks() {
    std::fprintf(stderr, "test_auto_two_column_with_two_blocks...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "Comparison";
    slide.blocks.push_back("Left side");
    slide.blocks.push_back("Right side");
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderTwoColumn), "2 blocks -> HeaderTwoColumn");
    return 0;
}

static int test_auto_two_column_three_blocks() {
    std::fprintf(stderr, "test_auto_two_column_three_blocks...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "Comparison";
    slide.blocks.push_back("A");
    slide.blocks.push_back("B");
    slide.blocks.push_back("C");
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderTwoColumn), "3+ blocks -> HeaderTwoColumn");
    return 0;
}

static int test_auto_header_image() {
    std::fprintf(stderr, "test_auto_header_image...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "Diagram";
    slide.imagePath = "/tmp/test.png";
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderImage), "image -> HeaderImage");
    return 0;
}

static int test_image_overrides_body() {
    std::fprintf(stderr, "test_image_overrides_body...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "Both";
    slide.bullets.push_back("text");
    slide.imagePath = "/tmp/test.png";
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderImage), "image wins over bullets");
    return 0;
}

static int test_image_overrides_columns() {
    std::fprintf(stderr, "test_image_overrides_columns...\n");
    Slide slide;
    slide.type = SlideType::Content;
    slide.layoutSpecified = false;
    slide.title = "All";
    slide.blocks.push_back("L");
    slide.blocks.push_back("R");
    slide.imagePath = "/tmp/test.png";
    LayoutKind kind = selectLayout(slide);
    ASSERT_EQ(static_cast<int>(kind), static_cast<int>(LayoutKind::HeaderImage), "image wins over multiple blocks");
    return 0;
}

// ---------------------------------------------------------------------------
// Part geometry tests
// ---------------------------------------------------------------------------

static LayoutMetrics makeMetrics(int w, int h) {
    LayoutMetrics m;
    m.slideW = w;
    m.slideH = h;
    m.titleAscent = 40.0f;
    m.titleDescent = -10.0f;
    m.titleLineH = 50.0f;
    m.bodyAscent = 20.0f;
    m.bodyDescent = -5.0f;
    m.bodyLineH = 25.0f;
    return m;
}

static int test_title_parts_single_fullslide() {
    std::fprintf(stderr, "test_title_parts_single_fullslide...\n");
    Slide slide;
    slide.title = "Hello";
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::Title, slide, m);
    ASSERT_EQ(parts.size(), (size_t)1, "Title layout has 1 part");
    ASSERT_EQ(static_cast<int>(parts[0].role), static_cast<int>(PartRole::FullSlide), "role is FullSlide");
    ASSERT_EQ(parts[0].rect.x, SLIDE_MARGIN, "x = SLIDE_MARGIN");
    ASSERT_EQ(parts[0].rect.w, 1280 - 2 * SLIDE_MARGIN, "w = slideW - 2*margin");
    ASSERT_EQ(parts[0].rect.y, 0, "y = 0");
    ASSERT_EQ(parts[0].rect.h, 720, "h = slideH");
    return 0;
}

static int test_section_parts_single_fullslide() {
    std::fprintf(stderr, "test_section_parts_single_fullslide...\n");
    Slide slide;
    slide.title = "Part Two";
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::Section, slide, m);
    ASSERT_EQ(parts.size(), (size_t)1, "Section layout has 1 part");
    ASSERT_EQ(static_cast<int>(parts[0].role), static_cast<int>(PartRole::FullSlide), "role is FullSlide");
    return 0;
}

static int test_header_body_parts_count() {
    std::fprintf(stderr, "test_header_body_parts_count...\n");
    Slide slide;
    slide.title = "Points";
    slide.bullets.push_back("A");
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::HeaderBody, slide, m);
    ASSERT_EQ(parts.size(), (size_t)3, "HeaderBody has 3 parts");
    ASSERT_EQ(static_cast<int>(parts[0].role), static_cast<int>(PartRole::Header), "part 0 is Header");
    ASSERT_EQ(static_cast<int>(parts[1].role), static_cast<int>(PartRole::Body), "part 1 is Body");
    ASSERT_EQ(static_cast<int>(parts[2].role), static_cast<int>(PartRole::Footer), "part 2 is Footer");
    return 0;
}

static int test_header_body_header_height() {
    std::fprintf(stderr, "test_header_body_header_height...\n");
    Slide slide;
    slide.title = "Test";
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::HeaderBody, slide, m);
    int expectedHeaderH = static_cast<int>(m.titleLineH) + 2 * PART_PADDING;
    ASSERT_EQ(parts[0].rect.h, expectedHeaderH, "header height = titleLineH + 2*PART_PADDING");
    return 0;
}

static int test_header_two_column_parts_count() {
    std::fprintf(stderr, "test_header_two_column_parts_count...\n");
    Slide slide;
    slide.title = "Compare";
    slide.blocks.push_back("L");
    slide.blocks.push_back("R");
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::HeaderTwoColumn, slide, m);
    ASSERT_EQ(parts.size(), (size_t)4, "HeaderTwoColumn has 4 parts");
    ASSERT_EQ(static_cast<int>(parts[0].role), static_cast<int>(PartRole::Header), "part 0 is Header");
    ASSERT_EQ(static_cast<int>(parts[1].role), static_cast<int>(PartRole::ColumnLeft), "part 1 is ColumnLeft");
    ASSERT_EQ(static_cast<int>(parts[2].role), static_cast<int>(PartRole::ColumnRight), "part 2 is ColumnRight");
    ASSERT_EQ(static_cast<int>(parts[3].role), static_cast<int>(PartRole::Footer), "part 3 is Footer");
    return 0;
}

static int test_header_two_column_equal_widths() {
    std::fprintf(stderr, "test_header_two_column_equal_widths...\n");
    Slide slide;
    slide.title = "Compare";
    slide.blocks.push_back("L");
    slide.blocks.push_back("R");
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::HeaderTwoColumn, slide, m);
    int contentW = 1280 - 2 * SLIDE_MARGIN;
    int expectedColW = (contentW - COLUMN_GAP) / 2;
    ASSERT_EQ(parts[1].rect.w, expectedColW, "left col width");
    ASSERT_EQ(parts[2].rect.w, expectedColW, "right col width");
    ASSERT_EQ(parts[2].rect.x, parts[1].rect.x + expectedColW + COLUMN_GAP, "right col x offset");
    return 0;
}

static int test_header_image_parts_count() {
    std::fprintf(stderr, "test_header_image_parts_count...\n");
    Slide slide;
    slide.title = "Diagram";
    slide.imagePath = "/tmp/test.png";
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::HeaderImage, slide, m);
    ASSERT_EQ(parts.size(), (size_t)4, "HeaderImage has 4 parts");
    ASSERT_EQ(static_cast<int>(parts[0].role), static_cast<int>(PartRole::Header), "part 0 is Header");
    ASSERT_EQ(static_cast<int>(parts[1].role), static_cast<int>(PartRole::Image), "part 1 is Image");
    ASSERT_EQ(static_cast<int>(parts[2].role), static_cast<int>(PartRole::Caption), "part 2 is Caption");
    ASSERT_EQ(static_cast<int>(parts[3].role), static_cast<int>(PartRole::Footer), "part 3 is Footer");
    return 0;
}

static int test_footer_right_aligned() {
    std::fprintf(stderr, "test_footer_right_aligned...\n");
    Slide slide;
    slide.title = "Test";
    LayoutMetrics m = makeMetrics(1280, 720);
    auto parts = computeParts(LayoutKind::HeaderBody, slide, m);
    const SlidePart& footer = parts[2];
    ASSERT_EQ(footer.rect.y, 720 - footer.rect.h, "footer at bottom of slide");
    ASSERT_EQ(footer.rect.x, SLIDE_MARGIN, "footer x = margin");
    ASSERT_EQ(footer.rect.w, 1280 - 2 * SLIDE_MARGIN, "footer width = content width");
    return 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    int failures = 0;

    int results[] = {
        test_explicit_title_layout(),
        test_explicit_section_layout(),
        test_auto_section_empty(),
        test_auto_header_body_with_bullets(),
        test_auto_header_body_with_subtitle(),
        test_auto_two_column_with_two_blocks(),
        test_auto_two_column_three_blocks(),
        test_auto_header_image(),
        test_image_overrides_body(),
        test_image_overrides_columns(),
        test_title_parts_single_fullslide(),
        test_section_parts_single_fullslide(),
        test_header_body_parts_count(),
        test_header_body_header_height(),
        test_header_two_column_parts_count(),
        test_header_two_column_equal_widths(),
        test_header_image_parts_count(),
        test_footer_right_aligned(),
    };

    const char* names[] = {
        "explicit_title_layout",
        "explicit_section_layout",
        "auto_section_empty",
        "auto_header_body_with_bullets",
        "auto_header_body_with_subtitle",
        "auto_two_column_with_two_blocks",
        "auto_two_column_three_blocks",
        "auto_header_image",
        "image_overrides_body",
        "image_overrides_columns",
        "title_parts_single_fullslide",
        "section_parts_single_fullslide",
        "header_body_parts_count",
        "header_body_header_height",
        "header_two_column_parts_count",
        "header_two_column_equal_widths",
        "header_image_parts_count",
        "footer_right_aligned",
    };

    int numTests = sizeof(results) / sizeof(results[0]);
    for (int i = 0; i < numTests; ++i) {
        if (results[i] != 0) {
            ++failures;
            std::fprintf(stderr, "FAILED: %s\n", names[i]);
        } else {
            std::fprintf(stderr, "PASSED: %s\n", names[i]);
        }
    }

    std::fprintf(stderr, "\n========================================\n");
    std::fprintf(stderr, "Results: %d/%d assertions passed", tests_passed, tests_run);
    if (failures > 0) {
        std::fprintf(stderr, ", %d test(s) FAILED\n", failures);
    } else {
        std::fprintf(stderr, ", all tests PASSED\n");
    }
    std::fprintf(stderr, "========================================\n");

    return failures > 0 ? 1 : 0;
}