#include "presenter_api.h"
#include "constants.h"
#include "parser.h"
#include <unistd.h>
#include "font.h"
#include "renderer.h"
#include "style.h"
#include "screenshot.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

/* --------------------------------------------------------------------------
 * Internal session object
 * ----------------------------------------------------------------------- */

struct PresenterSession {
    Presentation pres;
    FontSet fonts;
    // theme index into PresentationStyle::builtInThemes()
    int themeIndex = 0;
    // cached label string — refreshed on slide change
    mutable char labelBuf[32];

    // Renderer instances reused across render calls to avoid repeated alloc
    Renderer pixelRend;   // software renderer for RGBA pixel-buffer output
};

static const char* fallbackPresentationTitle(const Presentation& pres) {
    return pres.name.empty() ? "Presentation" : pres.name.c_str();
}

static std::string dirOf(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) pos = path.rfind('\\');
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

/* --------------------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------------- */

PresenterHandle presenter_load(const char* slides_path, const char* cwd) {
    if (!slides_path) return nullptr;

    // SDL must be initialised before FontSet::load (stbtt is pure CPU, but
    // the Renderer::init path needs SDL_CreateRGBSurface).
    // We initialise with 0 flags so this is safe even without a display.
    static bool sdlInited = false;
    if (!sdlInited) {
        if (SDL_Init(0) < 0) {
            fprintf(stderr, "[presenter_api] SDL_Init failed: %s\n", SDL_GetError());
            return nullptr;
        }
        sdlInited = true;
    }

    Presentation pres = parseXml(slides_path);
    if (pres.empty()) {
        fprintf(stderr, "[presenter_api] No slides found in %s\n", slides_path);
        return nullptr;
    }

    auto* s = new PresenterSession();
    s->pres = std::move(pres);

    // Match the loaded style to a built-in theme index
    const auto& themes = PresentationStyle::builtInThemes();
    s->themeIndex = 0;
    for (int i = 0; i < (int)themes.size(); ++i) {
        if (themes[i].name == s->pres.style.name) { s->themeIndex = i; break; }
    }

    // Change working directory after parsing. Slide-relative paths are resolved
    // during parse; runtime assets such as fonts and syntax data use cwd.
    std::string workDir = cwd ? std::string(cwd) : dirOf(slides_path);
    if (!workDir.empty()) {
        if (chdir(workDir.c_str()) != 0)
            fprintf(stderr, "[presenter_api] chdir(%s) failed\n", workDir.c_str());
    }

    if (!s->fonts.load(s->pres.style)) {
        fprintf(stderr, "[presenter_api] Failed to load fonts\n");
        delete s;
        return nullptr;
    }

    return s;
}

void presenter_free(PresenterHandle h) {
    if (!h) return;
    h->pixelRend.cleanup();
    delete h;
}

/* --------------------------------------------------------------------------
 * Navigation
 * ----------------------------------------------------------------------- */

int presenter_count(PresenterHandle h) {
    return h ? h->pres.size() : 0;
}

int presenter_current(PresenterHandle h) {
    return h ? h->pres.current : 0;
}

int presenter_can_go_next(PresenterHandle h) {
    return h ? (int)h->pres.canGoNext() : 0;
}

int presenter_can_go_prev(PresenterHandle h) {
    return h ? (int)h->pres.canGoPrev() : 0;
}

void presenter_next(PresenterHandle h) {
    if (h) h->pres.next();
}

void presenter_prev(PresenterHandle h) {
    if (h) h->pres.prev();
}

void presenter_go_to(PresenterHandle h, int index) {
    if (!h) return;
    if (index >= 0 && index < h->pres.size())
        h->pres.current = index;
}

/* --------------------------------------------------------------------------
 * Metadata
 * ----------------------------------------------------------------------- */

const char* presenter_slide_label(PresenterHandle h) {
    if (!h) return "0 / 0";
    snprintf(h->labelBuf, sizeof(h->labelBuf),
             "%d / %d", h->pres.current + 1, h->pres.size());
    return h->labelBuf;
}

PresenterPresentationInfo presenter_presentation_info(PresenterHandle h) {
    if (!h) {
        return {"", 0, 0, "0 / 0", 0, 0};
    }

    return {
        fallbackPresentationTitle(h->pres),
        h->pres.size(),
        h->pres.current,
        presenter_slide_label(h),
        (int)h->pres.canGoNext(),
        (int)h->pres.canGoPrev()
    };
}

PresenterSlideData presenter_slide_info(PresenterHandle h, int index) {
    if (!h || index < 0 || index >= h->pres.size()) {
        return {"", ""};
    }

    const Slide& slide = h->pres.slides[index];
    return {slide.title.c_str(), slide.notes.c_str()};
}

PresenterSlideData presenter_current_slide_info(PresenterHandle h) {
    return h ? presenter_slide_info(h, h->pres.current) : PresenterSlideData{"", ""};
}

const char* presenter_title(PresenterHandle h) {
    return presenter_presentation_info(h).title;
}

const char* presenter_notes(PresenterHandle h) {
    return presenter_current_slide_info(h).notes;
}

const char* presenter_slide_title(PresenterHandle h, int index) {
    return presenter_slide_info(h, index).title;
}

/* --------------------------------------------------------------------------
 * Pixel-buffer rendering (no SDL window)
 * ----------------------------------------------------------------------- */

