#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utf8.h"

#include "renderer.h"
#include "layout.h"
#include "image.h"
#include "highlight.h"
#include <algorithm>
#include <cstring>

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
    SDL_FillRect(m_surface, nullptr, Color(r, g, b).toUint32(m_surface->format));
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

// Find the nearest XML formatting tag at or after pos.
// Returns marker position (or npos); *outLen = tag length, *outKind = 1=bold 2=italic 3=code, *outOpen = opening or closing tag.
static size_t findMarker(const std::string& text, size_t pos, size_t* outLen, int* outKind, bool* outOpen) {
    struct Tag { const char* open; const char* close; int kind; bool attribs; };
    static const Tag tags[] = {
        {"<b>",    "</b>",    1, false},
        {"<i>",    "</i>",    2, false},
        {"<code",  "</code>", 3, true},   // <code> or <code lang="...">
    };

    size_t best = std::string::npos;
    *outLen = 0;
    *outKind = 0;
    *outOpen = false;

    for (const auto& t : tags) {
        size_t po = text.find(t.open, pos);
        if (po != std::string::npos) {
            size_t tagLen;
            if (t.attribs) {
                size_t gt = text.find('>', po);
                if (gt == std::string::npos) goto tryClose;
                tagLen = gt - po + 1;
            } else {
                tagLen = strlen(t.open);
            }
            if (po < best || best == std::string::npos) {
                best = po; *outLen = tagLen; *outKind = t.kind; *outOpen = true;
            }
        }
        tryClose:
        size_t pc = text.find(t.close, pos);
        if (pc != std::string::npos && (best == std::string::npos || pc < best)) {
            best = pc; *outLen = strlen(t.close); *outKind = t.kind; *outOpen = false;
        }
    }
    return best;
}

static void applyFormat(TextFormat& fmt, int kind, bool open) {
    if (kind == 1)      fmt.bold = open;
    else if (kind == 2) fmt.italic = open;
    else                fmt.code = open;
}

// Measure text with inline format markers applied. Updates fmt as it scans,
// so callers can keep one state across word/line boundaries.
static float measureFormatted(const FontVariants& fonts, const std::string& text, TextFormat& fmt) {
    float width = 0;
    size_t pos = 0;

    while (pos < text.size()) {
        size_t markerLen = 0;
        int kind = 0;
        bool open = false;
        size_t marker = findMarker(text, pos, &markerLen, &kind, &open);

        if (marker == std::string::npos) {
            width += pickFont(fonts, fmt).measureString(text.substr(pos));
            break;
        }
        if (marker > pos) {
            width += pickFont(fonts, fmt).measureString(text.substr(pos, marker - pos));
        }
        applyFormat(fmt, kind, open);
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
        bool open = false;
        size_t marker = findMarker(text, pos, &markerLen, &kind, &open);

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

        if (kind == 3 && open) {
            std::string tag = text.substr(marker, markerLen);
            std::string lang;
            auto lp = tag.find("lang=\"");
            if (lp != std::string::npos) {
                lp += 6;
                auto le = tag.find('"', lp);
                if (le != std::string::npos) lang = tag.substr(lp, le - lp);
            }
            pos = marker + markerLen;

            size_t closeLen = 0; int ck = 0; bool co = false;
            size_t closeMarker = findMarker(text, pos, &closeLen, &ck, &co);
            if (closeMarker != std::string::npos && ck == 3 && !co) {
                std::string codeText = text.substr(pos, closeMarker - pos);
                if (!lang.empty()) {
                    LanguageSpec spec = loadLanguage(lang);
                    auto tokens = tokenize(codeText, spec);
                    const Font& monoFont = fonts.get(FontType::Monospace);
                    for (const auto& tok : tokens) {
                        std::string seg = codeText.substr(tok.start, tok.len);
                        SDL_Color tc;
                        switch (tok.type) {
                            case HighlightType::Keyword:     tc = m_style->codeKeyword.toSDLColor();     break;
                            case HighlightType::Type:        tc = m_style->codeType.toSDLColor();        break;
                            case HighlightType::String:      tc = m_style->codeString.toSDLColor();      break;
                            case HighlightType::Comment:     tc = m_style->codeComment.toSDLColor();     break;
                            case HighlightType::Number:      tc = m_style->codeNumber.toSDLColor();      break;
                            case HighlightType::Builtin:     tc = m_style->codeBuiltin.toSDLColor();     break;
                            case HighlightType::Punctuation: tc = m_style->codePunctuation.toSDLColor(); break;
                            default:                         tc = m_style->codeText.toSDLColor();        break;
                        }
                        drawText(seg, curX, y, monoFont, tc);
                        curX += monoFont.measureString(seg);
                    }
                } else {
                    const Font& monoFont = fonts.get(FontType::Monospace);
                    drawText(codeText, curX, y, monoFont, codeColor);
                    curX += monoFont.measureString(codeText);
                }
                pos = closeMarker + closeLen;
            } else {
                // No closing tag found — just toggle code mode
                applyFormat(fmt, kind, open);
            }
        } else {
            applyFormat(fmt, kind, open);
            pos = marker + markerLen;
        }
    }
}

