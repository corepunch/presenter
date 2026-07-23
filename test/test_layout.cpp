#include "layout.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_run = 0;
static int tests_passed = 0;
static PresentationStyle defStyle = PresentationStyle::defaults();

#define ASSERT(expr, msg) \
    do { ++tests_run; if (!(expr)) { fprintf(stderr, "  FAIL [%d]: %s\n", __LINE__, msg); return 1; } ++tests_passed; } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { ++tests_run; if ((a) != (b)) { fprintf(stderr, "  FAIL [%d]: %s\n", __LINE__, msg); return 1; } ++tests_passed; } while(0)

static int test_layout_title() {
    fprintf(stderr, "test_layout_title...\n");
    Slide slide; slide.layout = SlideLayout::Title;
    ASSERT_EQ((int)layoutFromSlide(slide), (int)LayoutKind::Title, "Title");
    return 0;
}

static int test_layout_section() {
    fprintf(stderr, "test_layout_section...\n");
    Slide slide; slide.layout = SlideLayout::Section;
    ASSERT_EQ((int)layoutFromSlide(slide), (int)LayoutKind::Section, "Section");
    return 0;
}

static int test_layout_content() {
    fprintf(stderr, "test_layout_content...\n");
    Slide slide; slide.layout = SlideLayout::Content;
    ASSERT_EQ((int)layoutFromSlide(slide), (int)LayoutKind::HeaderBody, "Content");
    return 0;
}

static int test_layout_image() {
    fprintf(stderr, "test_layout_image...\n");
    Slide slide; slide.layout = SlideLayout::Image;
    ASSERT_EQ((int)layoutFromSlide(slide), (int)LayoutKind::HeaderImage, "Image");
    return 0;
}

static int test_layout_columns() {
    fprintf(stderr, "test_layout_columns...\n");
    Slide slide; slide.layout = SlideLayout::Columns;
    ASSERT_EQ((int)layoutFromSlide(slide), (int)LayoutKind::Columns, "Columns");
    return 0;
}

static int test_layout_blank() {
    fprintf(stderr, "test_layout_blank...\n");
    Slide slide; slide.layout = SlideLayout::Blank;
    ASSERT_EQ((int)layoutFromSlide(slide), (int)LayoutKind::HeaderBody, "Blank");
    return 0;
}

static LayoutMetrics makeMetrics(int w, int h) {
    LayoutMetrics m;
    m.slideW = w; m.slideH = h;
    m.titleAscent = 40.0f; m.titleDescent = -10.0f; m.titleLineH = 50.0f;
    m.bodyAscent = 20.0f; m.bodyDescent = -5.0f; m.bodyLineH = 25.0f;
    return m;
}

static int test_parts_title() {
    fprintf(stderr, "test_parts_title...\n");
    Slide slide;
    auto parts = computeParts(LayoutKind::Title, slide, makeMetrics(1280, 720), defStyle);
    ASSERT_EQ(parts.size(), (size_t)1, "1 part");
    ASSERT_EQ((int)parts[0].role, (int)PartRole::FullSlide, "FullSlide");
    return 0;
}

static int test_parts_header_body() {
    fprintf(stderr, "test_parts_header_body...\n");
    Slide slide; slide.title = "T"; slide.texts = {"A", "B"};
    auto parts = computeParts(LayoutKind::HeaderBody, slide, makeMetrics(1280, 720), defStyle);
    ASSERT_EQ(parts.size(), (size_t)3, "3 parts");
    ASSERT_EQ((int)parts[0].role, (int)PartRole::Header, "Header");
    ASSERT_EQ((int)parts[1].role, (int)PartRole::Body, "Body");
    ASSERT_EQ((int)parts[2].role, (int)PartRole::Footer, "Footer");
    return 0;
}

static int test_parts_columns_2() {
    fprintf(stderr, "test_parts_columns_2...\n");
    Slide slide; slide.layout = SlideLayout::Columns; slide.title = "T";
    slide.cols = 2;
    Slide left; left.slot = "left"; left.texts = {"L"};
    Slide right; right.slot = "right"; right.texts = {"R"};
    slide.children = {left, right};
    auto parts = computeParts(LayoutKind::Columns, slide, makeMetrics(1280, 720), defStyle);
    ASSERT_EQ(parts.size(), (size_t)4, "4 parts (header, 2 slots, footer)");
    ASSERT_EQ((int)parts[0].role, (int)PartRole::Header, "Header");
    ASSERT_EQ((int)parts[1].role, (int)PartRole::Slot, "Slot 0");
    ASSERT_EQ(parts[1].childIndex, 0, "child 0");
    ASSERT_EQ((int)parts[2].role, (int)PartRole::Slot, "Slot 1");
    ASSERT_EQ(parts[2].childIndex, 1, "child 1");
    ASSERT_EQ((int)parts[3].role, (int)PartRole::Footer, "Footer");
    int contentW = 1280 - 2 * defStyle.slideMargin;
    int colW = (contentW - 24) / 2;
    ASSERT_EQ(parts[1].rect.w, colW, "col width");
    ASSERT_EQ(parts[2].rect.w, colW, "col width");
    return 0;
}

