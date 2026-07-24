#include "parser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg) \
    do { ++tests_run; if (!(expr)) { fprintf(stderr, "  FAIL [%d]: %s\n", __LINE__, msg); return 1; } ++tests_passed; } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { ++tests_run; if ((a) != (b)) { fprintf(stderr, "  FAIL [%d]: %s (got %zu, exp %zu)\n", __LINE__, msg, (size_t)(a), (size_t)(b)); return 1; } ++tests_passed; } while(0)

#define ASSERT_STR_EQ(a, b, msg) \
    do { ++tests_run; if ((a) != (b)) { fprintf(stderr, "  FAIL [%d]: %s\n  got: \"%s\"\n  exp: \"%s\"\n", __LINE__, msg, (a).c_str(), (b).c_str()); return 1; } ++tests_passed; } while(0)

static const char* TMP_DIR = "/tmp/xml_test";

static void ensureTmpDir() { mkdir(TMP_DIR, 0755); }

static std::string writeTmp(const char* name, const std::string& content) {
    std::string path = std::string(TMP_DIR) + "/" + name;
    std::ofstream f(path); f << content; f.close();
    return path;
}

static void cleanupTmpDir() {
    const char* files[] = {"empty.xml", "title.xml", "content.xml", "columns.xml", "recursive.xml",
                           "formatted.xml", "legacy_title.xml", "nested_title.xml", "no_title.xml",
                           "style_inherited.xml", "style_explicit.xml", "charts.xml",
                           "chart_style.xml"};
    for (auto f : files) unlink((std::string(TMP_DIR) + "/" + f).c_str());
    rmdir(TMP_DIR);
}

static int test_empty() {
    fprintf(stderr, "test_empty...\n");
    Presentation p = parseXml(writeTmp("empty.xml", "<presentation></presentation>"));
    ASSERT_EQ(p.slides.size(), (size_t)0, "empty");
    return 0;
}

static int test_title() {
    fprintf(stderr, "test_title...\n");
    Presentation p = parseXml(writeTmp("title.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"title\" title=\"Hello\">\n"
        "    <notes>Welcome</notes>\n"
        "    <subtitle>Sub</subtitle>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT(p.slides[0].layout == SlideLayout::Title, "title layout");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Hello"), "title");
    ASSERT_STR_EQ(p.slides[0].subtitle, std::string("Sub"), "subtitle");
    ASSERT_STR_EQ(p.slides[0].notes, std::string("Welcome"), "notes");
    return 0;
}

