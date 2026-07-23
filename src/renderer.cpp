#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utf8.h"

#include "renderer.h"
#include "layout.h"
#include "image.h"
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

// ---- slide rendering: part-based layout ----

static void renderPartHeader(Renderer* r, SDL_Surface* surf,
                             const Slide& slide, const SlidePart& part,
                             const FontSet& fonts, int slideW) {
    FontVariants titleV = fonts.titleVariants();
    const Font& titleFont = titleV.get(FontType::Regular);
    SDL_Color white = {255, 255, 255, 255};

    // Vertically center title: place baseline so the glyph cell (ascent..descent) is centered
    float ascent = titleFont.getAscent();
    float descent = titleFont.getDescent();
    float titleLineH = ascent - descent;
    int textY = part.rect.y + (part.rect.h - static_cast<int>(titleLineH)) / 2 + static_cast<int>(ascent);

    r->renderFormatted(slide.title, static_cast<float>(part.rect.x + PART_PADDING),
                       static_cast<float>(textY), titleV, white);

    // Draw horizontal rule at the bottom edge of the header
    Uint32 lineColor = makeColor(surf, 100, 100, 120);
    SDL_Rect lineRect = {part.rect.x, part.rect.y + part.rect.h - 1, part.rect.w, 1};
    SDL_FillRect(surf, &lineRect, lineColor);
}

static void renderPartBody(Renderer* r, SDL_Surface* surf,
                           const Slide& slide, const SlidePart& part,
                           const FontSet& fonts) {
    FontVariants baseV = fonts.variants();
    FontVariants titleV = fonts.titleVariants();
    SDL_Color ltgray = {200, 200, 210, 255};
    SDL_Color white = {255, 255, 255, 255};
    int contentW = part.rect.w - 2 * PART_PADDING;
    int y = part.rect.y + PART_PADDING;

    for (size_t i = 0; i < slide.bullets.size(); i++) {
        std::string heading;
        if (isHeadingLine(slide.bullets[i], &heading)) {
            r->renderFormatted(heading, static_cast<float>(part.rect.x + PART_PADDING),
                               static_cast<float>(y), titleV, white);
            y += r->textHeight(titleV.get(FontType::Regular));
        } else {
            std::string line = "\xE2\x80\xA2 " + slide.bullets[i];
            r->renderFormattedBlock(line, part.rect.x + PART_PADDING, y, baseV, ltgray, contentW);
            y += r->textHeight(baseV.get(FontType::Regular));
        }
    }
}

static void renderPartColumnLeft(Renderer* r, SDL_Surface* surf,
                                 const Slide& slide, const SlidePart& part,
                                 const FontSet& fonts) {
    FontVariants baseV = fonts.variants();
    FontVariants titleV = fonts.titleVariants();
    SDL_Color ltgray = {200, 200, 210, 255};
    int contentW = part.rect.w - 2 * PART_PADDING;
    std::string content = slide.blocks.size() > 0 ? slide.blocks[0] : "";
    r->renderFormattedBlock(content, part.rect.x + PART_PADDING, part.rect.y + PART_PADDING,
                            baseV, ltgray, contentW, &titleV);
}

static void renderPartColumnRight(Renderer* r, SDL_Surface* surf,
                                  const Slide& slide, const SlidePart& part,
                                  const FontSet& fonts) {
    FontVariants baseV = fonts.variants();
    FontVariants titleV = fonts.titleVariants();
    SDL_Color ltgray = {200, 200, 210, 255};
    int contentW = part.rect.w - 2 * PART_PADDING;
    std::string content = slide.blocks.size() > 1 ? slide.blocks[1] : "";
    r->renderFormattedBlock(content, part.rect.x + PART_PADDING, part.rect.y + PART_PADDING,
                            baseV, ltgray, contentW, &titleV);
}

