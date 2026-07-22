#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "renderer.h"
#include <algorithm>
#include <cstring>

static inline Uint32 makeColor(SDL_Surface* surface, Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    return SDL_MapRGBA(surface->format, r, g, b, a);
}

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::init(SDL_Renderer* renderer, int width, int height) {
    m_renderer = renderer;
    m_width = width;
    m_height = height;
    m_surface = SDL_CreateRGBSurface(0, width, height, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    return m_surface != nullptr;
}

void Renderer::cleanup() {
    if (m_surface) {
        SDL_FreeSurface(m_surface);
        m_surface = nullptr;
    }
}

void Renderer::clear(int r, int g, int b) {
    SDL_FillRect(m_surface, nullptr, makeColor(m_surface, r, g, b));
}

void Renderer::drawText(const std::string& text, float x, float y,
                        const FontAtlas& font, SDL_Color color, FontType fontType) {
    if (text.empty() || !m_surface) return;

    const uint8_t* atlasBitmap = font.bitmapData();
    int atlasW = font.bitmapWidth();
    int atlasH = font.bitmapHeight();
    if (!atlasBitmap) return;

    SDL_Surface* atlasRGBA = SDL_CreateRGBSurface(0, atlasW, atlasH, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!atlasRGBA) return;

    SDL_LockSurface(atlasRGBA);
    auto* dst = static_cast<Uint32*>(atlasRGBA->pixels);
    for (int i = 0; i < atlasW * atlasH; i++) {
        Uint8 a = atlasBitmap[i];
        dst[i] = (a << 24) | (0x00FFFFFF);
    }
    SDL_UnlockSurface(atlasRGBA);

    SDL_SetSurfaceBlendMode(atlasRGBA, SDL_BLENDMODE_BLEND);
    SDL_SetSurfaceColorMod(atlasRGBA, color.r, color.g, color.b);

    float curX = x;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];

        const GlyphInfo& g = font.getGlyph(c);

        if (c != ' ') {
            SDL_Rect srcRect = {
                static_cast<int>(g.x0 * atlasW),
                static_cast<int>(g.y0 * atlasH),
                g.width,
                g.height
            };
            SDL_Rect dstRect = {
                static_cast<int>(curX + g.xoffset),
                static_cast<int>(y + g.yoffset),
                g.width,
                g.height
            };
            SDL_BlitSurface(atlasRGBA, &srcRect, m_surface, &dstRect);
            if (fontType == FontType::Bold || fontType == FontType::BoldItalic) {
                SDL_Rect dstBold = {dstRect.x + 1, dstRect.y, g.width, g.height};
                SDL_BlitSurface(atlasRGBA, &srcRect, m_surface, &dstBold);
            }
        }

        curX += g.xadvance;
        if (i > 0) {
            curX += font.getKerning(text[i - 1], c);
        }
    }

    SDL_FreeSurface(atlasRGBA);
}

void Renderer::renderFormatted(const std::string& text, float x, float y,
                                const FontAtlas& font, SDL_Color color) {
    SDL_Color codeColor = {255, 255, 0, 255};
    float curX = x;
    FontType fontType = FontType::Regular;
    bool code = false;
    size_t pos = 0;

    while (pos < text.size()) {
        size_t boldMarker = text.find("**", pos);
        size_t codeMarker = text.find('`', pos);

        // pick the nearest marker
        size_t marker = std::string::npos;
        size_t markerLen = 0;
        int markerType = 0; // 1=bold, 2=code

        if (boldMarker != std::string::npos && (marker == std::string::npos || boldMarker < marker)) {
            marker = boldMarker;
            markerLen = 2;
            markerType = 1;
        }
        if (codeMarker != std::string::npos && (marker == std::string::npos || codeMarker < marker)) {
            marker = codeMarker;
            markerLen = 1;
            markerType = 2;
        }

        if (marker == std::string::npos) {
            // no more markers, render remaining text
            std::string segment = text.substr(pos);
            SDL_Color clr = code ? codeColor : color;
            drawText(segment, curX, y, font, clr, fontType);
            break;
        }

        // render text before marker
        if (marker > pos) {
            std::string segment = text.substr(pos, marker - pos);
            SDL_Color clr = code ? codeColor : color;
            drawText(segment, curX, y, font, clr, fontType);
            curX += font.measureString(segment);
        }

        // toggle state
        if (markerType == 1) {
            fontType = (fontType == FontType::Bold) ? FontType::Regular : FontType::Bold;
        } else {
            code = !code;
        }
        pos = marker + markerLen;
    }
}

int Renderer::textHeight(const FontAtlas& font) {
    return static_cast<int>(font.getFontSize()) + 6;
}