static int renderToPixels(PresenterSession* s, bool presenterView,
                          uint8_t* pixels, int width, int height, int stride) {
    if (width <= 0 || height <= 0 || stride < width * 4) return 0;

    const int renderWidth = presenterView ? width : SLIDE_CANVAS_WIDTH;
    const int renderHeight = presenterView ? height : SLIDE_CANVAS_HEIGHT;
    SDL_Surface* target = SDL_CreateRGBSurfaceWithFormat(
        0, renderWidth, renderHeight, 32, SDL_PIXELFORMAT_RGBA32);
    if (!target) return 0;

    SDL_Renderer* sdlRenderer = SDL_CreateSoftwareRenderer(target);
    if (!sdlRenderer) { SDL_FreeSurface(target); return 0; }

    Renderer r;
    if (!r.init(sdlRenderer, renderWidth, renderHeight)) {
        SDL_DestroyRenderer(sdlRenderer);
        SDL_FreeSurface(target);
        return 0;
    }

    SDL_Texture* tex = presenterView
        ? r.renderPresenterView(s->pres, s->fonts)
        : r.renderSlide(s->pres.currentSlide(), s->fonts, s->pres.style,
                        s->pres.current + 1, s->pres.size());

    int ok = 0;
    SDL_Surface* rendered = r.surface();
    if (tex && rendered) {
        SDL_Surface* output = SDL_CreateRGBSurfaceWithFormatFrom(
            pixels, width, height, 32, stride, SDL_PIXELFORMAT_RGBA32);
        if (output) {
            ok = SDL_BlitScaled(rendered, nullptr, output, nullptr) == 0;
            SDL_FreeSurface(output);
        }
        SDL_DestroyTexture(tex);
    }

    r.cleanup();
    SDL_DestroyRenderer(sdlRenderer);
    SDL_FreeSurface(target);
    return ok;
}

int presenter_render_slide_rgba(PresenterHandle h,
                                uint8_t* pixels, int width, int height,
                                int stride) {
    if (!h || !pixels) return 0;
    return renderToPixels(h, false, pixels, width, height, stride);
}

int presenter_render_presenter_rgba(PresenterHandle h,
                                    uint8_t* pixels, int width, int height,
                                    int stride) {
    if (!h || !pixels) return 0;
    return renderToPixels(h, true, pixels, width, height, stride);
}

/* --------------------------------------------------------------------------
 * SDL2-backed rendering (audience window in standalone app or macOS app)
 * ----------------------------------------------------------------------- */

int presenter_render_slide_sdl(PresenterHandle h,
                               void* sdl_renderer, int w, int h_px) {
    if (!h || !sdl_renderer || w <= 0 || h_px <= 0) return 0;
    Renderer r;
    if (!r.init(static_cast<SDL_Renderer*>(sdl_renderer),
                SLIDE_CANVAS_WIDTH, SLIDE_CANVAS_HEIGHT)) return 0;
    SDL_Texture* tex = r.renderSlide(h->pres.currentSlide(), h->fonts,
                                     h->pres.style,
                                     h->pres.current + 1, h->pres.size());
    int ok = 0;
    if (tex) {
        auto* output = static_cast<SDL_Renderer*>(sdl_renderer);
        SDL_RenderClear(output);
        const float scale = std::min(
            static_cast<float>(w) / SLIDE_CANVAS_WIDTH,
            static_cast<float>(h_px) / SLIDE_CANVAS_HEIGHT);
        SDL_Rect destination{
            static_cast<int>((w - SLIDE_CANVAS_WIDTH * scale) / 2.0f),
            static_cast<int>((h_px - SLIDE_CANVAS_HEIGHT * scale) / 2.0f),
            static_cast<int>(SLIDE_CANVAS_WIDTH * scale),
            static_cast<int>(SLIDE_CANVAS_HEIGHT * scale)
        };
        SDL_RenderCopy(output, tex, nullptr, &destination);
        SDL_RenderPresent(output);
        SDL_DestroyTexture(tex);
        ok = 1;
    }
    r.cleanup();
    return ok;
}

int presenter_render_presenter_sdl(PresenterHandle h,
                                   void* sdl_renderer, int w, int h_px) {
    if (!h || !sdl_renderer) return 0;
    Renderer r;
    if (!r.init(static_cast<SDL_Renderer*>(sdl_renderer), w, h_px)) return 0;
    SDL_Texture* tex = r.renderPresenterView(h->pres, h->fonts);
    int ok = 0;
    if (tex) {
        SDL_RenderClear(static_cast<SDL_Renderer*>(sdl_renderer));
        SDL_RenderCopy(static_cast<SDL_Renderer*>(sdl_renderer), tex, nullptr, nullptr);
        SDL_RenderPresent(static_cast<SDL_Renderer*>(sdl_renderer));
        SDL_DestroyTexture(tex);
        ok = 1;
    }
    r.cleanup();
    return ok;
}

/* --------------------------------------------------------------------------
 * Theme cycling
 * ----------------------------------------------------------------------- */

int presenter_theme_count(PresenterHandle h) {
    if (!h) return 0;
    return (int)PresentationStyle::builtInThemes().size();
}

const char* presenter_theme_name(PresenterHandle h, int index) {
    if (!h) return "";
    const auto& themes = PresentationStyle::builtInThemes();
    if (index < 0 || index >= (int)themes.size()) return "";
    return themes[index].name.c_str();
}

int presenter_current_theme(PresenterHandle h) {
    return h ? h->themeIndex : 0;
}

void presenter_set_theme(PresenterHandle h, int index) {
    if (!h) return;
    const auto& themes = PresentationStyle::builtInThemes();
    if (index < 0 || index >= (int)themes.size()) return;
    h->themeIndex = index;
    h->pres.style = themes[index];
    h->fonts.load(h->pres.style);
}
