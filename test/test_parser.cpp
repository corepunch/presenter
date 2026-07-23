// Comprehensive parser test suite for parser.h/parser.cpp
// Include path: /Users/ICHERNA/Developer/presenter/include
// Compile: g++ -std=c++17 -I../include test_parser.cpp -o test_parser
// Run: ./test_parser

#include "parser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

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
            std::fprintf(stderr, "  FAIL [line %d]: %s (got %zu, expected %zu)\n", \
                         __LINE__, msg, (size_t)(a), (size_t)(b)); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

#define ASSERT_STR_EQ(a, b, msg) \
    do { \
        ++tests_run; \
        if ((a) != (b)) { \
            std::fprintf(stderr, "  FAIL [line %d]: %s\n  got:      \"%s\"\n  expected: \"%s\"\n", \
                         __LINE__, msg, (a).c_str(), (b).c_str()); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

static const char* TMP_DIR = "/tmp/parser_test";

static void ensureTmpDir() {
    mkdir(TMP_DIR, 0755);
}

static std::string writeTmpFile(const char* name, const std::string& content) {
    std::string path = std::string(TMP_DIR) + "/" + name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path;
}

static void cleanupTmpDir() {
    // Remove test files (best-effort, ignore errors)
    const char* files[] = {
        "empty.md",
        "single_title.md",
        "content_bullets.md",
        "image_slide.md",
        "section_slide.md",
        "two_column.md",
        "multiple_slides.md",
        "notes_slide.md",
        "notes_not_leaked.md",
        "image_path.md",
        "blank_lines.md",
        "default_layout.md",
    };
    for (auto f : files) {
        std::string p = std::string(TMP_DIR) + "/" + f;
        unlink(p.c_str());
    }
    rmdir(TMP_DIR);
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

static int test_empty_file() {
    std::fprintf(stderr, "test_empty_file...\n");
    std::string path = writeTmpFile("empty.md", "");
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)0, "empty file should produce 0 slides");
    return 0;
}

static int test_single_title_slide() {
    std::fprintf(stderr, "test_single_title_slide...\n");
    std::string path = writeTmpFile("single_title.md",
        "layout: Title\n"
        "# My Presentation\n"
        "Subtitle here\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT(p.slides[0].type == SlideType::Title, "slide type should be Title");
    ASSERT_STR_EQ(p.slides[0].title, std::string("My Presentation"), "title should match");
    // For Title layout, subtitle goes into imageAlt
    ASSERT_STR_EQ(p.slides[0].imageAlt, std::string("Subtitle here"), "subtitle should be in imageAlt");
    return 0;
}

static int test_content_slide_with_bullets() {
    std::fprintf(stderr, "test_content_slide_with_bullets...\n");
    std::string path = writeTmpFile("content_bullets.md",
        "layout: Content\n"
        "# Key Points\n"
        "- First bullet\n"
        "- Second bullet\n"
        "- Third bullet\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT(p.slides[0].type == SlideType::Content, "slide type should be Content");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Key Points"), "title should match");
    ASSERT_EQ(p.slides[0].bullets.size(), (size_t)3, "should have 3 bullets");
    ASSERT_STR_EQ(p.slides[0].bullets[0], std::string("First bullet"), "bullet 0");
    ASSERT_STR_EQ(p.slides[0].bullets[1], std::string("Second bullet"), "bullet 1");
    ASSERT_STR_EQ(p.slides[0].bullets[2], std::string("Third bullet"), "bullet 2");
    return 0;
}

static int test_image_slide() {
    std::fprintf(stderr, "test_image_slide...\n");
    std::string path = writeTmpFile("image_slide.md",
        "layout: Image\n"
        "# Architecture\n"
        "![System diagram](./images/arch.png)\n"
        "Caption text\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT(p.slides[0].type == SlideType::Image, "slide type should be Image");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Architecture"), "title should match");
    ASSERT_STR_EQ(p.slides[0].imageAlt, std::string("System diagram"), "image alt text");
    // imagePath should be absolute, resolved relative to the md file dir
    ASSERT(p.slides[0].imagePath.find("parser_test/images/arch.png") != std::string::npos,
           "image path should be resolved to absolute path");
    // Caption text becomes a bullet (non-image content)
    ASSERT_EQ(p.slides[0].bullets.size(), (size_t)1, "caption should be in bullets");
    ASSERT_STR_EQ(p.slides[0].bullets[0], std::string("Caption text"), "caption text");
    return 0;
}