static void renderPartImage(Renderer* r, SDL_Surface* surf,
                            const Slide& slide, const SlidePart& part,
                            const FontSet& fonts, int slideH) {
    if (slide.imagePath.empty()) return;

    int imgW = 0, imgH = 0, channels = 0;
    unsigned char* data = stbi_load(slide.imagePath.c_str(), &imgW, &imgH, &channels, 4);

    if (data && imgW > 0 && imgH > 0) {
        float scaleX = static_cast<float>(part.rect.w) / static_cast<float>(imgW);
        float scaleY = static_cast<float>(part.rect.h) / static_cast<float>(imgH);

        int dstW, dstH, dstX, dstY;
        ImageBuf srcBuf;

        if (slide.imageFit == ImageFit::Fill) {
            float scale = std::max(scaleX, scaleY);
            // Center-crop the source to the visible region before resampling
            int cropW = static_cast<int>(part.rect.w / scale);
            int cropH = static_cast<int>(part.rect.h / scale);
            int cropX = std::max(0, (imgW - cropW) / 2);
            int cropY = std::max(0, (imgH - cropH) / 2);
            cropW = std::min(cropW, imgW - cropX);
            cropH = std::min(cropH, imgH - cropY);

            // Build a contiguous buffer for the cropped region
            srcBuf.w = cropW;
            srcBuf.h = cropH;
            srcBuf.data = new uint8_t[cropW * cropH * 4];
            for (int row = 0; row < cropH; ++row)
                std::memcpy(srcBuf.data + row * cropW * 4,
                            data + ((cropY + row) * imgW + cropX) * 4,
                            cropW * 4);

            dstW = part.rect.w;
            dstH = part.rect.h;
            dstX = part.rect.x;
            dstY = part.rect.y;
        } else {
            float scale = std::min(scaleX, scaleY);
            dstW = static_cast<int>(imgW * scale);
            dstH = static_cast<int>(imgH * scale);
            dstX = part.rect.x + (part.rect.w - dstW) / 2;
            dstY = part.rect.y + (part.rect.h - dstH) / 2;

            srcBuf.w = imgW;
            srcBuf.h = imgH;
            srcBuf.data = data; // borrow — do not delete[]
        }

        ImageBuf resampled = resampleBilinear(srcBuf, dstW, dstH);

        if (slide.imageFit == ImageFit::Fill)
            delete[] srcBuf.data;
        // (Fit path borrows data — not freed here)

        if (resampled.data) {
            SDL_Surface* imgSurface = SDL_CreateRGBSurfaceFrom(
                resampled.data, dstW, dstH, 32, dstW * 4,
                0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
            if (imgSurface) {
                SDL_Rect dstRect = {dstX, dstY, dstW, dstH};
                SDL_BlitSurface(imgSurface, nullptr, surf, &dstRect);
                SDL_FreeSurface(imgSurface);
            }
            delete[] resampled.data;
        }

        stbi_image_free(data);
    } else {
        // Placeholder
        Uint32 phColor = makeColor(surf, 50, 50, 60);
        SDL_Rect ph = {part.rect.x, part.rect.y, part.rect.w, part.rect.h};
        SDL_FillRect(surf, &ph, phColor);
        const Font& baseFont = fonts.variants().get(FontType::Regular);
        SDL_Color ltgray = {200, 200, 210, 255};
        r->drawText("[ image not found ]", static_cast<float>(part.rect.x + 8),
                    static_cast<float>(part.rect.y + part.rect.h / 2 - 10), baseFont, ltgray);
    }
}

static void renderPartCaption(Renderer* r, SDL_Surface* surf,
                              const Slide& slide, const SlidePart& part,
                              const FontSet& fonts) {
    if (slide.bullets.empty()) return;
    FontVariants baseV = fonts.variants();
    SDL_Color ltgray = {200, 200, 210, 255};
    int contentW = part.rect.w - 2 * PART_PADDING;
    // Join all bullet lines as caption
    std::string caption;
    for (const auto& b : slide.bullets) {
        if (!caption.empty()) caption += " ";
        caption += b;
    }
    r->renderFormattedBlock(caption, part.rect.x + PART_PADDING, part.rect.y + PART_PADDING,
                            baseV, ltgray, contentW);
}

static void renderPartFooter(Renderer* r, SDL_Surface* surf,
                             const Slide& slide, const SlidePart& part,
                             const FontSet& fonts, int slideNum, int totalSlides) {
    FontVariants smallV = fonts.smallVariants();
    const Font& smallFont = smallV.get(FontType::Regular);
    SDL_Color dim = {140, 140, 155, 255};

    char numBuf[64];
    snprintf(numBuf, sizeof(numBuf), "%d / %d", slideNum, totalSlides);
    // Right-align the slide number
    float numW = smallFont.measureString(numBuf);
    float x = static_cast<float>(part.rect.x + part.rect.w - PART_PADDING) - numW;
    float y = static_cast<float>(part.rect.y + (part.rect.h - r->textHeight(smallFont)) / 2);
    r->drawText(numBuf, x, y, smallFont, dim);
}

static void renderPartFullSlide(Renderer* r, SDL_Surface* surf,
                                const Slide& slide, const SlidePart& part,
                                const FontSet& fonts) {
    FontVariants titleV = fonts.titleVariants();
    FontVariants baseV = fonts.variants();
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color ltgray = {200, 200, 210, 255};

    const Font& titleFont = titleV.get(FontType::Regular);
    float ascent = titleFont.getAscent();
    float descent = titleFont.getDescent();
    float titleLineH = ascent - descent;

    // For Title: center title + subtitle vertically
    // For Section: center title only
    std::string subtitleText = slide.subtitle.empty() ? slide.imageAlt : slide.subtitle;

    float totalH = titleLineH;
    if (!subtitleText.empty()) {
        const Font& baseFont = baseV.get(FontType::Regular);
        float baseAscent = baseFont.getAscent();
        float baseDescent = baseFont.getDescent();
        float subtitleLineH = baseAscent - baseDescent;
        totalH += PART_GAP + subtitleLineH;
    }

    int startY = part.rect.y + (part.rect.h - static_cast<int>(totalH)) / 2;

    // Title: baseline = top of centered cell + ascent
    int titleY = startY + static_cast<int>(ascent);
    r->renderFormatted(slide.title, static_cast<float>(part.rect.x),
                       static_cast<float>(titleY), titleV, white);

    // Subtitle (Title slides only)
    if (!subtitleText.empty()) {
        const Font& baseFont = baseV.get(FontType::Regular);
        float baseAscent2 = baseFont.getAscent();
        int subtitleY = startY + static_cast<int>(titleLineH) + PART_GAP + static_cast<int>(baseAscent2);
        r->renderFormattedBlock(subtitleText, part.rect.x, subtitleY, baseV, ltgray, part.rect.w);
    }
}

SDL_Texture* Renderer::renderSlide(const Slide& slide, const FontSet& fonts, int slideNum, int totalSlides) {
    clear();

    SDL_Renderer* savedR = m_renderer;
    SDL_Surface* savedS = m_surface;
    m_renderer = nullptr;

    SDL_Surface* surf = SDL_CreateRGBSurface(0, m_width, m_height, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (surf) {
        SDL_FillRect(surf, nullptr, makeColor(surf, 30, 30, 40));
        m_surface = surf;

        // Compute layout metrics
        FontVariants titleV = fonts.titleVariants();
        FontVariants baseV = fonts.variants();
        const Font& titleFont = titleV.get(FontType::Regular);
        const Font& baseFont = baseV.get(FontType::Regular);

        LayoutMetrics metrics;
        metrics.slideW = m_width;
        metrics.slideH = m_height;
        metrics.titleAscent = titleFont.getAscent();
        metrics.titleDescent = titleFont.getDescent();
        metrics.titleLineH = metrics.titleAscent - metrics.titleDescent;
        metrics.bodyAscent = baseFont.getAscent();
        metrics.bodyDescent = baseFont.getDescent();
        metrics.bodyLineH = metrics.bodyAscent - metrics.bodyDescent;

        // Select layout and compute parts
        LayoutKind kind = selectLayout(slide);
        auto parts = computeParts(kind, slide, metrics);

        // Render each part
        for (const auto& part : parts) {
            switch (part.role) {
                case PartRole::FullSlide:
                    renderPartFullSlide(this, surf, slide, part, fonts);
                    break;
                case PartRole::Header:
                    renderPartHeader(this, surf, slide, part, fonts, m_width);
                    break;
                case PartRole::Body:
                    renderPartBody(this, surf, slide, part, fonts);
                    break;
                case PartRole::ColumnLeft:
                    renderPartColumnLeft(this, surf, slide, part, fonts);
                    break;
                case PartRole::ColumnRight:
                    renderPartColumnRight(this, surf, slide, part, fonts);
                    break;
                case PartRole::Image:
                    renderPartImage(this, surf, slide, part, fonts, m_height);
                    break;
                case PartRole::Caption:
                    renderPartCaption(this, surf, slide, part, fonts);
                    break;
                case PartRole::Footer:
                    renderPartFooter(this, surf, slide, part, fonts, slideNum, totalSlides);
                    break;
            }
        }

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
