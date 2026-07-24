#include "common.h"
#include "constants.h"
#include "parser.h"
#include "font.h"
#include "renderer.h"
#include "screenshot.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static void printUsage(const char* prog) {
    printf("Usage: %s <presentation.slides> [options]\n", prog);
    printf("  --style <style.style>             Override the presentation style\n");
    printf("  --slide <number>                  Select a 1-based slide for capture\n");
    printf("  --screenshot <output.png>         Save the audience view and exit\n");
    printf("  --presenter-screenshot <file.png> Save the presenter view and exit\n");
}

using ThemeIterator = std::vector<PresentationStyle>::const_iterator;

static bool optionValue(const std::string& arg, const char* name,
                        std::string* value) {
    std::string prefix = std::string(name) + "=";
    if (arg.rfind(prefix, 0) != 0) return false;
    *value = arg.substr(prefix.size());
    return true;
}

static bool saveRenderedView(const Presentation& pres, const FontSet& fonts,
                             bool presenterView, const std::string& path) {
    int width = presenterView ? 640 : 1280;
    int height = presenterView ? 480 : 720;
    SDL_Surface* target = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* sdlRenderer = target
        ? SDL_CreateSoftwareRenderer(target) : nullptr;
    if (!target || !sdlRenderer) {
        if (sdlRenderer) SDL_DestroyRenderer(sdlRenderer);
        if (target) SDL_FreeSurface(target);
        return false;
    }

    Renderer renderer;
    bool initialized = renderer.init(sdlRenderer, width, height);
    SDL_Texture* texture = nullptr;
    if (initialized) {
        texture = presenterView
            ? renderer.renderPresenterView(pres, fonts)
            : renderer.renderSlide(pres.currentSlide(), fonts, pres.style,
                                   pres.current + 1, pres.size());
    }
    bool saved = initialized && saveSurfacePng(renderer.surface(), path);
    if (texture) SDL_DestroyTexture(texture);
    renderer.cleanup();
    SDL_DestroyRenderer(sdlRenderer);
    SDL_FreeSurface(target);
    return saved;
}

static ThemeIterator switchTheme(Presentation& pres, FontSet& fonts,
                                  ThemeIterator current, int direction) {
    const auto& themes = PresentationStyle::builtInThemes();
    ThemeIterator next = current + direction;
    if (next >= themes.end()) next = themes.begin();
    if (next < themes.begin()) next = themes.end() - 1;
    pres.style = *next;
    fonts.load(pres.style);
    printf("Theme: %s\n", pres.style.name.c_str());
    return next;
}