static int test_section_slide() {
    std::fprintf(stderr, "test_section_slide...\n");
    std::string path = writeTmpFile("section_slide.md",
        "layout: Section\n"
        "# Part Two\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT(p.slides[0].type == SlideType::Section, "slide type should be Section");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Part Two"), "title should match");
    ASSERT_EQ(p.slides[0].bullets.size(), (size_t)0, "section should have no bullets");
    return 0;
}

static int test_two_column_slide() {
    std::fprintf(stderr, "test_two_column_slide...\n");
    std::string path = writeTmpFile("two_column.md",
        "# Comparison\n"
        "Left side content\n"
        "More left\n"
        "---\n"
        "Right side content\n"
        "More right\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Comparison"), "title should match");
    ASSERT_EQ(p.slides[0].blocks.size(), (size_t)2, "should have 2 blocks");
    ASSERT_STR_EQ(p.slides[0].blocks[0], std::string("Left side content\nMore left"),
                  "block 0 content");
    ASSERT_STR_EQ(p.slides[0].blocks[1], std::string("Right side content\nMore right"),
                  "block 1 content");
    return 0;
}

static int test_multiple_slides() {
    std::fprintf(stderr, "test_multiple_slides...\n");
    std::string path = writeTmpFile("multiple_slides.md",
        "layout: Title\n"
        "# Welcome\n"
        "---\n"
        "layout: Content\n"
        "# Agenda\n"
        "- Item one\n"
        "- Item two\n"
        "---\n"
        "layout: Section\n"
        "# Deep Dive\n"
        "---\n"
        "layout: Content\n"
        "# Summary\n"
        "- Takeaway 1\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)4, "should have 4 slides");
    ASSERT(p.slides[0].type == SlideType::Title, "slide 0 should be Title");
    ASSERT(p.slides[1].type == SlideType::Content, "slide 1 should be Content");
    ASSERT(p.slides[2].type == SlideType::Section, "slide 2 should be Section");
    ASSERT(p.slides[3].type == SlideType::Content, "slide 3 should be Content");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Welcome"), "slide 0 title");
    ASSERT_STR_EQ(p.slides[1].title, std::string("Agenda"), "slide 1 title");
    ASSERT_STR_EQ(p.slides[2].title, std::string("Deep Dive"), "slide 2 title");
    ASSERT_STR_EQ(p.slides[3].title, std::string("Summary"), "slide 3 title");
    ASSERT_EQ(p.slides[1].bullets.size(), (size_t)2, "slide 1 has 2 bullets");
    ASSERT_EQ(p.slides[3].bullets.size(), (size_t)1, "slide 3 has 1 bullet");
    return 0;
}

static int test_notes_slide() {
    std::fprintf(stderr, "test_notes_slide...\n");
    std::string path = writeTmpFile("notes_slide.md",
        "layout: Content\n"
        "notes: Remember to mention Q3 results and the new roadmap.\n"
        "# Results\n"
        "- Revenue up 12%\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT_STR_EQ(p.slides[0].notes,
                  std::string("Remember to mention Q3 results and the new roadmap."),
                  "notes should be populated");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Results"), "title should match");
    ASSERT_EQ(p.slides[0].bullets.size(), (size_t)1, "should have 1 bullet");
    ASSERT_STR_EQ(p.slides[0].bullets[0], std::string("Revenue up 12%"), "bullet content");
    return 0;
}

static int test_notes_not_leaked() {
    std::fprintf(stderr, "test_notes_not_leaked...\n");
    std::string path = writeTmpFile("notes_not_leaked.md",
        "layout: Content\n"
        "notes: This is an internal note not shown on the slide.\n"
        "# Public Title\n"
        "- Visible bullet\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    // Check notes are populated
    ASSERT(!p.slides[0].notes.empty(), "notes should be populated");
    // Check title doesn't contain notes text
    ASSERT(p.slides[0].title.find("internal note") == std::string::npos,
           "notes text should not appear in title");
    // Check bullets don't contain notes text
    for (size_t i = 0; i < p.slides[0].bullets.size(); ++i) {
        ASSERT(p.slides[0].bullets[i].find("internal note") == std::string::npos,
               "notes text should not appear in bullets");
    }
    return 0;
}

static int test_image_path_resolution() {
    std::fprintf(stderr, "test_image_path_resolution...\n");
    // Write to a nested subdirectory to test path resolution
    std::string subDir = std::string(TMP_DIR) + "/sub";
    mkdir(subDir.c_str(), 0755);
    std::string path = subDir + "/image_path.md";
    {
        std::ofstream f(path);
        f << "layout: Image\n"
             "# Diagram\n"
             "![A diagram](../images/diagram.png)\n";
        f.close();
    }
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT(p.slides[0].type == SlideType::Image, "slide type should be Image");
    // The image path should be resolved to an absolute path
    ASSERT(p.slides[0].imagePath.find("parser_test/images/diagram.png") != std::string::npos,
           "relative path should resolve correctly with ..");
    // Clean up subdirectory
    unlink(path.c_str());
    rmdir(subDir.c_str());
    return 0;
}

static int test_blank_lines_trimmed() {
    std::fprintf(stderr, "test_blank_lines_trimmed...\n");
    std::string path = writeTmpFile("blank_lines.md",
        "\n"
        "\n"
        "layout: Content\n"
        "\n"
        "# Trimmed Title\n"
        "\n"
        "- Bullet one\n"
        "\n"
        "\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Trimmed Title"), "title should be trimmed");
    ASSERT_EQ(p.slides[0].bullets.size(), (size_t)1, "should have 1 bullet");
    ASSERT_STR_EQ(p.slides[0].bullets[0], std::string("Bullet one"), "bullet content");
    return 0;
}

static int test_default_layout() {
    std::fprintf(stderr, "test_default_layout...\n");
    std::string path = writeTmpFile("default_layout.md",
        "# No Layout Specified\n"
        "- Bullet A\n"
        "- Bullet B\n"
    );
    Presentation p = parseMarkdown(path);
    ASSERT_EQ(p.slides.size(), (size_t)1, "should have 1 slide");
    ASSERT(p.slides[0].type == SlideType::Content,
           "missing layout should default to Content");
    ASSERT_STR_EQ(p.slides[0].title, std::string("No Layout Specified"), "title should match");
    ASSERT_EQ(p.slides[0].bullets.size(), (size_t)2, "should have 2 bullets");
    return 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    ensureTmpDir();

    int failures = 0;

    // Each test function returns 0 on success, 1 on failure
    int results[] = {
        test_empty_file(),
        test_single_title_slide(),
        test_content_slide_with_bullets(),
        test_image_slide(),
        test_section_slide(),
        test_two_column_slide(),
        test_multiple_slides(),
        test_notes_slide(),
        test_notes_not_leaked(),
        test_image_path_resolution(),
        test_blank_lines_trimmed(),
        test_default_layout(),
    };

    const char* names[] = {
        "empty_file",
        "single_title_slide",
        "content_slide_with_bullets",
        "image_slide",
        "section_slide",
        "two_column_slide",
        "multiple_slides",
        "notes_slide",
        "notes_not_leaked",
        "image_path_resolution",
        "blank_lines_trimmed",
        "default_layout",
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

    cleanupTmpDir();

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