void Renderer::drawRectOutline(const SDL_Rect* rect, Uint32 color) {
    SDL_Rect r;
    r = SDL_Rect{rect->x, rect->y, rect->w, 1}; SDL_FillRect(m_surface, &r, color);
    r = SDL_Rect{rect->x, rect->y + rect->h - 1, rect->w, 1}; SDL_FillRect(m_surface, &r, color);
    r = SDL_Rect{rect->x, rect->y, 1, rect->h}; SDL_FillRect(m_surface, &r, color);
    r = SDL_Rect{rect->x + rect->w - 1, rect->y, 1, rect->h}; SDL_FillRect(m_surface, &r, color);
}

std::vector<std::string> Renderer::wordWrap(const std::string& text,
                                             const FontAtlas& font, int maxWidth) {
    std::vector<std::string> lines;
    if (text.empty() || maxWidth <= 0) {
        lines.push_back(text);
        return lines;
    }

    std::string line;
    float lineWidth = 0;
    std::string word;

    for (size_t i = 0; i <= text.size(); i++) {
        char c = (i < text.size()) ? text[i] : ' ';

        if (c == ' ' || c == '\n' || i == text.size()) {
            if (!word.empty()) {
                float wordWidth = font.measureString(word);
                float spaceW = line.empty() ? 0 : font.measureString(" ");

                if (lineWidth + spaceW + wordWidth > maxWidth && !line.empty()) {
                    lines.push_back(line);
                    line = word;
                    lineWidth = font.measureString(word);
                } else {
                    if (!line.empty()) line += " ";
                    line += word;
                    lineWidth += spaceW + wordWidth;
                }
                word.clear();
            }

            if (c == '\n') {
                lines.push_back(line);
                line.clear();
                lineWidth = 0;
            }
        } else {
            word += c;
        }
    }

    if (!line.empty()) {
        lines.push_back(line);
    }

    return lines;
}

void Renderer::renderTextBlock(const std::string& text, int x, int y,
                                const FontAtlas& font, SDL_Color color, int maxWidth) {
    if (maxWidth > 0) {
        auto lines = wordWrap(text, font, maxWidth);
        int lineH = textHeight(font);
        for (size_t i = 0; i < lines.size(); i++) {
            drawText(lines[i], static_cast<float>(x),
                     static_cast<float>(y + static_cast<int>(i) * lineH), font, color);
        }
    } else {
        drawText(text, static_cast<float>(x), static_cast<float>(y), font, color);
    }
}

void Renderer::renderFormattedBlock(const std::string& text, int x, int y,
                                     const FontAtlas& font, SDL_Color color, int maxWidth) {
    if (maxWidth > 0) {
        auto lines = wordWrap(text, font, maxWidth);
        int lineH = textHeight(font);
        for (size_t i = 0; i < lines.size(); i++) {
            renderFormatted(lines[i], static_cast<float>(x),
                           static_cast<float>(y + static_cast<int>(i) * lineH), font, color);
        }
    } else {
        renderFormatted(text, static_cast<float>(x), static_cast<float>(y), font, color);
    }
}

// ---- slide rendering: simple text layout ----

