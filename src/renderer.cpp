#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utf8.h"

#include "renderer.h"
#include "charts.h"
#include "layout.h"
#include "image.h"
#include "highlight.h"
#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::init(SDL_Renderer* renderer, int width, int height) {
    cleanup();
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

static void blendPixel(SDL_Surface* surf, int x, int y, Uint32 color, float coverage) {
    if (x < 0 || x >= surf->w || y < 0 || y >= surf->h) return;
    coverage = std::max(0.0f, std::min(1.0f, coverage));
    if (coverage <= 0.0f) return;

    auto* pixels = static_cast<Uint32*>(surf->pixels);
    Uint32& dst = pixels[y * (surf->pitch / 4) + x];
    if (coverage >= 1.0f) {
        dst = color;
        return;
    }

    Uint8 sr, sg, sb, sa;
    Uint8 dr, dg, db, da;
    SDL_GetRGBA(color, surf->format, &sr, &sg, &sb, &sa);
    SDL_GetRGBA(dst, surf->format, &dr, &dg, &db, &da);

    float alpha = coverage * (static_cast<float>(sa) / 255.0f);
    Uint8 rr = static_cast<Uint8>(std::lround(sr * alpha + dr * (1.0f - alpha)));
    Uint8 rg = static_cast<Uint8>(std::lround(sg * alpha + dg * (1.0f - alpha)));
    Uint8 rb = static_cast<Uint8>(std::lround(sb * alpha + db * (1.0f - alpha)));
    Uint8 ra = static_cast<Uint8>(std::lround(255.0f * (alpha + (da / 255.0f) * (1.0f - alpha))));
    dst = SDL_MapRGBA(surf->format, rr, rg, rb, ra);
}

// Estimate the area of a pixel covered by a rounded rectangle using a regular
// 4x4 sub-pixel grid. Straight edges remain exact; only arc pixels are blended.
static float roundedRectCoverage(int width, int height, int radius, int x, int y) {
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height) return 0.0f;
    int rad = std::min(radius, std::min(width, height) / 2);
    if (rad <= 0) return 1.0f;

    constexpr int samplesPerAxis = 4;
    int covered = 0;
    for (int sy = 0; sy < samplesPerAxis; ++sy) {
        float py = static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / samplesPerAxis;
        for (int sx = 0; sx < samplesPerAxis; ++sx) {
            float px = static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / samplesPerAxis;
            float cx = std::max(static_cast<float>(rad),
                                std::min(px, static_cast<float>(width - rad)));
            float cy = std::max(static_cast<float>(rad),
                                std::min(py, static_cast<float>(height - rad)));
            float dx = px - cx;
            float dy = py - cy;
            if (dx * dx + dy * dy <= static_cast<float>(rad * rad)) ++covered;
        }
    }
    return static_cast<float>(covered) / static_cast<float>(samplesPerAxis * samplesPerAxis);
}

static void fillRoundedRect(SDL_Surface* surf, SDL_Rect r, int radius, Uint32 color) {
    int rad = std::min(radius, std::min(r.w, r.h) / 2);
    if (rad <= 0) { SDL_FillRect(surf, &r, color); return; }

    SDL_Rect horizontal = {r.x + rad, r.y, r.w - 2 * rad, r.h};
    SDL_Rect vertical = {r.x, r.y + rad, r.w, r.h - 2 * rad};
    if (horizontal.w > 0) SDL_FillRect(surf, &horizontal, color);
    if (vertical.h > 0) SDL_FillRect(surf, &vertical, color);

    for (int y = 0; y < rad; ++y) {
        for (int x = 0; x < rad; ++x) {
            float coverage = roundedRectCoverage(r.w, r.h, rad, x, y);
            blendPixel(surf, r.x + x, r.y + y, color, coverage);
            blendPixel(surf, r.x + r.w - 1 - x, r.y + y, color, coverage);
            blendPixel(surf, r.x + x, r.y + r.h - 1 - y, color, coverage);
            blendPixel(surf, r.x + r.w - 1 - x, r.y + r.h - 1 - y, color, coverage);
        }
    }
}

