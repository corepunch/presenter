#include "check.h"
#include "font.h"
#include "renderer.h"
#include "image.h"
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

static int checks = 0;
#define REQUIRE(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static bool has(const CheckReport& r, const std::string& code) {
    for (const auto& i : r.issues) if (i.code == code) return true;
    return false;
}
static const CheckIssue& find(const CheckReport& r, const std::string& code) {
    for (const auto& i : r.issues) if (i.code == code) return i;
    std::fprintf(stderr, "Missing issue %s: %s", code.c_str(), formatCheckReport(r, false).c_str()); std::exit(1);
}

int main() {
    REQUIRE(SDL_Init(0) == 0);
    Presentation pres;
    pres.style = PresentationStyle::builtInThemes()[0];
    FontSet fonts; REQUIRE(fonts.load(pres.style));
    Renderer renderer;
    auto run = [&](LayoutNode node) {
        Slide slide; slide.elements.push_back(std::move(node)); pres.slides = {slide};
        return checkPresentation(pres, fonts, renderer);
    };
    LayoutNode text; text.kind = "text"; text.text = "Readable text";
    auto r = run(text); REQUIRE(has(r, "underpopulated_slide")); REQUIRE(r.slidesChecked == 1);
    pres.style.smallFontSize = 12; REQUIRE(fonts.load(pres.style));
    text.attributes["role"] = "small"; REQUIRE(has(run(text), "small_text"));
    pres.style = PresentationStyle::builtInThemes()[0]; REQUIRE(fonts.load(pres.style));
    text.attributes = {{"width", "40"}, {"height", "10"}, {"wrap", "false"}};
    r = run(text); REQUIRE(has(r, "text_overflow"));
    REQUIRE(find(r, "text_overflow").measurements.at("requiredWidth") > 40);
    text.attributes = {{"width", "200"}, {"height", "30"}};
    text.text = "Several words that wrap onto multiple lines in this narrow column.";
    REQUIRE(has(run(text), "text_overflow"));
    text.attributes = {{"width", "0"}}; REQUIRE(has(run(text), "zero_size"));
    LayoutNode horizontal; horizontal.kind = "stack"; horizontal.attributes["orientation"] = "horizontal";
    text.attributes = {{"width", "1400"}}; horizontal.children = {text};
    REQUIRE(has(run(horizontal), "outside_canvas"));
    text.attributes = {{"color", "#111111"}};
    LayoutNode border; border.kind = "border"; border.attributes["background"] = "#111111";
    border.children = {text}; REQUIRE(has(run(border), "low_contrast"));
    border.attributes["background"] = "#ffffff"; REQUIRE(!has(run(border), "low_contrast"));
    text.attributes = {{"width", "200"}, {"height", "80"}};
    border.attributes = {{"width", "100"}, {"height", "100"}};
    horizontal.children = {text}; border.children = {horizontal};
    REQUIRE(has(run(border), "clipped_by_parent"));
    LayoutNode code; code.kind = "code"; code.text = "a very long line of source code";
    code.attributes = {{"width", "20"}, {"height", "10"}};
    REQUIRE(has(run(code), "code_overflow"));
    LayoutNode icon; icon.kind = "icon"; icon.attributes["name"] = "not-a-real-icon";
    REQUIRE(has(run(icon), "unknown_icon"));
    LayoutNode stack; stack.kind = "stack";
    REQUIRE(has(run(stack), "empty_slide"));
    stack.attributes["width"] = "1400"; REQUIRE(has(run(stack), "layout_overflow"));
    text.text = " \n\t"; text.attributes.clear(); REQUIRE(has(run(text), "empty_slide"));

    char temp[] = "/tmp/presenter-check-XXXXXX";
    char* dir = mkdtemp(temp); REQUIRE(dir != nullptr);
    std::string path = std::string(dir) + "/image.bmp";
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 100, 100, 32, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface != nullptr); REQUIRE(SDL_SaveBMP(surface, path.c_str()) == 0); SDL_FreeSurface(surface);
    LayoutNode image; image.kind = "image";
    image.attributes = {{"src", path}, {"width", "1000"}, {"height", "600"}};
    r = run(image); REQUIRE(has(r, "image_upscaled"));
    REQUIRE(find(r, "image_upscaled").measurements.at("scalePercent") == 600);
    REQUIRE(find(r, "image_upscaled").measurements.at("displayWidth") == 600);
    REQUIRE(!has(r, "image_cropped")); REQUIRE(r.failsStrict());
    image.attributes["width"] = "10"; image.attributes["height"] = "10";
    r = run(image); REQUIRE(has(r, "image_downscaled")); REQUIRE(r.failsStrict());
    REQUIRE(find(r, "image_downscaled").measurements.at("scalePercent") == 10);
    for (int size : {11, 100, 199}) {
        image.attributes["width"] = image.attributes["height"] = std::to_string(size);
        r = run(image); REQUIRE(!has(r, "image_upscaled")); REQUIRE(!has(r, "image_downscaled"));
    }
    // A line plus a small image leaves a large trailing gap, even inside
    // a full-canvas panel. Empty containers do not mask the gap.
    text.text = "One line"; text.attributes.clear();
    image.attributes = {{"src", path}, {"width", "100"}, {"height", "100"}};
    stack.attributes.clear(); stack.children = {text, image};
    border.attributes = {{"background", "panel"}}; border.children = {stack};
    r = run(border);
    const auto& sparse = find(r, "underpopulated_slide");
    REQUIRE(sparse.severity == IssueSeverity::Warning);
    REQUIRE(sparse.element == "/slide[1]"); REQUIRE(r.failsStrict());
    REQUIRE(sparse.measurements.at("emptyBelowFraction") > 0.7);
    REQUIRE(!has(r, "empty_slide"));
    // Centered sparse content is deliberate whitespace, not a trailing gap.
    stack.attributes["verticalAlignment"] = "center";
    REQUIRE(!has(run(stack), "underpopulated_slide"));
    // A stretched text slot still contains only one line.
    text.attributes["height"] = "720";
    REQUIRE(has(run(text), "underpopulated_slide"));
    // Threshold is inclusive at 45% (324 px) of the 720 px canvas.
    image.attributes = {{"src", path}, {"width", "396"}, {"height", "396"}};
    REQUIRE(has(run(image), "underpopulated_slide"));
    image.attributes["width"] = image.attributes["height"] = "397";
    REQUIRE(!has(run(image), "underpopulated_slide"));
    // Contained images use the actual centered destination, not their slot.
    image.attributes = {{"src", path}, {"width", "100"}, {"height", "720"}};
    REQUIRE(!has(run(image), "underpopulated_slide"));
    stack.attributes.clear(); stack.children.clear();
    r = run(stack); REQUIRE(has(r, "empty_slide")); REQUIRE(!has(r, "underpopulated_slide"));
    image.attributes = {{"src", path}};
    image.attributes["width"] = image.attributes["height"] = "200";
    REQUIRE(has(run(image), "image_upscaled"));
    image.attributes["fit"] = "fill"; image.attributes["width"] = "100"; image.attributes["height"] = "10";
    r = run(image); REQUIRE(has(r, "image_cropped"));
    REQUIRE(std::abs(find(r, "image_cropped").measurements.at("croppedFraction") - 0.9) < 1e-6);
    image.attributes["src"] = path + "missing"; REQUIRE(has(run(image), "missing_image"));
    std::string corrupt = std::string(dir) + "/corrupt.bmp";
    { std::ofstream out(corrupt); out << "not an image"; }
    image.attributes["src"] = corrupt; REQUIRE(has(run(image), "unreadable_image"));
    image.attributes["src"] = "quote\"\\\n.bmp";
    r = run(image); REQUIRE(formatCheckReport(r, true).find("quote\\\"\\\\\\u000a.bmp") != std::string::npos);
    REQUIRE(formatCheckReport(r, false).find("Suggestion:") != std::string::npos);
    pres.slides.push_back(pres.slides.front());
    r = checkPresentation(pres, fonts, renderer, 2); REQUIRE(r.slidesChecked == 1);
    for (const auto& issue : r.issues) REQUIRE(issue.slide == 2);
    REQUIRE(checkPresentation(pres, fonts, renderer).slidesChecked == 2);
    auto p = placeImage(100, 100, {10, 20, 200, 100}, false);
    REQUIRE(p.destination.x == 60 && p.destination.y == 20 && p.destination.w == 100);
    p = placeImage(100, 100, {10, 20, 200, 100}, true);
    REQUIRE(p.source.h == 50 && p.source.y == 25 && p.destination.w == 200);
    REQUIRE(placeImage(0, 100, {0, 0, 100, 100}, true).source.w == 0);
    REQUIRE(placeImage(100, 100, {0, 0, 0, 100}, false).destination.w == 0);
    REQUIRE(placeImage(1, 10000, {0, 0, 10000, 1}, true).source.h == 1);
    std::filesystem::remove(path); std::filesystem::remove(corrupt); std::filesystem::remove(dir);
    SDL_Quit(); std::printf("Checker: %d checks passed\n", checks);
}
