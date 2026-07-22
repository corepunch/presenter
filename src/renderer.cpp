#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utf8.h"

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
                        const Font& font, SDL_Color color) {
    if (text.empty() || !m_surface) return;

    float curX = x;
    utf8_int32_t prev = 0;
    const utf8_int8_t* s = reinterpret_cast<const utf8_int8_t*>(text.c_str());
    while (*s) {
        utf8_int32_t cp = 0;
        s = utf8codepoint(s, &cp);

        if (prev) curX += font.getKerning(static_cast<uint32_t>(prev), static_cast<uint32_t>(cp));
        curX += font.drawGlyph(m_surface, static_cast<uint32_t>(cp), curX, y, color);

        prev = cp;
    }
}

// Pick the font matching the current inline-format state
static const Font& pickFont(const FontVariants& fonts, const TextFormat& fmt) {
    if (fmt.code)                 return fonts.get(FontType::Monospace);
    if (fmt.bold && fmt.italic)   return fonts.get(FontType::BoldItalic);
    if (fmt.bold)                 return fonts.get(FontType::Bold);
    if (fmt.italic)               return fonts.get(FontType::Italic);
    return fonts.get(FontType::Regular);
}

// Find the nearest format marker at or after pos.
// Returns marker position (or npos); *outLen = marker length, *outKind = 1=bold 2=italic 3=code.
static size_t findMarker(const std::string& text, size_t pos, size_t* outLen, int* outKind) {
    size_t boldMarker   = text.find("**", pos);
    size_t italicMarker = text.find('_', pos);
    size_t codeMarker   = text.find('`', pos);

    size_t marker = std::string::npos;
    size_t markerLen = 0;
    int kind = 0;

    if (boldMarker != std::string::npos) {
        marker = boldMarker; markerLen = 2; kind = 1;
    }
    if (italicMarker != std::string::npos && (marker == std::string::npos || italicMarker < marker)) {
        marker = italicMarker; markerLen = 1; kind = 2;
    }
    if (codeMarker != std::string::npos && (marker == std::string::npos || codeMarker < marker)) {
        marker = codeMarker; markerLen = 1; kind = 3;
    }

    *outLen = markerLen;
    *outKind = kind;
    return marker;
}

static void toggleFormat(TextFormat& fmt, int kind) {
    if (kind == 1)      fmt.bold = !fmt.bold;
    else if (kind == 2) fmt.italic = !fmt.italic;
    else                fmt.code = !fmt.code;
}

// Measure text with inline format markers applied. Updates fmt as it scans,
// so callers can keep one state across word/line boundaries.
static float measureFormatted(const FontVariants& fonts, const std::string& text, TextFormat& fmt) {
    float width = 0;
    size_t pos = 0;

    while (pos < text.size()) {
        size_t markerLen = 0;
        int kind = 0;
        size_t marker = findMarker(text, pos, &markerLen, &kind);

        if (marker == std::string::npos) {
            width += pickFont(fonts, fmt).measureString(text.substr(pos));
            break;
        }
        if (marker > pos) {
            width += pickFont(fonts, fmt).measureString(text.substr(pos, marker - pos));
        }
        toggleFormat(fmt, kind);
        pos = marker + markerLen;
    }
    return width;
}

void Renderer::renderFormatted(const std::string& text, float x, float y,
                                const FontVariants& fonts, SDL_Color color) {
    SDL_Color codeColor = {255, 255, 0, 255};
    float curX = x;
    TextFormat fmt;
    size_t pos = 0;

    while (pos < text.size()) {
        size_t markerLen = 0;
        int kind = 0;
        size_t marker = findMarker(text, pos, &markerLen, &kind);

        if (marker == std::string::npos) {
            std::string segment = text.substr(pos);
            SDL_Color clr = fmt.code ? codeColor : color;
            drawText(segment, curX, y, pickFont(fonts, fmt), clr);
            break;
        }

        if (marker > pos) {
            std::string segment = text.substr(pos, marker - pos);
            const Font& font = pickFont(fonts, fmt);
            SDL_Color clr = fmt.code ? codeColor : color;
            drawText(segment, curX, y, font, clr);
            curX += font.measureString(segment);
        }

        toggleFormat(fmt, kind);
        pos = marker + markerLen;
    }
}

int Renderer::textHeight(const Font& font) {
    return static_cast<int>(font.getFontSize()) + LINE_PADDING;
}

void Renderer::drawRectOutline(const SDL_Rect* rect, Uint32 color) {
    SDL_Rect r;
    r = SDL_Rect{rect->x, rect->y, rect->w, 1}; SDL_FillRect(m_surface, &r, color);
    r = SDL_Rect{rect->x, rect->y + rect->h - 1, rect->w, 1}; SDL_FillRect(m_surface, &r, color);
    r = SDL_Rect{rect->x, rect->y, 1, rect->h}; SDL_FillRect(m_surface, &r, color);
    r = SDL_Rect{rect->x + rect->w - 1, rect->y, 1, rect->h}; SDL_FillRect(m_surface, &r, color);
}