static int test_parts_columns_3() {
    fprintf(stderr, "test_parts_columns_3...\n");
    Slide slide; slide.layout = SlideLayout::Columns; slide.title = "T";
    slide.cols = 3; slide.gap = 12;
    Slide a, b, c;
    a.slot = "0"; b.slot = "1"; c.slot = "2";
    slide.children = {a, b, c};
    auto parts = computeParts(LayoutKind::Columns, slide, makeMetrics(1280, 720), defStyle);
    ASSERT_EQ(parts.size(), (size_t)5, "5 parts (header, 3 slots, footer)");
    ASSERT_EQ((int)parts[1].role, (int)PartRole::Slot, "Slot 0");
    ASSERT_EQ((int)parts[2].role, (int)PartRole::Slot, "Slot 1");
    ASSERT_EQ((int)parts[3].role, (int)PartRole::Slot, "Slot 2");
    return 0;
}

static int test_parts_columns_grid() {
    fprintf(stderr, "test_parts_columns_grid...\n");
    Slide slide; slide.layout = SlideLayout::Columns; slide.title = "G";
    slide.cols = 2; slide.gap = 12;
    for (int i = 0; i < 4; i++) slide.children.push_back(Slide{});
    auto parts = computeParts(LayoutKind::Columns, slide, makeMetrics(1280, 720), defStyle);
    ASSERT_EQ(parts.size(), (size_t)6, "6 parts (header, 4 slots, footer)");
    ASSERT_EQ(parts[1].rect.y, parts[2].rect.y, "row 0 same y (slots 0,1)");
    ASSERT_EQ(parts[3].rect.y, parts[4].rect.y, "row 1 same y (slots 2,3)");
    ASSERT(parts[3].rect.y > parts[1].rect.y, "row 1 below row 0");
    return 0;
}

static int test_parts_header_image() {
    fprintf(stderr, "test_parts_header_image...\n");
    Slide slide; slide.title = "T"; slide.imagePath = "/tmp/x.png";
    auto parts = computeParts(LayoutKind::HeaderImage, slide, makeMetrics(1280, 720), defStyle);
    ASSERT_EQ(parts.size(), (size_t)4, "4 parts");
    ASSERT_EQ((int)parts[0].role, (int)PartRole::Header, "Header");
    ASSERT_EQ((int)parts[1].role, (int)PartRole::Image, "Image");
    ASSERT_EQ((int)parts[2].role, (int)PartRole::Caption, "Caption");
    ASSERT_EQ((int)parts[3].role, (int)PartRole::Footer, "Footer");
    ASSERT_EQ(parts[2].rect.y,
              parts[1].rect.y + parts[1].rect.h + defStyle.partGap,
              "caption follows image area by configured gap");
    return 0;
}

static int test_image_caption_stack_is_centered() {
    fprintf(stderr, "test_image_caption_stack_is_centered...\n");
    Rect available = {100, 200, 400, 500};
    ImageCaptionStack stack = computeImageCaptionStack(
        available, 1600, 900, 50, 20, ImageFit::Fit);

    int stackTop = stack.image.y;
    int stackBottom = stack.caption.y + stack.caption.h;
    int spaceAbove = stackTop - available.y;
    int spaceBelow = available.y + available.h - stackBottom;
    ASSERT(std::abs(spaceAbove - spaceBelow) <= 1, "whole stack vertically centered");
    ASSERT_EQ(stack.caption.y, stack.image.y + stack.image.h + 20,
              "caption follows rendered image, not its maximum area");
    ASSERT_EQ(stack.image.w, available.w, "landscape image uses available width");
    return 0;
}

static int test_image_caption_stack_accounts_for_wrapped_caption() {
    fprintf(stderr, "test_image_caption_stack_accounts_for_wrapped_caption...\n");
    Rect available = {0, 0, 300, 600};
    ImageCaptionStack oneLine = computeImageCaptionStack(
        available, 1200, 800, 40, 16, ImageFit::Fit);
    ImageCaptionStack twoLines = computeImageCaptionStack(
        available, 1200, 800, 80, 16, ImageFit::Fit);

    ASSERT(twoLines.image.y < oneLine.image.y,
           "a taller caption moves the centered stack upward");
    ASSERT_EQ(twoLines.caption.h, 80, "measured caption height is retained");
    return 0;
}

int main() {
    int failures = 0;
    int results[] = {
        test_layout_title(), test_layout_section(), test_layout_content(),
        test_layout_image(), test_layout_columns(), test_layout_blank(),
        test_parts_title(), test_parts_header_body(),
        test_parts_columns_2(), test_parts_columns_3(), test_parts_columns_grid(),
        test_parts_header_image(),
        test_image_caption_stack_is_centered(),
        test_image_caption_stack_accounts_for_wrapped_caption(),
    };
    const char* names[] = {
        "title", "section", "content", "image", "columns", "blank",
        "parts_title", "parts_header_body",
        "parts_columns_2", "parts_columns_3", "parts_columns_grid",
        "parts_header_image",
        "image_caption_stack_centered",
        "image_caption_stack_wrapped_caption",
    };
    for (int i = 0; i < 14; ++i) {
        if (results[i]) { ++failures; fprintf(stderr, "FAILED: %s\n", names[i]); }
        else fprintf(stderr, "PASSED: %s\n", names[i]);
    }
    fprintf(stderr, "\n%d/%d passed\n", tests_passed, tests_run);
    return failures ? 1 : 0;
}