static void drawRoundedRectOutline(SDL_Surface* surf, SDL_Rect r, int radius, Uint32 color) {
    int rad = std::min(radius, std::min(r.w, r.h) / 2);
    if (r.w <= 0 || r.h <= 0) return;
    if (rad <= 0) {
        SDL_Rect top = {r.x, r.y, r.w, 1};
        SDL_Rect bottom = {r.x, r.y + r.h - 1, r.w, 1};
        SDL_Rect left = {r.x, r.y, 1, r.h};
        SDL_Rect right = {r.x + r.w - 1, r.y, 1, r.h};
        SDL_FillRect(surf, &top, color);
        SDL_FillRect(surf, &bottom, color);
        SDL_FillRect(surf, &left, color);
        SDL_FillRect(surf, &right, color);
        return;
    }

    int innerW = r.w - 2;
    int innerH = r.h - 2;
    int innerRad = std::max(0, rad - 1);
    for (int y = 0; y < r.h; ++y) {
        for (int x = 0; x < r.w; ++x) {
            bool nearEdge = y < 2 || y >= r.h - 2 ||
                            x < rad + 1 || x >= r.w - rad - 1;
            if (!nearEdge) continue;
            float outer = roundedRectCoverage(r.w, r.h, rad, x, y);
            float inner = roundedRectCoverage(innerW, innerH, innerRad, x - 1, y - 1);
            blendPixel(surf, r.x + x, r.y + y, color, std::max(0.0f, outer - inner));
        }
    }
}

