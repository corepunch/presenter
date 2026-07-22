#include "common.h"
#include "parser.h"
#include "font.h"
#include "renderer.h"

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static void printUsage(const char* prog) {
    printf("Usage: %s <presentation.md> [--font path.ttf] [--size N]\n", prog);
    printf("  --font   Path to TTF font (default: assets/Roboto-Regular.ttf)\n");
    printf("  --size   Font size in pixels (default: 28)\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mdPath = argv[1];
    std::string fontPath = "assets/Roboto-Regular.ttf";
    float fontSize = 28.0f;

    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--font" && i + 1 < argc) {
            fontPath = argv[++i];
        } else if (std::string(argv[i]) == "--size" && i + 1 < argc) {
            fontSize = static_cast<float>(std::atoi(argv[++i]));
        }
    }

    Presentation pres = parseMarkdown(mdPath);
    if (pres.empty()) {
        fprintf(stderr, "Error: no slides found in %s\n", mdPath.c_str());
        return 1;
    }
    printf("Loaded %d slides from %s\n", pres.size(), mdPath.c_str());

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    FontAtlas font;
    if (!font.load(fontPath, fontSize)) {
        fprintf(stderr, "Failed to load font: %s\n", fontPath.c_str());
        SDL_Quit();
        return 1;
    }
    printf("Loaded font %s at %.0fpx\n", fontPath.c_str(), fontSize);

    FontAtlas smallFont;
    float smallFontSize = fontSize * 0.7f;
    if (!smallFont.load(fontPath, smallFontSize)) {
        fprintf(stderr, "Failed to load small font: %s\n", fontPath.c_str());
        SDL_Quit();
        return 1;
    }
    printf("Loaded small font %s at %.0fpx\n", fontPath.c_str(), smallFontSize);

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
        960, 600,
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
                case SDLK_SPACE:
                case SDLK_RETURN:
                    if (pres.canGoNext()) {
                        pres.next();
                        needsRender = true;
                        printf("Slide %d/%d\n", pres.current + 1, pres.size());
                    }
                    break;
                case SDLK_LEFT:
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
            SDL_Texture* audienceTex = audienceRend.renderSlide(pres.currentSlide(), font);
            SDL_Texture* presenterTex = presenterRend.renderPresenterView(pres, font, smallFont);

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
