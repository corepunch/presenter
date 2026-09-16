#include "renderer.h"
#include "layout.h"
#include "parser.h"
#include "charts.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>

static void testImageCorners(Renderer& renderer, const FontSet& fonts, PresentationStyle style) {
    char directory[] = "/tmp/presenter-image-corners-XXXXXX";
    assert(mkdtemp(directory));
    auto path = std::filesystem::path(directory) / "solid.bmp";
    SDL_Surface* source = SDL_CreateRGBSurfaceWithFormat(0, 24, 24, 32, SDL_PIXELFORMAT_RGBA32);
    assert(source);
    SDL_FillRect(source, nullptr, style.titleColor.toUint32(source->format));
    assert(SDL_SaveBMP(source, path.string().c_str()) == 0);
    SDL_FreeSurface(source);
    for (const char* fit : {"fit", "fill"}) for (int radius : {0, 12, 100}) {
        style.imageCornerRadius = radius;
        LayoutNode photo; photo.kind = "image"; photo.attributes = {{"src", path.string()}, {"fit", fit}};
        LayoutNode card; card.kind = "border";
        card.attributes = {{"width", "60"}, {"height", "40"}, {"background", "accent"}};
        card.children.push_back(photo);
        Slide slide; slide.elements.push_back(card);
        auto* texture = renderer.renderSlide(slide, fonts, style);
        assert(texture); SDL_DestroyTexture(texture);
        auto* surface = renderer.surface();
        auto* pixels = static_cast<Uint32*>(surface->pixels);
        auto pixel = [&](int x, int y) { return pixels[y * (surface->pitch / 4) + x]; };
        int left = std::string(fit) == "fit" ? 10 : 0;
        int right = std::string(fit) == "fit" ? 49 : 59;
        Uint32 corner = (radius == 0 ? style.titleColor : style.accentColor).toUint32(surface->format);
        assert(pixel(left, 0) == corner && pixel(right, 0) == corner);
        assert(pixel(left, 39) == corner && pixel(right, 39) == corner);
        assert(pixel(30, 20) == style.titleColor.toUint32(surface->format));
        assert(pixel(61, 20) == style.bgColor.toUint32(surface->format));
        if (radius == 12) {
            bool antialiased = false;
            for (int y = 0; y < 12; ++y) for (int x = left; x < left + 12; ++x) {
                auto p = pixel(x, y);
                antialiased |= p != style.titleColor.toUint32(surface->format) &&
                               p != style.accentColor.toUint32(surface->format);
            }
            assert(antialiased);
        }
    }
    std::filesystem::remove_all(directory);
}

int main() {
    assert(SDL_Init(0) == 0);
    SDL_Surface* target = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(target); assert(sdl);
    Renderer renderer; assert(renderer.init(sdl, 1280, 720));
    auto style = PresentationStyle::defaults();
    renderer.setStyle(&style);
    FontSet fonts; assert(fonts.load(style));
    ui::LayoutContext context{renderer};
    testImageCorners(renderer, fonts, style);
    renderer.setStyle(&style);
    ui::Grid grid; grid.columns = {{}, {}}; grid.rows = {{ui::Track::Unit::Auto, 0}};
    auto text = std::make_unique<ui::Text>("A long line with <b>bold words</b> that must wrap to the allocated column width.", fonts.variants(), style.textColor);
    text->wrap = true; auto* tp = text.get(); grid.add(std::move(text));
    grid.measure(context, {400, 720}); grid.arrange(context, {0, 0, 400, 720});
    int expected = renderer.wordWrap(tp->text, fonts.variants(), 200).size() * renderer.textHeight(fonts.get(FontType::Regular));
    assert(tp->bounds().width == 200 && tp->bounds().height == expected);
    ui::Text formatted("<b>Bold</b>", fonts.variants(), style.textColor);
    formatted.measure(context, {1000, 1000});
    assert(formatted.desiredSize().width < 150); // markup must not occupy space
    auto lines = renderer.wordWrap("<b>one two three four five six</b>", fonts.variants(), 100);
    assert(lines.size() > 1);
    for (const auto& line : lines) {
        assert(line.find("<b>") == 0 && line.substr(line.size() - 4) == "</b>");
        assert(renderer.formattedWidth(line, fonts.variants()) <= 100);
    }
    auto inlineCode = renderer.wordWrap("<code lang=\"cpp\">int x</code>", fonts.variants(), 1000);
    assert(inlineCode.size() == 1);
    assert(inlineCode[0].find("lang=\"cpp\"") != std::string::npos);
    ui::Text constrained("Text wraps at the effective maximum width.", fonts.variants(), style.textColor);
    constrained.wrap = true; constrained.width = 300; constrained.maxWidth = 160;
    constrained.measure(context, {1000, 1000}); constrained.arrange(context, {0, 0, 1000, 1000});
    assert(constrained.bounds().width == 160);
    assert(constrained.desiredSize().height == static_cast<int>(renderer.wordWrap(
        constrained.text, fonts.variants(), 160).size()) * renderer.textHeight(fonts.get(FontType::Regular)));

    // Glyphs and primitive drawing must respect nested clips.
    renderer.fillRect({0, 0, 1280, 720}, style.bgColor);
    SDL_Rect clip{40, 40, 35, 25}; SDL_SetClipRect(renderer.surface(), &clip);
    renderer.drawText("Overflow", 30, 70, fonts.get(FontType::Regular), style.textColor.toSDLColor());
    renderer.fillRect({20, 20, 100, 100}, style.accentColor, 12);
    Chart chart; chart.type = ChartType::Pie; chart.points = {{"A", 30}, {"B", 70}};
    renderChart(&renderer, renderer.surface(), chart, fonts, 0, 0, 400, 300);
    SDL_SetClipRect(renderer.surface(), nullptr);
    auto* pixels = static_cast<Uint32*>(renderer.surface()->pixels);
    for (int y = 0; y < 130; ++y) for (int x = 0; x < 130; ++x)
        if (x < 40 || x >= 75 || y < 40 || y >= 65)
            assert(pixels[y * (renderer.surface()->pitch / 4) + x] == style.bgColor.toUint32(renderer.surface()->format));

    for (const char* path : {"demo/Nature Portfolio.slides", "demo/Theme Studio.slides"}) {
        auto pres = parseXml(path); assert(!pres.empty());
        FontSet demoFonts; assert(demoFonts.load(pres.style));
        for (const auto& slide : pres.slides) {
            auto* texture = renderer.renderSlide(slide, demoFonts, pres.style);
            assert(renderer.layoutOverflowCount() == 0);
            assert(texture); SDL_DestroyTexture(texture);
        }
        auto* texture = renderer.renderPresenterView(pres, demoFonts);
        assert(texture); SDL_DestroyTexture(texture);
    }
    renderer.cleanup(); SDL_DestroyRenderer(sdl); SDL_FreeSurface(target); SDL_Quit();
    puts("render: wrapping, formatting, clipping, all demo slides and presenter view passed");
}
