#include "common.h"
#include "constants.h"
#include "parser.h"
#include "font.h"
#include "renderer.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static void printUsage(const char* prog) {
    printf("Usage: %s <presentation.xml> [--style <style.xml>]\n", prog);
}

using ThemeIterator = std::vector<PresentationStyle>::const_iterator;

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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string xmlPath = argv[1];
    std::string stylePath;

    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--style" && i + 1 < argc) {
            stylePath = argv[++i];
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

    const auto& themes = PresentationStyle::builtInThemes();
    auto currentTheme = themes.begin();
    for (auto it = themes.begin(); it != themes.end(); ++it) {
        if (it->name == pres.style.name) { currentTheme = it; break; }
    }

    printf("Loaded %d slides from %s\n", pres.size(), xmlPath.c_str());

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    FontSet fonts;
    if (!fonts.load(pres.style)) {
        fprintf(stderr, "Failed to load fonts\n");
        SDL_Quit();
        return 1;
    }
    printf("Loaded fonts (title:%.0f subtitle:%.0f content:%.0f small:%.0f childTitle:%.0f)\n",
           pres.style.titleFontSize, pres.style.subtitleFontSize,
           pres.style.contentFontSize, pres.style.smallFontSize,
           pres.style.childTitleFontSize);

    // Audience window
    SDL_Window* audienceWindow = SDL_CreateWindow(
        "Presentation",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!audienceWindow) {
        fprintf(stderr, "Failed to create audience window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* audienceRenderer = SDL_CreateRenderer(audienceWindow, -1,
        SDL_RENDERER_SOFTWARE);
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
    SDL_Renderer* presenterRenderer = SDL_CreateRenderer(presenterWindow, -1,
        SDL_RENDERER_SOFTWARE);
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
    bool needsRender = true;
    bool audienceFullscreen = false;

    printf("Controls: Right/Space/Enter = next, Left/Backspace = prev, Home/End = first/last, F5 = fullscreen toggle, Escape = quit\n");
    printf("          Shift+Left/Right = switch theme\n");
    printf("Theme: %s (%d/%zu)\n", pres.style.name.c_str(),
           static_cast<int>(currentTheme - themes.begin()) + 1, themes.size());

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_RIGHT:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        currentTheme = switchTheme(pres, fonts, currentTheme, +1);
                        needsRender = true;
                    } else if (pres.canGoNext()) {
                        pres.next();
                        needsRender = true;
                        printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    }
                    break;
                case SDLK_SPACE:
                case SDLK_RETURN:
                    if (pres.canGoNext()) {
                        pres.next();
                        needsRender = true;
                        printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    }
                    break;
                case SDLK_LEFT:
                    if (event.key.keysym.mod & KMOD_SHIFT) {
                        currentTheme = switchTheme(pres, fonts, currentTheme, -1);
                        needsRender = true;
                    } else if (pres.canGoPrev()) {
                        pres.prev();
                        needsRender = true;
                        printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    }
                    break;
                case SDLK_BACKSPACE:
                    if (pres.canGoPrev()) {
                        pres.prev();
                        needsRender = true;
                        printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    }
                    break;
                case SDLK_HOME:
                    pres.first();
                    needsRender = true;
                    printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    break;
                case SDLK_END:
                    pres.last();
                    needsRender = true;
                    printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    break;
                case SDLK_F5:
                    audienceFullscreen = !audienceFullscreen;
                    if (audienceFullscreen) {
                        SDL_SetWindowFullscreen(audienceWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    } else {
                        SDL_SetWindowFullscreen(audienceWindow, 0);
                    }
                    needsRender = true;
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
                    } else {
                        SDL_GetWindowSize(presenterWindow, &pw, &ph);
                        presenterRend.init(presenterRenderer, pw, ph);
                    }
                    needsRender = true;
                }
                break;
            }
        }

        if (needsRender) {
            SDL_Texture* audienceTex = audienceRend.renderSlide(pres.currentSlide(), fonts, pres.style, pres.current + 1, pres.size());
            SDL_Texture* presenterTex = presenterRend.renderPresenterView(pres, fonts);

            // Render audience
            SDL_RenderClear(audienceRenderer);
            if (audienceTex) {
                SDL_RenderCopy(audienceRenderer, audienceTex, nullptr, nullptr);
                SDL_DestroyTexture(audienceTex);
            }
            SDL_RenderPresent(audienceRenderer);

            // Render presenter
            SDL_RenderClear(presenterRenderer);
            if (presenterTex) {
                SDL_RenderCopy(presenterRenderer, presenterTex, nullptr, nullptr);
                SDL_DestroyTexture(presenterTex);
            }
            SDL_RenderPresent(presenterRenderer);

            needsRender = false;
        }

        SDL_Delay(16); // ~60fps cap
    }

    audienceRend.cleanup();
    presenterRend.cleanup();
    SDL_DestroyRenderer(audienceRenderer);
    SDL_DestroyRenderer(presenterRenderer);
    SDL_DestroyWindow(audienceWindow);
    SDL_DestroyWindow(presenterWindow);
    SDL_Quit();

    return 0;
}