static SDL_Renderer* createDisplayRenderer(SDL_Window* window) {
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    return renderer;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string xmlPath = argv[1];
    std::string stylePath;
    std::string screenshotPath;
    std::string presenterScreenshotPath;
    int selectedSlide = 1;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--style" && i + 1 < argc) {
            stylePath = argv[++i];
        } else if (optionValue(arg, "--style", &stylePath)) {
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (optionValue(arg, "--screenshot", &screenshotPath)) {
        } else if (arg == "--presenter-screenshot" && i + 1 < argc) {
            presenterScreenshotPath = argv[++i];
        } else if (optionValue(arg, "--presenter-screenshot",
                               &presenterScreenshotPath)) {
        } else if (arg == "--slide" && i + 1 < argc) {
            selectedSlide = std::atoi(argv[++i]);
        } else {
            std::string slideValue;
            if (optionValue(arg, "--slide", &slideValue)) {
                selectedSlide = std::atoi(slideValue.c_str());
            } else {
                fprintf(stderr, "Unknown or incomplete option: %s\n", arg.c_str());
                printUsage(argv[0]);
                return 1;
            }
        }
    }

    Presentation pres = parseXml(xmlPath);
    if (pres.empty()) {
        fprintf(stderr, "Error: no slides found in %s\n", xmlPath.c_str());
        return 1;
    }

    // Override style from CLI
    if (!stylePath.empty()) {
        pres.style = PresentationStyle::load(stylePath);
    }
    if (selectedSlide < 1 || selectedSlide > pres.size()) {
        fprintf(stderr, "Slide must be between 1 and %d\n", pres.size());
        return 1;
    }
    pres.current = selectedSlide - 1;

    const auto& themes = PresentationStyle::builtInThemes();
    auto currentTheme = themes.begin();
    for (auto it = themes.begin(); it != themes.end(); ++it) {
        if (it->name == pres.style.name) { currentTheme = it; break; }
    }

    printf("Loaded %d slides from %s\n", pres.size(), xmlPath.c_str());

    bool captureOnly = !screenshotPath.empty() ||
                       !presenterScreenshotPath.empty();
    if (SDL_Init(captureOnly ? 0 : SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    FontSet fonts;
    if (!fonts.load(pres.style)) {
        fprintf(stderr, "Failed to load fonts\n");
        SDL_Quit();
        return 1;
    }
    printf("Loaded fonts (title:%.0f subtitle:%.0f content:%.0f bullet:%.0f small:%.0f childTitle:%.0f)\n",
           pres.style.titleFontSize, pres.style.subtitleFontSize,
           pres.style.contentFontSize, pres.style.bulletFontSize,
           pres.style.smallFontSize, pres.style.childTitleFontSize);

    if (captureOnly) {
        bool ok = true;
        if (!screenshotPath.empty()) {
            ok = saveRenderedView(pres, fonts, false, screenshotPath) && ok;
            fprintf(ok ? stdout : stderr, "%s audience screenshot: %s\n",
                    ok ? "Saved" : "Failed to save", screenshotPath.c_str());
        }
        if (!presenterScreenshotPath.empty()) {
            bool presenterOk = saveRenderedView(
                pres, fonts, true, presenterScreenshotPath);
            ok = presenterOk && ok;
            fprintf(presenterOk ? stdout : stderr,
                    "%s presenter screenshot: %s\n",
                    presenterOk ? "Saved" : "Failed to save",
                    presenterScreenshotPath.c_str());
        }
        SDL_Quit();
        return ok ? 0 : 1;
    }

    // Audience window
    const char* audienceTitle = pres.name.empty()
        ? "Presentation" : pres.name.c_str();
    SDL_Window* audienceWindow = SDL_CreateWindow(
        audienceTitle,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!audienceWindow) {
        fprintf(stderr, "Failed to create audience window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* audienceRenderer = createDisplayRenderer(audienceWindow);
    if (!audienceRenderer) {
        fprintf(stderr, "Failed to create audience renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Presenter window
    SDL_Window* presenterWindow = SDL_CreateWindow(
        "Presenter View",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!presenterWindow) {
        fprintf(stderr, "Failed to create presenter window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* presenterRenderer = createDisplayRenderer(presenterWindow);
    if (!presenterRenderer) {
        fprintf(stderr, "Failed to create presenter renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Renderer audienceRend;
    Renderer presenterRend;

    int aw, ah, pw, ph;
    SDL_GetWindowSize(audienceWindow, &aw, &ah);
    SDL_GetWindowSize(presenterWindow, &pw, &ph);
    audienceRend.init(audienceRenderer, aw, ah);
    presenterRend.init(presenterRenderer, pw, ph);

    bool running = bool(true);
    bool audienceDirty = true;
    bool presenterDirty = true;
    bool presentAudience = false;
    bool presentPresenter = false;
    bool audienceFullscreen = false;
    bool captureAudience = false;
    bool capturePresenter = false;
    SDL_Texture* audienceTex = nullptr;
    SDL_Texture* presenterTex = nullptr;

    printf("Controls: Right/Space/Enter = next, Left/Backspace = prev, Home/End = first/last, F5 = fullscreen toggle, Escape = quit\n");
    printf("          Shift+Left/Right = switch theme, S = save slide, Shift+S = save presenter view\n");
    printf("Theme: %s (%d/%zu)\n", pres.style.name.c_str(),
           static_cast<int>(currentTheme - themes.begin()) + 1, themes.size());

    auto markBothDirty = [&] {
        audienceDirty = true;
        presenterDirty = true;
    };

    auto handleEvent = [&](const SDL_Event& event) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_RIGHT:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        currentTheme = switchTheme(pres, fonts, currentTheme, +1);
                        markBothDirty();
                    } else if (pres.canGoNext()) {
                        pres.next();
                        markBothDirty();
                    }
                    break;
                case SDLK_SPACE:
                case SDLK_RETURN:
                    if (pres.canGoNext()) {
                        pres.next();
                        markBothDirty();
                    }
                    break;
                case SDLK_LEFT:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        currentTheme = switchTheme(pres, fonts, currentTheme, -1);
                        markBothDirty();
                    } else if (pres.canGoPrev()) {
                        pres.prev();
                        markBothDirty();
                    }
                    break;
                case SDLK_BACKSPACE:
                    if (pres.canGoPrev()) {
                        pres.prev();
                        markBothDirty();
                    }
                    break;
                case SDLK_HOME:
                    pres.first();
                    markBothDirty();
                    printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    break;
                case SDLK_END:
                    pres.last();
                    markBothDirty();
                    printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    break;
                case SDLK_F5:
                    audienceFullscreen = !audienceFullscreen;
                    if (audienceFullscreen) {
                        SDL_SetWindowFullscreen(audienceWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    } else {
                        SDL_SetWindowFullscreen(audienceWindow, 0);
                    }
                    audienceDirty = true;
                    break;
                case SDLK_s:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        capturePresenter = true;
                    } else {
                        captureAudience = true;
                    }
                    break;
                case SDLK_ESCAPE:
                    running = false;
                    break;
                default:
                    break;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    if (event.window.windowID == SDL_GetWindowID(audienceWindow)) {
                        SDL_GetWindowSize(audienceWindow, &aw, &ah);
                        audienceRend.init(audienceRenderer, aw, ah);
                        audienceDirty = true;
                    } else if (event.window.windowID ==
                               SDL_GetWindowID(presenterWindow)) {
                        SDL_GetWindowSize(presenterWindow, &pw, &ph);
                        presenterRend.init(presenterRenderer, pw, ph);
                        presenterDirty = true;
                    }
                } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    if (event.window.windowID == SDL_GetWindowID(audienceWindow))
                        presentAudience = true;
                    else if (event.window.windowID ==
                             SDL_GetWindowID(presenterWindow))
                        presentPresenter = true;
                }
                break;
        }
    };

    while (running) {
        if (audienceDirty) {
            if (audienceTex) SDL_DestroyTexture(audienceTex);
            audienceTex = audienceRend.renderSlide(
                pres.currentSlide(), fonts, pres.style,
                pres.current + 1, pres.size());
            audienceDirty = false;
            presentAudience = true;
        }
        if (presenterDirty) {
            if (presenterTex) SDL_DestroyTexture(presenterTex);
            presenterTex = presenterRend.renderPresenterView(pres, fonts);
            presenterDirty = false;
            presentPresenter = true;
        }

        if (presentAudience) {
            SDL_RenderClear(audienceRenderer);
            if (audienceTex)
                SDL_RenderCopy(audienceRenderer, audienceTex, nullptr, nullptr);
            SDL_RenderPresent(audienceRenderer);
            presentAudience = false;
        }

        if (presentPresenter) {
            SDL_RenderClear(presenterRenderer);
            if (presenterTex)
                SDL_RenderCopy(presenterRenderer, presenterTex, nullptr, nullptr);
            SDL_RenderPresent(presenterRenderer);
            presentPresenter = false;
        }

        if (captureAudience) {
            char path[64];
            snprintf(path, sizeof(path), "presenter-slide-%02d.png",
                     pres.current + 1);
            bool saved = saveSurfacePng(audienceRend.surface(), path);
            fprintf(saved ? stdout : stderr, "%s screenshot: %s\n",
                    saved ? "Saved" : "Failed to save", path);
            captureAudience = false;
        }
        if (capturePresenter) {
            char path[64];
            snprintf(path, sizeof(path), "presenter-notes-%02d.png",
                     pres.current + 1);
            bool saved = saveSurfacePng(presenterRend.surface(), path);
            fprintf(saved ? stdout : stderr, "%s screenshot: %s\n",
                    saved ? "Saved" : "Failed to save", path);
            capturePresenter = false;
        }

        SDL_Event event;
        if (!SDL_WaitEvent(&event)) {
            fprintf(stderr, "SDL_WaitEvent failed: %s\n", SDL_GetError());
            break;
        }
        handleEvent(event);
        while (SDL_PollEvent(&event)) handleEvent(event);
    }

    if (audienceTex) SDL_DestroyTexture(audienceTex);
    if (presenterTex) SDL_DestroyTexture(presenterTex);
    audienceRend.cleanup();
    presenterRend.cleanup();
    SDL_DestroyRenderer(audienceRenderer);
    SDL_DestroyRenderer(presenterRenderer);
    SDL_DestroyWindow(audienceWindow);
    SDL_DestroyWindow(presenterWindow);
    SDL_Quit();

    return 0;
}