std::vector<std::string> Renderer::wordWrap(const std::string& text,
                                              const FontVariants& fonts, int maxWidth) {
    std::vector<std::string> lines;
    if (text.empty() || maxWidth <= 0) {
        lines.push_back(text);
        return lines;
    }

    TextFormat fmt; // persists across words so ** multi-word spans ** measure right
    std::string line;
    float lineWidth = 0;
    std::string word;

    for (size_t i = 0; i <= text.size(); i++) {
        char c = (i < text.size()) ? text[i] : ' ';

        if (c == ' ' || c == '\n' || i == text.size()) {
            if (!word.empty()) {
                float wordWidth = measureFormatted(fonts, word, fmt);
                TextFormat fmtCopy = fmt;
                float spaceW = line.empty() ? 0 : measureFormatted(fonts, " ", fmtCopy);

                if (lineWidth + spaceW + wordWidth > maxWidth && !line.empty()) {
                    lines.push_back(line);
                    line = word;
                    lineWidth = wordWidth;
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
                                const Font& font, SDL_Color color, int maxWidth) {
    if (maxWidth > 0) {
        FontVariants v = { &font, &font, &font, &font, &font };
        auto lines = wordWrap(text, v, maxWidth);
        int lineH = textHeight(font);
        for (size_t i = 0; i < lines.size(); i++) {
            drawText(lines[i], static_cast<float>(x),
                     static_cast<float>(y + static_cast<int>(i) * lineH), font, color);
        }
    } else {
        drawText(text, static_cast<float>(x), static_cast<float>(y), font, color);
    }
}

// Returns true if the raw line is a markdown heading (# .. ###### followed by a space)
static bool isHeadingLine(const std::string& raw, std::string* outText) {
    size_t hashes = raw.find_first_not_of('#');
    if (hashes == 0 || hashes == std::string::npos || hashes > 6) return false;
    if (raw[hashes] != ' ') return false;
    *outText = raw.substr(hashes + 1);
    return true;
}

void Renderer::renderFormattedBlock(const std::string& text, int x, int y,
                                     const FontVariants& fonts, SDL_Color color, int maxWidth,
                                     const FontVariants* headingFonts) {
    int lineH = textHeight(fonts.get(FontType::Regular));
    int curY = y;

    // Process one raw line at a time so markdown headings render correctly
    size_t start = 0;
    while (true) {
        size_t nl = text.find('\n', start);
        std::string raw = (nl == std::string::npos)
            ? text.substr(start)
            : text.substr(start, nl - start);

        std::string heading;
        if (headingFonts && isHeadingLine(raw, &heading)) {
            renderFormatted(heading, static_cast<float>(x), static_cast<float>(curY), *headingFonts, color);
            curY += textHeight(headingFonts->get(FontType::Regular));
        } else if (maxWidth > 0) {
            auto lines = wordWrap(raw, fonts, maxWidth);
            for (const auto& l : lines) {
                renderFormatted(l, static_cast<float>(x), static_cast<float>(curY), fonts, color);
                curY += lineH;
            }
        } else {
            renderFormatted(raw, static_cast<float>(x), static_cast<float>(curY), fonts, color);
            curY += lineH;
        }

        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

// ---- slide rendering: simple text layout ----

static void renderSlideSimple(Renderer* r, SDL_Surface* surf,
                               const Slide& slide, const FontSet& fonts,
                               int w, int h) {
    int margin = SLIDE_MARGIN;
    FontVariants baseV = fonts.variants();
    FontVariants titleV = fonts.titleVariants();
    const Font& baseFont = baseV.get(FontType::Regular);
    int th = static_cast<int>(baseFont.getFontSize()) + LINE_PADDING;
    int thTitle = static_cast<int>(titleV.get(FontType::Regular).getFontSize()) + LINE_PADDING;
    int y = margin;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color ltgray = {200, 200, 210, 255};

    // title at 2x size
    r->renderFormatted(slide.title, static_cast<float>(margin),
                static_cast<float>(y+25), titleV, white);
    y += thTitle - 2;

    // underline (centered between title and content)
    Uint32 lineColor = makeColor(surf, 100, 100, 120);
    SDL_Rect lineRect = {margin, y, w - 2 * margin, 1};
    SDL_FillRect(surf, &lineRect, lineColor);
    y += th + 9;

    int contentW = w - 2 * margin;

    // subtitle (for title slides stored in imageAlt)
    if (!slide.imageAlt.empty() && slide.type == SlideType::Title) {
        r->renderFormattedBlock(slide.imageAlt, margin, y + 10, baseV, ltgray, contentW);
        y += th + 8;
    }

    // bullets / text content
    for (size_t i = 0; i < slide.bullets.size(); i++) {
        std::string heading;
        if (isHeadingLine(slide.bullets[i], &heading)) {
            // ## / ### sub-heading: render at title size, no bullet marker
            r->renderFormatted(heading, static_cast<float>(margin),
                               static_cast<float>(y), titleV, white);
            y += thTitle;
        } else {
            std::string line = "\xE2\x80\xA2 " + slide.bullets[i];
            r->renderFormattedBlock(line, margin, y, baseV, ltgray, contentW);
            y += th;
        }
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
                        static_cast<float>(y + 30), baseFont, ltgray);
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
        r->drawText("Old:", static_cast<float>(margin), static_cast<float>(y), baseFont, colLabel);
        r->drawText("New:", static_cast<float>(margin + colW + gap), static_cast<float>(y), baseFont, colLabel);
        y += th;

        // left content
        r->renderFormattedBlock(slide.leftContent, margin, y, baseV, ltgray, colW, &titleV);

        // right content
        r->renderFormattedBlock(slide.rightContent, margin + colW + gap, y, baseV, ltgray, colW, &titleV);
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

    int margin = PRESENTER_MARGIN;
    FontVariants baseV = fonts.variants();
    FontVariants smallV = fonts.smallVariants();
    const Font& baseFont = baseV.get(FontType::Regular);
    const Font& smallFont = smallV.get(FontType::Regular);
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

    // title (base size)
    const Slide& current = pres.currentSlide();
    renderFormatted(current.title, static_cast<float>(margin), static_cast<float>(y), baseV, white);
    y += textHeight(baseFont) + 12;

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
        renderFormattedBlock(current.notes, margin + 6, y + th + 4, smallV, ltgray, contentW - 12);
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