int Renderer::textHeight(const Font& font) {
    return static_cast<int>(font.getFontSize()) + style().linePadding;
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

static void renderCodeBlock(Renderer* r, SDL_Surface* surf,
                            const CodeBlock& cb, const Font& monoFont,
                            int x, int y, int maxWidth, int) {
    const auto& s = r->style();
    if (cb.code.empty()) return;

    struct LineRange { size_t start; size_t end; };
    std::vector<LineRange> lines;
    {
        size_t start = 0;
        while (start < cb.code.size()) {
            size_t nl = cb.code.find('\n', start);
            size_t end = (nl == std::string::npos) ? cb.code.size() : nl;
            lines.push_back({start, end});
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        if (lines.empty()) lines.push_back({0, 0});
    }

    int lineH = static_cast<int>(monoFont.getAscent() - monoFont.getDescent()) + s.linePadding;
    int pad = std::max(8, s.partPadding / 2);
    int blockW = maxWidth;
    int blockH = static_cast<int>(lines.size()) * lineH + 2 * pad;

    Uint32 bg = SDL_MapRGBA(surf->format, s.codeBg.r, s.codeBg.g, s.codeBg.b, s.codeBg.a);
    SDL_Rect bgRect = {x, y, blockW, blockH};
    SDL_FillRect(surf, &bgRect, bg);

    Uint32 brd = SDL_MapRGBA(surf->format, s.codeBorder.r, s.codeBorder.g, s.codeBorder.b, s.codeBorder.a);
    r->drawRectOutline(&bgRect, brd);

    LanguageSpec spec;
    if (!cb.lang.empty()) spec = loadLanguage(cb.lang);
    auto tokens = tokenize(cb.code, spec);

    float ascent = monoFont.getAscent();
    float curX0 = static_cast<float>(x + pad);
    int curY = y + pad + static_cast<int>(ascent);

    size_t tokIdx = 0;
    for (const auto& line : lines) {
        float curX = curX0;

        while (tokIdx < tokens.size() && tokens[tokIdx].start < line.end) {
            const auto& tok = tokens[tokIdx];
            size_t tokEnd = tok.start + tok.len;
            if (tokEnd <= line.start) { tokIdx++; continue; }

            size_t segStart = std::max(tok.start, line.start);
            size_t segEnd = std::min(tokEnd, line.end);

            std::string text = cb.code.substr(segStart, segEnd - segStart);

            SDL_Color clr;
            switch (tok.type) {
                case HighlightType::Keyword:     clr = s.codeKeyword.toSDLColor();     break;
                case HighlightType::Type:        clr = s.codeType.toSDLColor();        break;
                case HighlightType::String:      clr = s.codeString.toSDLColor();      break;
                case HighlightType::Comment:     clr = s.codeComment.toSDLColor();     break;
                case HighlightType::Number:      clr = s.codeNumber.toSDLColor();      break;
                case HighlightType::Builtin:     clr = s.codeBuiltin.toSDLColor();     break;
                case HighlightType::Punctuation: clr = s.codePunctuation.toSDLColor(); break;
                default:                         clr = s.codeText.toSDLColor();        break;
            }

            r->drawText(text, curX, static_cast<float>(curY), monoFont, clr);
            curX += monoFont.measureString(text);

            if (tokEnd <= line.end) tokIdx++;
            else break;
        }

        curY += lineH;
    }
}

// ---- slide rendering: part-based layout ----

// Forward declarations (used by recursive renderPartSlot)
static void renderPartFullSlide(Renderer* r, SDL_Surface* surf, const Slide& slide, const SlidePart& part, const FontSet& fonts, const FontVariants& titleV);
static void renderPartImage(Renderer* r, SDL_Surface* surf, const Slide& slide, const SlidePart& part, const FontSet& fonts, int slideH);
static void renderPartCaption(Renderer* r, SDL_Surface* surf, const Slide& slide, const SlidePart& part, const FontSet& fonts);

static void renderPartHeader(Renderer* r, SDL_Surface* surf,
                             const Slide& slide, const SlidePart& part,
                             const FontVariants& titleV, int slideW) {
    const Font& titleFont = titleV.get(FontType::Regular);
    const auto& s = r->style();
    float ascent = titleFont.getAscent();
    float descent = titleFont.getDescent();
    float titleLineH = ascent - descent;
    int textY = part.rect.y + (part.rect.h - static_cast<int>(titleLineH)) / 2 + static_cast<int>(ascent);

    r->renderFormatted(slide.title, static_cast<float>(part.rect.x + s.partPadding),
                       static_cast<float>(textY), titleV, s.titleColor.toSDLColor());

    Uint32 lc = SDL_MapRGBA(surf->format, s.lineColor.r, s.lineColor.g, s.lineColor.b, s.lineColor.a);
    SDL_Rect lineRect = {part.rect.x, part.rect.y + part.rect.h - 1, part.rect.w, 1};
    SDL_FillRect(surf, &lineRect, lc);
}

static void renderPartBody(Renderer* r, SDL_Surface* surf,
                           const Slide& slide, const SlidePart& part,
                           const FontSet& fonts, const FontVariants& titleV) {
    FontVariants baseV = fonts.variants();
    const auto& s = r->style();
    int contentW = part.rect.w - 2 * s.partPadding;
    int y = part.rect.y + s.partPadding;

    for (size_t i = 0; i < slide.texts.size(); i++) {
        std::string heading;
        if (isHeadingLine(slide.texts[i], &heading)) {
            r->renderFormatted(heading, static_cast<float>(part.rect.x + s.partPadding),
                               static_cast<float>(y), titleV, s.titleColor.toSDLColor());
            y += r->textHeight(titleV.get(FontType::Regular));
        } else {
            std::string line = "\xE2\x80\xA2 " + slide.texts[i];
            r->renderFormattedBlock(line, part.rect.x + s.partPadding, y, baseV, s.textColor.toSDLColor(), contentW);
            y += r->textHeight(baseV.get(FontType::Regular));
        }
    }

    const Font& monoFont = fonts.get(FontType::Monospace);
    for (const auto& cb : slide.codeBlocks) {
        size_t lines = 1;
        for (char ch : cb.code) if (ch == '\n') lines++;
        int lineH = static_cast<int>(monoFont.getAscent() - monoFont.getDescent()) + s.linePadding;
        int pad = std::max(8, s.partPadding / 2);
        int blockH = static_cast<int>(lines) * lineH + 2 * pad;

        renderCodeBlock(r, surf, cb, monoFont,
                        part.rect.x + s.partPadding, y,
                        contentW, blockH);
        y += blockH + s.partGap;
    }
}

// Recursively render a child slide into a slot area
static void renderPartSlot(Renderer* r, SDL_Surface* surf,
                           const Slide& slide, const SlidePart& part,
                           const FontSet& fonts, int slideNum, int totalSlides) {
    if (part.childIndex < 0 || part.childIndex >= static_cast<int>(slide.children.size())) return;
    const Slide& child = slide.children[part.childIndex];

    // Render child slide to its own surface, then blit into the slot rect
    int slotW = part.rect.w;
    int slotH = part.rect.h;
    SDL_Surface* childSurf = SDL_CreateRGBSurface(0, slotW, slotH, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!childSurf) return;

    SDL_FillRect(childSurf, nullptr, r->style().bgColor.toUint32(surf->format));

    // Save state and set up child surface
    SDL_Renderer* savedR = r->sdlRenderer();
    SDL_Surface* savedS = r->surface();
    r->setSdlRenderer(nullptr);
    r->setSurface(childSurf);

    // Compute layout for the child slide at slot size
    FontVariants titleV = fonts.childTitleVariants();
    FontVariants baseV = fonts.variants();
    const Font& titleFont = titleV.get(FontType::Regular);
    const Font& baseFont = baseV.get(FontType::Regular);

    LayoutMetrics metrics;
    metrics.slideW = slotW;
    metrics.slideH = slotH;
    metrics.titleAscent = titleFont.getAscent();
    metrics.titleDescent = titleFont.getDescent();
    metrics.titleLineH = metrics.titleAscent - metrics.titleDescent;
    metrics.bodyAscent = baseFont.getAscent();
    metrics.bodyDescent = baseFont.getDescent();
    metrics.bodyLineH = metrics.bodyAscent - metrics.bodyDescent;

    LayoutKind kind = layoutFromSlide(child);
    auto parts = computeParts(kind, child, metrics, r->style());

    for (const auto& p : parts) {
        switch (p.role) {
            case PartRole::FullSlide:  renderPartFullSlide(r, childSurf, child, p, fonts, titleV); break;
            case PartRole::Header:     renderPartHeader(r, childSurf, child, p, titleV, slotW); break;
            case PartRole::Body:       renderPartBody(r, childSurf, child, p, fonts, titleV); break;
            case PartRole::Slot:       renderPartSlot(r, childSurf, child, p, fonts, 0, 0); break;
            case PartRole::Image:      renderPartImage(r, childSurf, child, p, fonts, slotH); break;
            case PartRole::Caption:    renderPartCaption(r, childSurf, child, p, fonts); break;
            case PartRole::Footer:     break;
        }
    }

    r->setSdlRenderer(savedR);
    r->setSurface(savedS);

    SDL_Rect dst = {part.rect.x, part.rect.y, slotW, slotH};
    SDL_BlitSurface(childSurf, nullptr, surf, &dst);
    SDL_FreeSurface(childSurf);
}

static void renderImageAt(SDL_Surface* surf, const std::string& imagePath,
                          const SlidePart& part, ImageFit fit,
                          const Font& baseFont) {
    if (imagePath.empty()) return;

    int imgW = 0, imgH = 0, channels = 0;
    unsigned char* data = stbi_load(imagePath.c_str(), &imgW, &imgH, &channels, 4);

    if (data && imgW > 0 && imgH > 0) {
        float scaleX = static_cast<float>(part.rect.w) / static_cast<float>(imgW);
        float scaleY = static_cast<float>(part.rect.h) / static_cast<float>(imgH);

        int dstW, dstH, dstX, dstY;
        ImageBuf srcBuf;

        if (fit == ImageFit::Fill) {
            float scale = std::max(scaleX, scaleY);
            int cropW = static_cast<int>(part.rect.w / scale);
            int cropH = static_cast<int>(part.rect.h / scale);
            int cropX = std::max(0, (imgW - cropW) / 2);
            int cropY = std::max(0, (imgH - cropH) / 2);
            cropW = std::min(cropW, imgW - cropX);
            cropH = std::min(cropH, imgH - cropY);

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
            srcBuf.data = data;
        }

        ImageBuf resampled = resampleBilinear(srcBuf, dstW, dstH);

        if (fit == ImageFit::Fill)
            delete[] srcBuf.data;

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
        Color phC = "#32323C";
        SDL_Rect ph = {part.rect.x, part.rect.y, part.rect.w, part.rect.h};
        SDL_FillRect(surf, &ph, phC.toUint32(surf->format));
    }
}

static void renderPartImage(Renderer* r, SDL_Surface* surf,
                            const Slide& slide, const SlidePart& part,
                            const FontSet& fonts, int slideH) {
    const Font& baseFont = fonts.variants().get(FontType::Regular);
    renderImageAt(surf, slide.imagePath, part, slide.imageFit, baseFont);
}

static void renderPartCaption(Renderer* r, SDL_Surface* surf,
                              const Slide& slide, const SlidePart& part,
                              const FontSet& fonts) {
    std::string text = slide.caption;
    if (text.empty()) {
        for (const auto& t : slide.texts) {
            if (!text.empty()) text += " ";
            text += t;
        }
    }
    if (text.empty()) return;
    FontVariants baseV = fonts.variants();
    const auto& s = r->style();
    int contentW = part.rect.w - 2 * s.partPadding;
    r->renderFormattedBlock(text, part.rect.x + s.partPadding, part.rect.y + s.partPadding,
                            baseV, s.textColor.toSDLColor(), contentW);
}

static void renderPartFooter(Renderer* r, SDL_Surface* surf,
                             const Slide& slide, const SlidePart& part,
                             const FontSet& fonts, int slideNum, int totalSlides) {
    FontVariants smallV = fonts.smallVariants();
    const Font& smallFont = smallV.get(FontType::Regular);
    const auto& s = r->style();

    char numBuf[64];
    snprintf(numBuf, sizeof(numBuf), "%d / %d", slideNum, totalSlides);
    float numW = smallFont.measureString(numBuf);
    float x = static_cast<float>(part.rect.x + part.rect.w - s.partPadding) - numW;
    float y = static_cast<float>(part.rect.y + (part.rect.h - r->textHeight(smallFont)) / 2);
    r->drawText(numBuf, x, y, smallFont, s.dimColor.toSDLColor());
}

static void renderPartFullSlide(Renderer* r, SDL_Surface* surf,
                                const Slide& slide, const SlidePart& part,
                                const FontSet& fonts, const FontVariants& titleV) {
    FontVariants baseV = fonts.variants();
    const auto& s = r->style();

    const Font& titleFont = titleV.get(FontType::Regular);
    float ascent = titleFont.getAscent();
    float descent = titleFont.getDescent();
    float titleLineH = ascent - descent;

    std::string subtitleText = slide.subtitle;

    float totalH = titleLineH;
    if (!subtitleText.empty()) {
        const Font& baseFont = baseV.get(FontType::Regular);
        float baseAscent = baseFont.getAscent();
        float baseDescent = baseFont.getDescent();
        float subtitleLineH = baseAscent - baseDescent;
        totalH += s.partGap + subtitleLineH;
    }

    int startY = part.rect.y + (part.rect.h - static_cast<int>(totalH)) / 2;
    int titleY = startY + static_cast<int>(ascent);
    r->renderFormatted(slide.title, static_cast<float>(part.rect.x),
                       static_cast<float>(titleY), titleV, s.titleColor.toSDLColor());

    if (!subtitleText.empty()) {
        const Font& baseFont = baseV.get(FontType::Regular);
        float baseAscent2 = baseFont.getAscent();
        int subtitleY = startY + static_cast<int>(titleLineH) + s.partGap + static_cast<int>(baseAscent2);
        r->renderFormattedBlock(subtitleText, part.rect.x, subtitleY, baseV, s.subtitleColor.toSDLColor(), part.rect.w);
    }
}

SDL_Texture* Renderer::renderSlide(const Slide& slide, const FontSet& fonts, const PresentationStyle& style, int slideNum, int totalSlides) {
    setStyle(&style);
    clear(style.bgColor.r, style.bgColor.g, style.bgColor.b);

    SDL_Renderer* savedR = m_renderer;
    SDL_Surface* savedS = m_surface;
    m_renderer = nullptr;

    SDL_Surface* surf = SDL_CreateRGBSurface(0, m_width, m_height, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (surf) {
        SDL_FillRect(surf, nullptr, style.bgColor.toUint32(surf->format));
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
        LayoutKind kind = layoutFromSlide(slide);
        auto parts = computeParts(kind, slide, metrics, style);

        // Render each part
        for (const auto& part : parts) {
            switch (part.role) {
                case PartRole::FullSlide:
                    renderPartFullSlide(this, surf, slide, part, fonts, titleV);
                    break;
                case PartRole::Header:
                    renderPartHeader(this, surf, slide, part, titleV, m_width);
                    break;
                case PartRole::Body:
                    renderPartBody(this, surf, slide, part, fonts, titleV);
                    break;
                case PartRole::Slot:
                    renderPartSlot(this, surf, slide, part, fonts, slideNum, totalSlides);
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
    const auto& s = pres.style;
    setStyle(&s);
    clear(s.bgColor.r + 10, s.bgColor.g + 10, s.bgColor.b + 10);

    int margin = s.presenterMargin;
    FontVariants baseV = fonts.variants();
    FontVariants smallV = fonts.smallVariants();
    const Font& baseFont = baseV.get(FontType::Regular);
    const Font& smallFont = smallV.get(FontType::Regular);
    int th = textHeight(smallFont);
    int y = margin;

    char numBuf[64];
    snprintf(numBuf, sizeof(numBuf), "Slide %d / %d", pres.current + 1, pres.size());
    drawText(numBuf, static_cast<float>(margin), static_cast<float>(y), smallFont, s.dimColor.toSDLColor());
    y += th + 4;

    const Slide& current = pres.currentSlide();
    renderFormatted(current.title, static_cast<float>(margin), static_cast<float>(y), baseV, s.titleColor.toSDLColor());
    y += textHeight(baseFont) + 12;

    int contentW = m_width - 2 * margin;
    if (!current.notes.empty()) {
        Uint32 notesBg = Color(s.bgColor.r - 5, s.bgColor.g - 5, s.bgColor.b + 5).toUint32(m_surface->format);
        int notesH = std::max(th * 3, m_height - y - margin - th * 2);
        SDL_Rect notesRect = {margin, y - 18, contentW, notesH};
        SDL_FillRect(m_surface, &notesRect, notesBg);
        drawRectOutline(&notesRect, s.lineColor.toUint32(m_surface->format));

        drawText("Notes:", static_cast<float>(margin + 6),
                 static_cast<float>(y + 4), smallFont, s.dimColor.toSDLColor());
        renderFormattedBlock(current.notes, margin + 6, y + th + 4, smallV, s.textColor.toSDLColor(), contentW - 12);
        y += notesH + 12;
    }

    if (pres.canGoNext()) {
        const Slide& next = pres.slides[pres.current + 1];
        drawText("Next:", static_cast<float>(margin),
                 static_cast<float>(y), smallFont, s.dimColor.toSDLColor());
        drawText(next.title, static_cast<float>(margin),
                 static_cast<float>(y + th + 2), smallFont, s.textColor.toSDLColor());
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, m_surface);
    return texture;
}