static void renderSlideSimple(Renderer* r, SDL_Surface* surf,
                               const Slide& slide, const FontSet& fonts,
                               int w, int h) {
    int margin = 40;
    const FontAtlas& font = fonts.get(FontType::Regular);
    int th = static_cast<int>(font.getFontSize()) + 6;
    int y = margin;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color ltgray = {200, 200, 210, 255};

    // title
    r->renderFormatted(slide.title, static_cast<float>(margin),
                static_cast<float>(y), font, white);
    y += th - 2;

    // underline (centered between title and content)
    Uint32 lineColor = makeColor(surf, 100, 100, 120);
    SDL_Rect lineRect = {margin, y, w - 2 * margin, 1};
    SDL_FillRect(surf, &lineRect, lineColor);
    y += th + 9;

    int contentW = w - 2 * margin;

    // subtitle (for title slides stored in imageAlt)
    if (!slide.imageAlt.empty() && slide.type == SlideType::Title) {
        r->renderFormattedBlock(slide.imageAlt, margin, y, font, ltgray, contentW);
        y += th + 8;
    }

    // bullets / text content
    for (size_t i = 0; i < slide.bullets.size(); i++) {
        std::string line = "\xE2\x80\xA2 " + slide.bullets[i];
        r->renderFormattedBlock(line, margin, y, font, ltgray, contentW);
        y += th;
    }

    // image
    if (slide.type == SlideType::Image && !slide.imagePath.empty()) {
        y += 8;
        int imgW = 0, imgH = 0, channels = 0;
        unsigned char* data = stbi_load(slide.imagePath.c_str(), &imgW, &imgH, &channels, 4);

        if (data && imgW > 0 && imgH > 0) {
            int areaH = h - y - margin;
            int areaW = w - 2 * margin;
            float scaleX = static_cast<float>(areaW) / static_cast<float>(imgW);
            float scaleY = static_cast<float>(areaH) / static_cast<float>(imgH);
            float scale = std::min(scaleX, scaleY);
            int dstW = static_cast<int>(imgW * scale);
            int dstH = static_cast<int>(imgH * scale);

            SDL_Surface* imgSurface = SDL_CreateRGBSurfaceFrom(
                data, imgW, imgH, 32, imgW * 4,
                0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

            if (imgSurface) {
                SDL_Rect dstRect = {margin, y, dstW, dstH};
                SDL_BlitScaled(imgSurface, nullptr, surf, &dstRect);
                SDL_FreeSurface(imgSurface);
            }
            stbi_image_free(data);
            y += dstH + 8;
        } else {
            Uint32 phColor = makeColor(surf, 50, 50, 60);
            SDL_Rect ph = {margin, y, contentW, 80};
            SDL_FillRect(surf, &ph, phColor);
            r->drawText("[ image not found ]", static_cast<float>(margin + 8),
                        static_cast<float>(y + 30), font, ltgray);
            y += 88;
        }
    }

    // two-column
    if (slide.type == SlideType::TwoColumn) {
        y += 8;
        int gap = w / 10;
        int colW = (w - 2 * margin - gap) / 2;

        // left column label
        SDL_Color colLabel = {180, 180, 200, 255};
        r->drawText("Old:", static_cast<float>(margin), static_cast<float>(y), font, colLabel);
        r->drawText("New:", static_cast<float>(margin + colW + gap), static_cast<float>(y), font, colLabel);
        y += th;

        // left content
        r->renderFormattedBlock(slide.leftContent, margin, y, font, ltgray, colW);

        // right content
        r->renderFormattedBlock(slide.rightContent, margin + colW + gap, y, font, ltgray, colW);
    }
}

SDL_Texture* Renderer::renderSlide(const Slide& slide, const FontSet& fonts) {
    clear();

    SDL_Renderer* savedR = m_renderer;
    SDL_Surface* savedS = m_surface;
    m_renderer = nullptr;

    SDL_Surface* surf = SDL_CreateRGBSurface(0, m_width, m_height, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (surf) {
        SDL_FillRect(surf, nullptr, makeColor(surf, 30, 30, 40));
        m_surface = surf;

        renderSlideSimple(this, surf, slide, fonts, m_width, m_height);

        SDL_Rect dst = {0, 0, m_width, m_height};
        SDL_BlitSurface(surf, nullptr, savedS, &dst);
        SDL_FreeSurface(surf);
    }

    m_renderer = savedR;
    m_surface = savedS;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, m_surface);
    return texture;
}

SDL_Texture* Renderer::renderPresenterView(const Presentation& pres, const FontSet& fonts) {
    clear(40, 40, 50);

    int margin = 20;
    const FontAtlas& font = fonts.get(FontType::Regular);
    const FontAtlas& smallFont = fonts.get(FontType::Regular);
    int th = textHeight(smallFont);
    int y = margin;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color ltgray = {200, 200, 210, 255};
    SDL_Color dim = {140, 140, 155, 255};

    // slide number
    char numBuf[64];
    snprintf(numBuf, sizeof(numBuf), "Slide %d / %d", pres.current + 1, pres.size());
    drawText(numBuf, static_cast<float>(margin), static_cast<float>(y), smallFont, dim);
    y += th + 4;

    // title (larger font)
    const Slide& current = pres.currentSlide();
    renderFormatted(current.title, static_cast<float>(margin), static_cast<float>(y), font, white);
    y += textHeight(font) + 12;

    // notes section
    int contentW = m_width - 2 * margin;
    if (!current.notes.empty()) {
        Uint32 notesBg = makeColor(m_surface, 35, 35, 45);
        int notesH = std::max(th * 3, m_height - y - margin - th * 2);
        SDL_Rect notesRect = {margin, y - 18, contentW, notesH};
        SDL_FillRect(m_surface, &notesRect, notesBg);
        drawRectOutline(&notesRect, makeColor(m_surface, 60, 60, 70));

        drawText("Notes:", static_cast<float>(margin + 6),
                 static_cast<float>(y + 4), smallFont, dim);
        renderFormattedBlock(current.notes, margin + 6, y + th + 4, smallFont, ltgray, contentW - 12);
        y += notesH + 12;
    }

    // next slide
    if (pres.canGoNext()) {
        const Slide& next = pres.slides[pres.current + 1];
        drawText("Next:", static_cast<float>(margin),
                 static_cast<float>(y), smallFont, dim);
        drawText(next.title, static_cast<float>(margin),
                 static_cast<float>(y + th + 2), smallFont, ltgray);
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, m_surface);
    return texture;
}