static int test_content() {
    fprintf(stderr, "test_content...\n");
    Presentation p = parseXml(writeTmp("content.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"content\" title=\"Key Points\">\n"
        "    <text>First</text>\n"
        "    <text>Second</text>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT(p.slides[0].layout == SlideLayout::Content, "content layout");
    ASSERT_EQ(p.slides[0].texts.size(), (size_t)2, "2 texts");
    ASSERT_STR_EQ(p.slides[0].texts[0], std::string("First"), "text 0");
    return 0;
}

static int test_columns() {
    fprintf(stderr, "test_columns...\n");
    Presentation p = parseXml(writeTmp("columns.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"columns\" gap=\"24\" title=\"Before and After\">\n"
        "    <notes>Point out the diff</notes>\n"
        "    <slide slot=\"left\">\n"
        "      <image src=\"./before.png\"/>\n"
        "      <text>Old UI</text>\n"
        "    </slide>\n"
        "    <slide slot=\"right\">\n"
        "      <image src=\"./after.png\"/>\n"
        "      <text>New UI</text>\n"
        "    </slide>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT(p.slides[0].layout == SlideLayout::Columns, "columns layout");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Before and After"), "title");
    ASSERT_EQ(p.slides[0].children.size(), (size_t)2, "2 children");
    ASSERT_STR_EQ(p.slides[0].children[0].slot, std::string("left"), "left slot");
    ASSERT_STR_EQ(p.slides[0].children[1].slot, std::string("right"), "right slot");
    ASSERT(p.slides[0].children[0].imagePath.find("before.png") != std::string::npos, "before image");
    ASSERT(p.slides[0].children[1].imagePath.find("after.png") != std::string::npos, "after image");
    ASSERT_STR_EQ(p.slides[0].children[0].texts[0], std::string("Old UI"), "before caption");
    ASSERT_STR_EQ(p.slides[0].children[1].texts[0], std::string("New UI"), "after caption");
    return 0;
}

static int test_recursive() {
    fprintf(stderr, "test_recursive...\n");
    Presentation p = parseXml(writeTmp("recursive.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"columns\" cols=\"2\" title=\"Deep\">\n"
        "    <slide slot=\"left\">\n"
        "      <slide layout=\"columns\" cols=\"2\">\n"
        "        <slide slot=\"0\"><text>A1</text></slide>\n"
        "        <slide slot=\"1\"><text>A2</text></slide>\n"
        "      </slide>\n"
        "    </slide>\n"
        "    <slide slot=\"right\"><text>B</text></slide>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT_EQ(p.slides[0].children.size(), (size_t)2, "2 children");
    ASSERT_EQ(p.slides[0].children[0].children.size(), (size_t)1, "left child has 1 nested");
    ASSERT_EQ(p.slides[0].children[0].children[0].children.size(), (size_t)2, "nested columns has 2 children");
    ASSERT_EQ(p.slides[0].children[0].children[0].children[0].texts.size(), (size_t)1, "deep text");
    return 0;
}

static int test_formatted_text() {
    fprintf(stderr, "test_formatted_text...\n");
    Presentation p = parseXml(writeTmp("formatted.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"content\" title=\"Formatting Test\">\n"
        "    <text>Plain text</text>\n"
        "    <text><b>Bold</b> text here</text>\n"
        "    <text>Some <i>italic</i> markup</text>\n"
        "    <text>Run <code>npm install</code> first</text>\n"
        "    <text><b>Bold <i>and italic</i> combined</b></text>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT_EQ(p.slides[0].texts.size(), (size_t)5, "5 text elements");
    ASSERT_STR_EQ(p.slides[0].texts[0], std::string("Plain text"), "plain text");
    ASSERT_STR_EQ(p.slides[0].texts[1], std::string("<b>Bold</b> text here"), "bold tag preserved");
    ASSERT_STR_EQ(p.slides[0].texts[3], std::string("Run <code>npm install</code> first"), "code tag preserved");
    ASSERT_STR_EQ(p.slides[0].texts[4], std::string("<b>Bold <i>and italic</i> combined</b>"), "nested tags preserved");
    return 0;
}

static int test_legacy_title_ignored() {
    fprintf(stderr, "test_legacy_title_ignored...\n");
    Presentation p = parseXml(writeTmp("legacy_title.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"content\" title=\"From Attribute\">\n"
        "    <title>Legacy Child Element</title>\n"
        "    <text>Body content</text>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT_STR_EQ(p.slides[0].title, std::string("From Attribute"), "title from attribute wins");
    ASSERT_EQ(p.slides[0].texts.size(), (size_t)1, "1 text (legacy <title> not stored as text)");
    ASSERT_STR_EQ(p.slides[0].texts[0], std::string("Body content"), "body text preserved");
    return 0;
}

static int test_nested_title_attrs() {
    fprintf(stderr, "test_nested_title_attrs...\n");
    Presentation p = parseXml(writeTmp("nested_title.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"columns\" gap=\"24\" title=\"Main Title\">\n"
        "    <slide slot=\"left\" title=\"Left Column\">\n"
        "      <text>Left content</text>\n"
        "    </slide>\n"
        "    <slide slot=\"right\" title=\"Right Column\">\n"
        "      <text>Right content</text>\n"
        "    </slide>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 slide");
    ASSERT_STR_EQ(p.slides[0].title, std::string("Main Title"), "parent title");
    ASSERT_STR_EQ(p.slides[0].children[0].title, std::string("Left Column"), "left child title");
    ASSERT_STR_EQ(p.slides[0].children[1].title, std::string("Right Column"), "right child title");
    ASSERT_STR_EQ(p.slides[0].children[0].slot, std::string("left"), "left slot");
    ASSERT_STR_EQ(p.slides[0].children[1].slot, std::string("right"), "right slot");
    return 0;
}

static int test_no_title() {
    fprintf(stderr, "test_no_title...\n");
    Presentation p = parseXml(writeTmp("no_title.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide layout=\"content\">\n"
        "    <text>Body without title</text>\n"
        "  </slide>\n"
        "  <slide layout=\"section\"/>\n"
        "  <slide layout=\"columns\" cols=\"2\">\n"
        "    <slide slot=\"0\"><text>A</text></slide>\n"
        "    <slide slot=\"1\"><text>B</text></slide>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)3, "3 slides");
    ASSERT_STR_EQ(p.slides[0].title, std::string(""), "content slide empty title");
    ASSERT_STR_EQ(p.slides[1].title, std::string(""), "section slide empty title");
    ASSERT_STR_EQ(p.slides[2].title, std::string(""), "columns slide empty title");
    ASSERT_EQ(p.slides[2].children.size(), (size_t)2, "columns has 2 children");
    return 0;
}

static int test_presenter_corner_radius_style() {
    fprintf(stderr, "test_presenter_corner_radius_style...\n");
    PresentationStyle defaults = PresentationStyle::defaults();
    ASSERT_EQ(defaults.presenterCornerRadius, defaults.cornerRadius / 2,
              "default presenter radius is half the slide radius");

    PresentationStyle inherited = PresentationStyle::load(writeTmp(
        "style_inherited.xml",
        "<style><layout cornerRadius=\"30\"/></style>"));
    ASSERT_EQ(inherited.cornerRadius, 30, "custom slide radius");
    ASSERT_EQ(inherited.presenterCornerRadius, 15,
              "presenter radius follows custom slide radius at half");

    PresentationStyle explicitStyle = PresentationStyle::load(writeTmp(
        "style_explicit.xml",
        "<style><layout cornerRadius=\"30\" presenterCornerRadius=\"7\"/></style>"));
    ASSERT_EQ(explicitStyle.presenterCornerRadius, 7,
              "explicit presenter radius overrides inherited half");
    return 0;
}

static int test_charts_and_icons() {
    fprintf(stderr, "test_charts_and_icons...\n");
    Presentation p = parseXml(writeTmp("charts.xml",
        "<?xml version=\"1.0\"?>\n<presentation>\n"
        "  <slide title=\"Metrics\">\n"
        "    <text icon=\"rocket\">Launch</text>\n"
        "    <text icon=\"none\">Source line</text>\n"
        "    <chart type=\"line\" title=\"Revenue\" icon=\"chart-line\" height=\"300\" showValues=\"false\">\n"
        "      <point label=\"Q1\" value=\"12.5\"/>\n"
        "      <point label=\"Q2\" value=\"18\"/>\n"
        "    </chart>\n"
        "    <icon name=\"rocket\">Launch velocity</icon>\n"
        "  </slide>\n</presentation>\n"));
    ASSERT_EQ(p.slides.size(), (size_t)1, "1 chart slide");
    ASSERT_EQ(p.slides[0].textIcons.size(), (size_t)2, "2 text icon settings");
    ASSERT_STR_EQ(p.slides[0].textIcons[0], std::string("rocket"), "custom bullet icon");
    ASSERT_STR_EQ(p.slides[0].textIcons[1], std::string("none"), "suppressed bullet icon");
    ASSERT_EQ(p.slides[0].charts.size(), (size_t)1, "1 chart");
    ASSERT(p.slides[0].charts[0].type == ChartType::Line, "line chart type");
    ASSERT_EQ(p.slides[0].charts[0].points.size(), (size_t)2, "2 chart points");
    ASSERT_STR_EQ(p.slides[0].charts[0].points[0].label, std::string("Q1"), "point label");
    ASSERT(p.slides[0].charts[0].points[0].value == 12.5, "point value");
    ASSERT_EQ(p.slides[0].charts[0].height, 300, "chart height");
    ASSERT(!p.slides[0].charts[0].showValues, "showValues false");
    ASSERT_EQ(p.slides[0].icons.size(), (size_t)1, "1 icon");
    ASSERT_STR_EQ(p.slides[0].icons[0].name, std::string("rocket"), "icon name");
    ASSERT_STR_EQ(p.slides[0].icons[0].text, std::string("Launch velocity"), "icon text");

    PresentationStyle style = PresentationStyle::load(writeTmp(
        "chart_style.xml",
        "<style><charts series1=\"#123456\" grid=\"#ABCDEF\"/></style>"));
    ASSERT(style.chartSeries1.r == 0x12 && style.chartSeries1.g == 0x34 &&
           style.chartSeries1.b == 0x56, "custom first chart series color");
    ASSERT(style.chartGrid.r == 0xAB && style.chartGrid.g == 0xCD &&
           style.chartGrid.b == 0xEF, "custom chart grid color");
    return 0;
}

int main() {
    ensureTmpDir();
    int failures = 0;
    int results[] = {
        test_empty(), test_title(), test_content(), test_columns(), test_recursive(),
        test_formatted_text(), test_legacy_title_ignored(), test_nested_title_attrs(), test_no_title(),
        test_presenter_corner_radius_style(), test_charts_and_icons(),
    };
    const char* names[] = {
        "empty", "title", "content", "columns", "recursive",
        "formatted_text", "legacy_title_ignored", "nested_title_attrs", "no_title",
        "presenter_corner_radius_style", "charts_and_icons",
    };
    int numTests = sizeof(results) / sizeof(results[0]);
    for (int i = 0; i < numTests; ++i) {
        if (results[i]) { ++failures; fprintf(stderr, "FAILED: %s\n", names[i]); }
        else fprintf(stderr, "PASSED: %s\n", names[i]);
    }
    cleanupTmpDir();
    fprintf(stderr, "\n%d/%d passed\n", tests_passed, tests_run);
    return failures ? 1 : 0;
}
