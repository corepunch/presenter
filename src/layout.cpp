#include "layout.h"
#include <algorithm>

LayoutKind layoutFromSlide(const Slide& slide) {
    switch (slide.layout) {
        case SlideLayout::Title:   return LayoutKind::Title;
        case SlideLayout::Section: return LayoutKind::Section;
        case SlideLayout::Columns: return LayoutKind::Columns;
        case SlideLayout::Image:   return LayoutKind::HeaderImage;
        case SlideLayout::Blank:   return LayoutKind::HeaderBody;
        case SlideLayout::Content:
        default:
            if (!slide.imagePath.empty()) return LayoutKind::HeaderImage;
            return LayoutKind::HeaderBody;
    }
}

std::vector<SlidePart> computeParts(LayoutKind kind, const Slide& slide,
                                    const LayoutMetrics& metrics,
                                    const PresentationStyle& s) {
    std::vector<SlidePart> parts;
    int contentW = metrics.slideW - 2 * s.slideMargin;

    switch (kind) {
        case LayoutKind::Title:
        case LayoutKind::Section: {
            SlidePart full;
            full.role = PartRole::FullSlide;
            full.rect = {s.slideMargin, 0, contentW, metrics.slideH};
            parts.push_back(full);
            break;
        }
        case LayoutKind::HeaderBody: {
            int headerH = static_cast<int>(metrics.titleLineH) + 2 * s.partPadding;
            SlidePart header;
            header.role = PartRole::Header;
            header.rect = {s.slideMargin, 0, contentW, headerH};
            parts.push_back(header);

            int bodyY = headerH + s.partGap;
            int footerH = static_cast<int>(metrics.bodyLineH) + s.partPadding;
            int bodyH = metrics.slideH - bodyY - footerH - s.partPadding;
            SlidePart body;
            body.role = PartRole::Body;
            body.rect = {s.slideMargin, bodyY, contentW, bodyH};
            parts.push_back(body);

            SlidePart footer;
            footer.role = PartRole::Footer;
            footer.rect = {s.slideMargin, metrics.slideH - footerH, contentW, footerH};
            parts.push_back(footer);
            break;
        }
        case LayoutKind::Columns: {
            int headerH = static_cast<int>(metrics.titleLineH) + 2 * s.partPadding;
            SlidePart header;
            header.role = PartRole::Header;
            header.rect = {s.slideMargin, 0, contentW, headerH};
            parts.push_back(header);

            int bodyY = headerH + s.partGap;
            int footerH = static_cast<int>(metrics.bodyLineH) + s.partPadding;
            int bodyH = metrics.slideH - bodyY - footerH - s.partPadding;

            int nCols = std::max(1, slide.cols);
            int gap = slide.gap > 0 ? slide.gap : s.columnGap;
            int nSlots = static_cast<int>(slide.children.size());
            int nRows = (nSlots + nCols - 1) / nCols;
            int nGapsH = nCols - 1;
            int nGapsV = nRows - 1;
            int colW = (contentW - nGapsH * gap) / nCols;
            int rowH = (bodyH - nGapsV * gap) / std::max(1, nRows);

            for (int i = 0; i < nSlots; i++) {
                int col = i % nCols;
                int row = i / nCols;
                SlidePart slot;
                slot.role = PartRole::Slot;
                slot.childIndex = i;
                slot.rect = {
                    s.slideMargin + col * (colW + gap),
                    bodyY + row * (rowH + gap),
                    colW,
                    rowH
                };
                parts.push_back(slot);
            }

            SlidePart footer;
            footer.role = PartRole::Footer;
            footer.rect = {s.slideMargin, metrics.slideH - footerH, contentW, footerH};
            parts.push_back(footer);
            break;
        }
        case LayoutKind::HeaderImage: {
            int headerH = slide.title.empty() ? 0 : static_cast<int>(metrics.titleLineH) + 2 * s.partPadding;
            int gapAbove = headerH > 0 ? s.partGap : 0;
            if (!slide.title.empty()) {
                SlidePart header;
                header.role = PartRole::Header;
                header.rect = {s.slideMargin, 0, contentW, headerH};
                parts.push_back(header);
            }

            int footerH = static_cast<int>(metrics.bodyLineH) + s.partPadding;
            int captionH = static_cast<int>(metrics.bodyLineH) + s.partPadding;
            int imageAreaY = headerH + gapAbove;
            int imageAreaH = metrics.slideH - imageAreaY - s.partGap - captionH - footerH;

            SlidePart image;
            image.role = PartRole::Image;
            image.rect = {s.slideMargin, imageAreaY, contentW, imageAreaH};
            parts.push_back(image);

            SlidePart caption;
            caption.role = PartRole::Caption;
            caption.rect = {s.slideMargin, imageAreaY + imageAreaH + s.partGap, contentW, captionH};
            parts.push_back(caption);

            SlidePart footer;
            footer.role = PartRole::Footer;
            footer.rect = {s.slideMargin, metrics.slideH - footerH, contentW, footerH};
            parts.push_back(footer);
            break;
        }
    }

    return parts;
}