static void maskImageCorners(SDL_Surface* surf, SDL_Rect r, int radius, Uint32 bgColor) {
    int rad = std::min(radius, std::min(r.w, r.h) / 2);
    if (rad <= 0) return;

    for (int y = 0; y < rad; ++y) {
        for (int x = 0; x < rad; ++x) {
            float outside = 1.0f - roundedRectCoverage(r.w, r.h, rad, x, y);
            blendPixel(surf, r.x + x, r.y + y, bgColor, outside);
            blendPixel(surf, r.x + r.w - 1 - x, r.y + y, bgColor, outside);
            blendPixel(surf, r.x + x, r.y + r.h - 1 - y, bgColor, outside);
            blendPixel(surf, r.x + r.w - 1 - x, r.y + r.h - 1 - y, bgColor, outside);
        }
    }
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

void Renderer::fillRect(const SDL_Rect& rect, Color color, int cornerRadius) {
    if (!m_surface || rect.w <= 0 || rect.h <= 0) return;
    fillRoundedRect(m_surface, rect, cornerRadius, color.toUint32(m_surface->format));
}

void Renderer::drawRectOutline(const SDL_Rect& rect, Color color, int cornerRadius) {
    if (!m_surface || rect.w <= 0 || rect.h <= 0) return;
    drawRoundedRectOutline(
        m_surface, rect, cornerRadius, color.toUint32(m_surface->format));
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
    fillRoundedRect(surf, bgRect, s.cornerRadius, bg);

    Uint32 brd = SDL_MapRGBA(surf->format, s.codeBorder.r, s.codeBorder.g, s.codeBorder.b, s.codeBorder.a);
    drawRoundedRectOutline(surf, bgRect, s.cornerRadius, brd);

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
static void renderPartImage(Renderer* r, SDL_Surface* surf, const Slide& slide, const SlidePart& part);
static void renderPartCaption(Renderer* r, SDL_Surface* surf, const Slide& slide, const SlidePart& part, const FontSet& fonts);
static void centerImageCaptionStack(Renderer* r, const Slide& slide,
                                    const FontSet& fonts,
                                    std::vector<SlidePart>& parts,
                                    int availableBottom);

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

    r->fillRect({part.rect.x, part.rect.y + part.rect.h - 1, part.rect.w, 1}, s.lineColor);
}

static int codeLineCount(const std::string& code) {
    int lines = 1;
    for (char ch : code) if (ch == '\n') ++lines;
    return lines;
}

static int codeNaturalWidth(const CodeBlock& cb, const Font& font, int padding) {
    int width = 0;
    size_t start = 0;
    while (true) {
        size_t newline = cb.code.find('\n', start);
        std::string line = newline == std::string::npos
            ? cb.code.substr(start)
            : cb.code.substr(start, newline - start);
        width = std::max(width, static_cast<int>(std::ceil(font.measureString(line))));
        if (newline == std::string::npos) break;
        start = newline + 1;
    }
    return width + 2 * padding;
}

static int customBulletWidth(const std::string& icon, const FontSet& fonts,
                             const PresentationStyle& style) {
    if (icon.empty() || icon == "none") return 0;
    uint32_t codepoint = iconCodepoint(icon);
    float advance = fonts.bulletIcons().measureGlyph(codepoint);
    if (advance <= 0) advance = fonts.bulletIcons().getFontSize();
    return static_cast<int>(std::ceil(advance)) + style.partGap / 2;
}

static int measureBodyHeight(Renderer* r, const Slide& slide,
                             const FontSet& fonts, const FontVariants& titleV,
                             int contentW) {
    FontVariants bulletV = fonts.bulletVariants();
    const auto& s = r->style();
    const Font& titleFont = titleV.get(FontType::Regular);
    const Font& bulletFont = bulletV.get(FontType::Regular);
    const Font& monoFont = fonts.get(FontType::Monospace);
    int titleLineH = r->textHeight(titleFont);
    int bulletLineH = r->textHeight(bulletFont);
    int monoLineH = static_cast<int>(monoFont.getAscent() - monoFont.getDescent()) + s.linePadding;
    int codePad = std::max(8, s.partPadding / 2);

    int contentH = 0;
    bool hasContent = false;
    for (size_t i = 0; i < slide.texts.size(); ++i) {
        const auto& text = slide.texts[i];
        if (hasContent) contentH += s.bulletGap;
        std::string heading;
        if (isHeadingLine(text, &heading)) {
            contentH += titleLineH;
        } else {
            std::string icon = i < slide.textIcons.size()
                ? slide.textIcons[i] : std::string();
            if (icon.empty()) {
                std::string line = "\xE2\x80\xA2 " + text;
                contentH += static_cast<int>(
                    r->wordWrap(line, bulletV, contentW).size()) * bulletLineH;
            } else {
                int markerW = customBulletWidth(icon, fonts, s);
                contentH += static_cast<int>(
                    r->wordWrap(text, bulletV,
                                std::max(1, contentW - markerW)).size()) *
                    bulletLineH;
            }
        }
        hasContent = true;
    }
    for (const auto& cb : slide.codeBlocks) {
        if (hasContent) contentH += s.partGap;
        contentH += codeLineCount(cb.code) * monoLineH + 2 * codePad;
        hasContent = true;
    }
    for (const auto& icon : slide.icons) {
        (void)icon;
        if (hasContent) contentH += s.bulletGap;
        contentH += iconBlockNaturalHeight(r, fonts);
        hasContent = true;
    }
    for (const auto& chart : slide.charts) {
        if (hasContent) contentH += s.partGap;
        contentH += chartNaturalHeight(chart);
        hasContent = true;
    }
    return contentH;
}

static void renderBodyContent(Renderer* r, SDL_Surface* surf,
                              const Slide& slide, const FontSet& fonts,
                              const FontVariants& titleV,
                              int contentX, int contentY, int contentW) {
    FontVariants bulletV = fonts.bulletVariants();
    const auto& s = r->style();
    const Font& titleFont = titleV.get(FontType::Regular);
    const Font& bulletFont = bulletV.get(FontType::Regular);
    const Font& monoFont = fonts.get(FontType::Monospace);
    int titleLineH = r->textHeight(titleFont);
    int bulletLineH = r->textHeight(bulletFont);
    int monoLineH = static_cast<int>(monoFont.getAscent() - monoFont.getDescent()) + s.linePadding;
    int codePad = std::max(8, s.partPadding / 2);
    int y = contentY;
    bool renderedContent = false;

    for (size_t i = 0; i < slide.texts.size(); i++) {
        if (renderedContent) y += s.bulletGap;
        std::string heading;
        if (isHeadingLine(slide.texts[i], &heading)) {
            r->renderFormatted(heading, static_cast<float>(contentX),
                               static_cast<float>(y) + titleFont.getAscent(),
                               titleV, s.titleColor.toSDLColor());
            y += titleLineH;
        } else {
            std::string icon = i < slide.textIcons.size()
                ? slide.textIcons[i] : std::string();
            std::string line = icon.empty()
                ? "\xE2\x80\xA2 " + slide.texts[i]
                : slide.texts[i];
            int markerW = customBulletWidth(icon, fonts, s);
            int textW = std::max(1, contentW - markerW);
            int wrappedLines = static_cast<int>(
                r->wordWrap(line, bulletV, textW).size());
            if (markerW > 0) {
                uint32_t codepoint = iconCodepoint(icon);
                if (codepoint && fonts.bulletIcons().hasGlyph(codepoint)) {
                    fonts.bulletIcons().drawGlyph(
                        surf, codepoint, static_cast<float>(contentX),
                        y + bulletFont.getAscent(),
                        s.accentColor.toSDLColor());
                }
            }
            r->renderFormattedBlock(line, contentX + markerW,
                                    y + static_cast<int>(bulletFont.getAscent()),
                                    bulletV, s.textColor.toSDLColor(), textW);
            y += wrappedLines * bulletLineH;
        }
        renderedContent = true;
    }

    for (const auto& cb : slide.codeBlocks) {
        if (renderedContent) y += s.partGap;
        int blockH = codeLineCount(cb.code) * monoLineH + 2 * codePad;

        renderCodeBlock(r, surf, cb, monoFont,
                        contentX, y, contentW, blockH);
        y += blockH;
        renderedContent = true;
    }

    for (const auto& icon : slide.icons) {
        if (renderedContent) y += s.bulletGap;
        renderIconBlock(r, surf, icon, fonts, contentX, y, contentW);
        y += iconBlockNaturalHeight(r, fonts);
        renderedContent = true;
    }

    for (const auto& chart : slide.charts) {
        if (renderedContent) y += s.partGap;
        int chartH = chartNaturalHeight(chart);
        renderChart(r, surf, chart, fonts, contentX, y, contentW, chartH);
        y += chartH;
        renderedContent = true;
    }
}

static void renderPartBody(Renderer* r, SDL_Surface* surf,
                           const Slide& slide, const SlidePart& part,
                           const FontSet& fonts, const FontVariants& titleV) {
    const auto& s = r->style();
    int contentW = std::max(0, part.rect.w - 2 * s.partPadding);
    int availableH = std::max(0, part.rect.h - 2 * s.partPadding);
    if (contentW == 0 || availableH == 0) return;

    const Font& monoFont = fonts.get(FontType::Monospace);
    int codePad = std::max(8, s.partPadding / 2);
    int naturalW = contentW;
    for (const auto& cb : slide.codeBlocks)
        naturalW = std::max(naturalW, codeNaturalWidth(cb, monoFont, codePad));

    int naturalH = measureBodyHeight(r, slide, fonts, titleV, naturalW);
    if (naturalH <= 0) return;

    Rect available = {
        part.rect.x + s.partPadding,
        part.rect.y + s.partPadding,
        contentW,
        availableH
    };
    Rect placement = computeImageCaptionStack(
        available, naturalW, naturalH, 0, 0, ImageFit::Fit).image;

    if (placement.w == naturalW && placement.h == naturalH) {
        renderBodyContent(r, surf, slide, fonts, titleV,
                          placement.x, placement.y, naturalW);
        return;
    }

    SDL_Surface* contentSurf = SDL_CreateRGBSurface(
        0, naturalW, naturalH, 32,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (!contentSurf) return;
    SDL_Surface* savedSurface = r->surface();
    r->setSurface(contentSurf);
    r->fillRect({0, 0, naturalW, naturalH}, s.bgColor);
    renderBodyContent(r, contentSurf, slide, fonts, titleV, 0, 0, naturalW);
    r->setSurface(savedSurface);

    ImageBuf source = {
        static_cast<uint8_t*>(contentSurf->pixels),
        contentSurf->w,
        contentSurf->h
    };
    ImageBuf scaled = resampleBilinear(source, placement.w, placement.h);
    if (scaled.data) {
        SDL_Surface* scaledSurf = SDL_CreateRGBSurfaceFrom(
            scaled.data, scaled.w, scaled.h, 32, scaled.w * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        if (scaledSurf) {
            SDL_Rect destination = {placement.x, placement.y, placement.w, placement.h};
            SDL_BlitSurface(scaledSurf, nullptr, surf, &destination);
            SDL_FreeSurface(scaledSurf);
        }
        delete[] scaled.data;
    }
    SDL_FreeSurface(contentSurf);
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

    // Save state and set up child surface
    SDL_Renderer* savedR = r->sdlRenderer();
    SDL_Surface* savedS = r->surface();
    r->setSdlRenderer(nullptr);
    r->setSurface(childSurf);
    r->fillRect({0, 0, slotW, slotH}, r->style().bgColor);

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
    if (kind == LayoutKind::HeaderImage)
        centerImageCaptionStack(r, child, fonts, parts, slotH);

    for (const auto& p : parts) {
        switch (p.role) {
            case PartRole::FullSlide:  renderPartFullSlide(r, childSurf, child, p, fonts, titleV); break;
            case PartRole::Header:     renderPartHeader(r, childSurf, child, p, titleV, slotW); break;
            case PartRole::Body:       renderPartBody(r, childSurf, child, p, fonts, titleV); break;
            case PartRole::Slot:       renderPartSlot(r, childSurf, child, p, fonts, 0, 0); break;
            case PartRole::Image:      renderPartImage(r, childSurf, child, p); break;
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
                          int cornerRadius, Uint32 bgColor, Color placeholderBg) {
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
            ImageRect placement = fitImageToArea(
                imgW, imgH,
                part.rect.x, part.rect.y, part.rect.w, part.rect.h);
            dstW = placement.w;
            dstH = placement.h;
            dstX = placement.x;
            dstY = placement.y;

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
                maskImageCorners(surf, dstRect, cornerRadius, bgColor);
                SDL_FreeSurface(imgSurface);
            }
            delete[] resampled.data;
        }

        stbi_image_free(data);
    } else {
        SDL_Rect ph = {part.rect.x, part.rect.y, part.rect.w, part.rect.h};
        SDL_FillRect(surf, &ph, placeholderBg.toUint32(surf->format));
    }
}

static void renderPartImage(Renderer* r, SDL_Surface* surf,
                            const Slide& slide, const SlidePart& part) {
    Uint32 bgColor = r->style().bgColor.toUint32(surf->format);
    renderImageAt(surf, slide.imagePath, part, slide.imageFit,
                  r->style().cornerRadius, bgColor, r->style().codeBg);
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

static std::string captionText(const Slide& slide) {
    if (!slide.caption.empty()) return slide.caption;

    std::string text;
    for (const auto& line : slide.texts) {
        if (!text.empty()) text += " ";
        text += line;
    }
    return text;
}

static void centerImageCaptionStack(Renderer* r, const Slide& slide,
                                    const FontSet& fonts,
                                    std::vector<SlidePart>& parts,
                                    int availableBottom) {
    SlidePart* image = nullptr;
    SlidePart* caption = nullptr;
    for (auto& part : parts) {
        if (part.role == PartRole::Image) image = &part;
        if (part.role == PartRole::Caption) caption = &part;
    }
    if (!image || !caption) return;

    const auto& s = r->style();
    std::string text = captionText(slide);
    int captionH = 0;
    if (!text.empty()) {
        FontVariants baseV = fonts.variants();
        int contentW = std::max(1, image->rect.w - 2 * s.partPadding);
        int lineCount = static_cast<int>(r->wordWrap(text, baseV, contentW).size());
        captionH = lineCount * r->textHeight(baseV.get(FontType::Regular)) + s.partPadding;
    }

    int sourceW = 0;
    int sourceH = 0;
    int channels = 0;
    if (slide.imagePath.empty() ||
        !stbi_info(slide.imagePath.c_str(), &sourceW, &sourceH, &channels)) {
        sourceW = std::max(1, image->rect.w);
        sourceH = std::max(1, availableBottom - image->rect.y - captionH);
    }

    Rect available = {
        image->rect.x,
        image->rect.y,
        image->rect.w,
        std::max(0, availableBottom - image->rect.y)
    };
    ImageCaptionStack stack = computeImageCaptionStack(
        available, sourceW, sourceH, captionH, s.partGap, slide.imageFit);
    image->rect = stack.image;
    caption->rect = stack.caption;
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

    SDL_Renderer* savedR = m_renderer;
    m_renderer = nullptr;

    // Render directly into the retained backing surface. The previous path
    // allocated a second full-window surface, drew into it, copied every
    // pixel, and freed it again for every slide change.
    SDL_Surface* surf = m_surface;
    if (surf) {
        fillRect({0, 0, m_width, m_height}, style.bgColor);

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
        if (kind == LayoutKind::HeaderImage) {
            int availableBottom = m_height;
            for (const auto& part : parts) {
                if (part.role == PartRole::Footer) {
                    availableBottom = part.rect.y;
                    break;
                }
            }
            centerImageCaptionStack(this, slide, fonts, parts, availableBottom);
        }

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
                    renderPartImage(this, surf, slide, part);
                    break;
                case PartRole::Caption:
                    renderPartCaption(this, surf, slide, part, fonts);
                    break;
                case PartRole::Footer:
                    renderPartFooter(this, surf, slide, part, fonts, slideNum, totalSlides);
                    break;
            }
        }

    }

    m_renderer = savedR;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, m_surface);
    return texture;
}

