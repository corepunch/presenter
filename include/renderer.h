#pragma once
#include "common.h"
#include "font.h"
#include <SDL2/SDL.h>
#include <string>

// Inline formatting state tracked while scanning ** / _ / ` markers
struct TextFormat {
    bool bold = false;
    bool italic = false;
    bool code = false;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init(SDL_Renderer* renderer, int width, int height);
    void cleanup();

    SDL_Texture* renderSlide(const Slide& slide, const FontSet& fonts, const PresentationStyle& style, int slideNum = 1, int totalSlides = 1);
    SDL_Texture* renderPresenterView(const Presentation& pres, const FontSet& fonts);

    void clear(int r = 30, int g = 30, int b = 40);
    void drawText(const std::string& text, float x, float y, const Font& font, SDL_Color color);
    void renderFormatted(const std::string& text, float x, float y, const FontVariants& fonts, SDL_Color color);
    void renderFormattedBlock(const std::string& text, int x, int y,
                               const FontVariants& fonts, SDL_Color color, int maxWidth = 0,
                               const FontVariants* headingFonts = nullptr);
    int textHeight(const Font& font);
    void renderTextBlock(const std::string& text, int x, int y,
                         const Font& font, SDL_Color color, int maxWidth = 0);
    std::vector<std::string> wordWrap(const std::string& text, const FontVariants& fonts, int maxWidth);
    void drawRectOutline(const SDL_Rect* rect, Uint32 color);

    int width() const { return m_width; }
    int height() const { return m_height; }

    SDL_Surface* surface() const { return m_surface; }
    void setSurface(SDL_Surface* s) { m_surface = s; }
    SDL_Renderer* sdlRenderer() const { return m_renderer; }
    void setSdlRenderer(SDL_Renderer* r) { m_renderer = r; }
    const PresentationStyle& style() const { return *m_style; }
    void setStyle(const PresentationStyle* s) { m_style = s; }

private:

    SDL_Renderer* m_renderer = nullptr;
    SDL_Surface* m_surface = nullptr;
    const PresentationStyle* m_style = nullptr;
    int m_width = 0;
    int m_height = 0;
};
