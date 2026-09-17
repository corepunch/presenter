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
#include <sstream>

Renderer::~Renderer() {
    cleanup();
}

void Renderer::setPixelRatio(float ratio) {
    if (!(ratio >= MIN_PIXEL_RATIO && ratio <= MAX_PIXEL_RATIO)) {
        ratio = std::clamp(ratio, MIN_PIXEL_RATIO, MAX_PIXEL_RATIO);
    }
    m_pixelRatio = ratio;
}

int Renderer::deviceWidth() const {
    return static_cast<int>(std::lround(m_width * m_pixelRatio));
}

int Renderer::deviceHeight() const {
    return static_cast<int>(std::lround(m_height * m_pixelRatio));
}

int Renderer::toDevice(int v) const {
    return static_cast<int>(std::lround(v * m_pixelRatio));
}

float Renderer::toDevice(float v) const {
    return v * m_pixelRatio;
}

SDL_Rect Renderer::toDeviceRect(const SDL_Rect& r) const {
    return toDeviceRect(r.x, r.y, r.w, r.h);
}

SDL_Rect Renderer::toDeviceRect(int x, int y, int w, int h) const {
    return {toDevice(x), toDevice(y), std::max(0, toDevice(w)), std::max(0, toDevice(h))};
}

bool Renderer::init(SDL_Renderer* renderer, int width, int height) {
    cleanup();
    m_renderer = renderer;
    m_width = width;
    m_height = height;
    m_surface = SDL_CreateRGBSurface(0, deviceWidth(), deviceHeight(), 32,
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
    if (x < surf->clip_rect.x || x >= surf->clip_rect.x + surf->clip_rect.w ||
        y < surf->clip_rect.y || y >= surf->clip_rect.y + surf->clip_rect.h) return;
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

static void maskImageCorners(SDL_Surface* surf, SDL_Rect r, int radius) {
    int rad = std::min(radius, std::min(r.w, r.h) / 2);
    if (rad <= 0) return;

    for (int y = 0; y < rad; ++y) {
        for (int x = 0; x < rad; ++x) {
            float coverage = roundedRectCoverage(r.w, r.h, rad, x, y);
            auto mask = [&](int px, int py) {
                auto* pixels = static_cast<Uint32*>(surf->pixels);
                Uint32& pixel = pixels[py * (surf->pitch / 4) + px];
                Uint8 red, green, blue, alpha;
                SDL_GetRGBA(pixel, surf->format, &red, &green, &blue, &alpha);
                pixel = SDL_MapRGBA(surf->format, red, green, blue,
                    static_cast<Uint8>(std::lround(alpha * coverage)));
            };
            mask(r.x + x, r.y + y);
            mask(r.x + r.w - 1 - x, r.y + y);
            mask(r.x + x, r.y + r.h - 1 - y);
            mask(r.x + r.w - 1 - x, r.y + r.h - 1 - y);
        }
    }
}

void Renderer::drawText(const std::string& text, float x, float y,
                        const Font& font, SDL_Color color) {
    if (text.empty() || !m_surface) return;

    // Logical coordinates in, device pixels out: glyph bitmaps are generated
    // at fontSize*pixelRatio so coverage stays sharp on retina surfaces.
    float curX = toDevice(x);
    float deviceY = toDevice(y);
    float ratio = m_pixelRatio;
    utf8_int32_t prev = 0;
    const utf8_int8_t* s = reinterpret_cast<const utf8_int8_t*>(text.c_str());
    while (*s) {
        utf8_int32_t cp = 0;
        s = utf8codepoint(s, &cp);

        if (prev) curX += font.getKerning(static_cast<uint32_t>(prev), static_cast<uint32_t>(cp)) * ratio;
        curX += font.drawGlyph(m_surface, static_cast<uint32_t>(cp), curX, deviceY, color, ratio);

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
    SDL_Color codeColor = style().accentColor.toSDLColor();
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
    fillRoundedRect(m_surface, toDeviceRect(rect), toDevice(cornerRadius),
        color.toUint32(m_surface->format));
}

void Renderer::drawRectOutline(const SDL_Rect& rect, Color color, int cornerRadius) {
    if (!m_surface || rect.w <= 0 || rect.h <= 0) return;
    drawRoundedRectOutline(
        m_surface, toDeviceRect(rect), toDevice(cornerRadius),
        color.toUint32(m_surface->format));
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
    auto reopen = [](const TextFormat& f) {
        return std::string(f.bold ? "<b>" : "") + (f.italic ? "<i>" : "") + (f.code ? "<code>" : "");
    };
    auto close = [](const TextFormat& f) {
        return std::string(f.code ? "</code>" : "") + (f.italic ? "</i>" : "") + (f.bold ? "</b>" : "");
    };

    for (size_t i = 0; i <= text.size(); i++) {
        char c = (i < text.size()) ? text[i] : ' ';
        if (c == '<') {
            size_t length = 0; int kind = 0; bool open = false;
            if (findMarker(text, i, &length, &kind, &open) == i) {
                word += text.substr(i, length);
                i += length - 1;
                continue;
            }
        }

        if (c == ' ' || c == '\n' || i == text.size()) {
            if (!word.empty()) {
                TextFormat beforeWord = fmt;
                float wordWidth = measureFormatted(fonts, word, fmt);
                TextFormat fmtCopy = fmt;
                float spaceW = line.empty() ? 0 : measureFormatted(fonts, " ", fmtCopy);

                if (lineWidth + spaceW + wordWidth > maxWidth && !line.empty()) {
                    lines.push_back(line + close(beforeWord));
                    line = reopen(beforeWord) + word;
                    lineWidth = wordWidth;
                } else {
                    if (!line.empty()) line += " ";
                    line += word;
                    lineWidth += spaceW + wordWidth;
                }
                word.clear();
            }

            if (c == '\n') {
                lines.push_back(line + close(fmt));
                line = reopen(fmt);
                lineWidth = 0;
            }
        } else {
            word += c;
        }
    }

    if (!line.empty()) {
        lines.push_back(line + close(fmt));
    }

    return lines;
}

float Renderer::formattedWidth(const std::string& text, const FontVariants& fonts) {
    TextFormat format;
    return measureFormatted(fonts, text, format);
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
    fillRoundedRect(surf, r->toDeviceRect(bgRect), r->toDevice(s.cornerRadius), bg);

    Uint32 brd = SDL_MapRGBA(surf->format, s.codeBorder.r, s.codeBorder.g, s.codeBorder.b, s.codeBorder.a);
    drawRoundedRectOutline(surf, r->toDeviceRect(bgRect), r->toDevice(s.cornerRadius), brd);

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

static void renderImageAt(Renderer* renderer, SDL_Surface* surf, const std::string& imagePath,
                          const ui::Rect& rect, ImageFit fit,
                          int cornerRadius, Color placeholderBg) {
    if (imagePath.empty()) return;

    int imgW = 0, imgH = 0, channels = 0;
    unsigned char* data = stbi_load(imagePath.c_str(), &imgW, &imgH, &channels, 4);

    if (data && imgW > 0 && imgH > 0) {
        ImagePlacement placement = placeImage(imgW, imgH,
            {rect.x, rect.y, rect.width, rect.height}, fit == ImageFit::Fill);
        const auto& crop = placement.source;
        const auto& destination = placement.destination;
        // Resample to device pixels so retina displays use full source detail
        // instead of magnifying a 1x bitmap. Layout stays in logical units.
        float ratio = renderer ? renderer->pixelRatio() : 1.0f;
        int dstW = std::max(1, static_cast<int>(std::lround(destination.w * ratio)));
        int dstH = std::max(1, static_cast<int>(std::lround(destination.h * ratio)));
        int dstX = static_cast<int>(std::lround(destination.x * ratio));
        int dstY = static_cast<int>(std::lround(destination.y * ratio));
        int devRadius = static_cast<int>(std::lround(cornerRadius * ratio));
        ImageBuf srcBuf{data, imgW, imgH};
        if (fit == ImageFit::Fill) {
            srcBuf = {new uint8_t[crop.w * crop.h * 4], crop.w, crop.h};
            for (int row = 0; row < crop.h; ++row)
                std::memcpy(srcBuf.data + row * crop.w * 4,
                            data + ((crop.y + row) * imgW + crop.x) * 4,
                            crop.w * 4);
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
                // Mask the image alpha before compositing so rounded corners
                // reveal the actual parent surface, including colored cards.
                maskImageCorners(imgSurface, {0, 0, dstW, dstH}, devRadius);
                SDL_SetSurfaceBlendMode(imgSurface, SDL_BLENDMODE_BLEND);
                SDL_BlitSurface(imgSurface, nullptr, surf, &dstRect);
                SDL_FreeSurface(imgSurface);
            }
            delete[] resampled.data;
        }

        stbi_image_free(data);
    } else {
        SDL_Rect ph = renderer ? renderer->toDeviceRect(rect.x, rect.y, rect.width, rect.height)
                               : SDL_Rect{rect.x, rect.y, rect.width, rect.height};
        int phRadius = renderer ? renderer->toDevice(cornerRadius) : cornerRadius;
        fillRoundedRect(surf, ph, phRadius,
            placeholderBg.toUint32(surf->format));
    }
}

namespace {
std::string attr(const LayoutNode& node, const char* key, const std::string& fallback = "") {
    auto it = node.attributes.find(key);
    return it == node.attributes.end() ? fallback : it->second;
}

class VisualLeaf final : public ui::Element {
public:
    VisualLeaf(const LayoutNode& node, const FontSet& fonts) : node(node), fonts(fonts) {}
protected:
    ui::Size measureOverride(ui::LayoutContext& context, ui::Size available) override {
        auto& r = context.renderer;
        if (node.kind == "image") {
            int w = 0, h = 0, channels = 0;
            stbi_info(attr(node, "src").c_str(), &w, &h, &channels);
            if (w <= 0 || h <= 0) return {240, 160};
            double scale = std::min({1.0, double(available.width) / w, double(available.height) / h});
            return {static_cast<int>(w * scale), static_cast<int>(h * scale)};
        }
        if (node.kind == "chart") return {std::min(480, available.width), node.chart.height};
        if (node.kind == "icon") {
            int pad = std::max(8, r.style().partPadding / 2);
            int marker = static_cast<int>(fonts.icons().getFontSize()) + 3 * pad;
            int textWidth = std::max(1, available.width - marker);
            auto lines = r.wordWrap(node.text, fonts.variants(), textWidth);
            int w = 0;
            for (const auto& line : lines)
                w = std::max(w, static_cast<int>(std::ceil(r.formattedWidth(line, fonts.variants()))));
            return {w + marker, std::max(iconBlockNaturalHeight(&r, fonts),
                static_cast<int>(lines.size()) * r.textHeight(fonts.get(FontType::Regular)) + 2 * pad)};
        }
        const auto& font = fonts.get(FontType::Monospace);
        std::istringstream lines(node.text);
        std::string line; int w = 0, count = 0;
        while (std::getline(lines, line)) {
            ++count;
            w = std::max(w, static_cast<int>(std::ceil(font.measureString(line))));
        }
        int padding = 2 * std::max(8, r.style().partPadding / 2);
        return {w + padding, std::max(1, count) *
            (static_cast<int>(font.getAscent() - font.getDescent()) + r.style().linePadding) + padding};
    }
    void renderOverride(ui::LayoutContext& context) override {
        auto& r = context.renderer;
        const auto& b = bounds();
        if (node.kind == "chart") renderChart(&r, r.surface(), node.chart, fonts, b.x, b.y, b.width, b.height);
        else if (node.kind == "icon") renderIconBlock(&r, r.surface(), {attr(node, "name"), node.text}, fonts, b.x, b.y, b.width, b.height);
        else if (node.kind == "image") renderImageAt(&r, r.surface(), attr(node, "src"), b,
            attr(node, "fit") == "fill" ? ImageFit::Fill : ImageFit::Fit,
            r.style().imageCornerRadius, r.style().codeBg);
        else renderCodeBlock(&r, r.surface(), {node.text, attr(node, "lang")},
            fonts.get(FontType::Monospace), b.x, b.y, b.width, b.height);
    }
private:
    const LayoutNode& node;
    const FontSet& fonts;
};
}

std::unique_ptr<ui::Element> createVisualLeaf(const LayoutNode& node, const FontSet& fonts) {
    return std::make_unique<VisualLeaf>(node, fonts);
}

SDL_Texture* Renderer::renderSlide(const Slide& slide, const FontSet& fonts,
        const PresentationStyle& style, int, int) {
    setStyle(&style);
    SDL_Renderer* saved = m_renderer;
    m_renderer = nullptr;
    fillRect({0, 0, m_width, m_height}, style.bgColor);
    m_layoutOverflows = 0;
    if (!slide.elements.empty()) {
        auto root = buildSlideLayout(slide.elements.front(), fonts, style);
        ui::LayoutContext context{*this};
        root->measure(context, {m_width, m_height});
        root->arrange(context, {0, 0, m_width, m_height});
        root->render(context);
        m_layoutOverflows = context.overflowCount;
    }
    m_renderer = saved;
    return m_renderer ? SDL_CreateTextureFromSurface(m_renderer, m_surface) : nullptr;
}

SDL_Texture* Renderer::renderPresenterView(const Presentation& pres, const FontSet& fonts) {
    const auto& s = pres.style;
    setStyle(&s);
    fillRect({0, 0, m_width, m_height}, s.bgColor);

    // Notes use the full text width. Only the next-slide cue has a top gap.
    auto stack = std::make_unique<ui::Stack>();
    auto addText = [&](const std::string& value, FontVariants faces, Color color) {
        if (value.empty()) return;
        auto text = std::make_unique<ui::Text>(value, faces, color);
        text->wrap = true;
        stack->add(std::move(text));
    };
    if (!pres.empty()) {
        const Slide& current = pres.currentSlide();
        addText(current.title, fonts.variants(), s.titleColor);
        addText(current.notes, fonts.smallVariants(), s.textColor);
        if (pres.canGoNext()) {
            auto next = std::make_unique<ui::Text>(
                "Next: " + pres.slides[pres.current + 1].title,
                fonts.smallVariants(), s.presenterNextColor);
            next->wrap = true;
            next->margin.top = 12;
            stack->add(std::move(next));
        }
    }

    ui::BorderStyle padding;
    padding.padding = ui::Thickness(std::max(0, s.presenterMargin));
    ui::Border root(std::move(stack), padding);
    ui::LayoutContext context{*this};
    root.measure(context, {m_width, m_height});
    root.arrange(context, {0, 0, m_width, m_height});
    root.render(context);
    m_layoutOverflows = context.overflowCount;

    return m_renderer ? SDL_CreateTextureFromSurface(m_renderer, m_surface) : nullptr;
}