SDL_Texture* Renderer::renderPresenterView(const Presentation& pres, const FontSet& fonts) {
    const auto& s = pres.style;
    setStyle(&s);
    clear(s.bgColor.r, s.bgColor.g, s.bgColor.b);

    char numBuf[64];
    snprintf(numBuf, sizeof(numBuf), "Slide %d / %d", pres.current + 1, pres.size());

    const Slide& current = pres.currentSlide();
    FontVariants baseFonts = fonts.variants();
    FontVariants smallFonts = fonts.smallVariants();
    int cardPadding = std::max(8, s.partPadding / 2);
    ui::BorderStyle cardStyle;
    cardStyle.padding = ui::Thickness(cardPadding);
    cardStyle.background = s.codeBg;
    cardStyle.borderColor = s.lineColor;
    cardStyle.hasBackground = true;
    cardStyle.hasBorder = true;
    cardStyle.cornerRadius = s.presenterCornerRadius;

    auto root = std::make_unique<ui::Stack>();
    root->margin = ui::Thickness(s.presenterMargin);
    root->gap = std::max(8, s.partGap);

    auto header = std::make_unique<ui::Stack>();
    header->gap = std::max(2, s.linePadding / 2);
    header->add(std::make_unique<ui::Text>(
        numBuf, smallFonts, s.dimColor));
    auto title = std::make_unique<ui::Text>(
        current.title, baseFonts, s.titleColor);
    title->wrap = true;
    header->add(std::move(title));

    auto headerCard = std::make_unique<ui::Border>(
        std::move(header), cardStyle);
    root->add(std::move(headerCard));

    auto notes = std::make_unique<ui::Stack>();
    notes->gap = std::max(4, s.linePadding / 2);
    notes->add(std::make_unique<ui::Text>(
        "Notes", smallFonts, s.dimColor));
    auto noteText = std::make_unique<ui::Text>(
        current.notes.empty() ? "No notes for this slide." : current.notes,
        smallFonts,
        current.notes.empty() ? s.dimColor : s.textColor);
    noteText->wrap = true;
    notes->add(std::move(noteText), 1.0f);

    auto notesCard = std::make_unique<ui::Border>(
        std::move(notes), cardStyle);
    root->add(std::move(notesCard), 1.0f);

    if (pres.canGoNext()) {
        const Slide& next = pres.slides[pres.current + 1];
        auto nextSlide = std::make_unique<ui::Stack>();
        nextSlide->gap = std::max(2, s.linePadding / 2);
        nextSlide->add(std::make_unique<ui::Text>(
            "Next", smallFonts, s.dimColor));
        auto nextTitle = std::make_unique<ui::Text>(
            next.title, smallFonts, s.textColor);
        nextTitle->wrap = true;
        nextSlide->add(std::move(nextTitle));

        auto nextCard = std::make_unique<ui::Border>(
            std::move(nextSlide), cardStyle);
        root->add(std::move(nextCard));
    }

    ui::LayoutContext context{*this};
    root->measure(context, {m_width, m_height});
    root->arrange(context, {0, 0, m_width, m_height});
    root->render(context);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, m_surface);
    return texture;
}
