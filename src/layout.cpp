#include "layout.h"
#include "constants.h"

LayoutKind layoutFromType(SlideType type) {
    switch (type) {
        case SlideType::Title:      return LayoutKind::Title;
        case SlideType::Section:    return LayoutKind::Section;
        case SlideType::Content:    return LayoutKind::HeaderBody;
        case SlideType::TwoColumn:  return LayoutKind::HeaderTwoColumn;
        case SlideType::Image:      return LayoutKind::HeaderImage;
    }
    return LayoutKind::HeaderBody;
}

LayoutKind selectLayout(const Slide& slide) {
    if (slide.layoutSpecified) {
        return layoutFromType(slide.type);
    }

    bool hasLeftRight = !slide.leftContent.empty() || !slide.rightContent.empty();
    if (hasLeftRight) {
        return LayoutKind::HeaderTwoColumn;
    }

    bool hasImage = !slide.imagePath.empty();
    if (hasImage) {
        return LayoutKind::HeaderImage;
    }

    bool hasContentLines = !slide.bullets.empty();
    if (!hasContentLines && slide.subtitle.empty()) {
        return LayoutKind::Section;
    }

    return LayoutKind::HeaderBody;
}

std::vector<SlidePart> computeParts(LayoutKind kind, const Slide& slide, const LayoutMetrics& metrics) {
    std::vector<SlidePart> parts;
    int margin = SLIDE_MARGIN;
    int contentW = metrics.slideW - 2 * margin;

    switch (kind) {
        case LayoutKind::Title: {
            SlidePart full;
            full.role = PartRole::FullSlide;
            full.rect = {margin, 0, contentW, metrics.slideH};
            parts.push_back(full);
            break;
        }
        case LayoutKind::Section: {
            SlidePart full;
            full.role = PartRole::FullSlide;
            full.rect = {margin, 0, contentW, metrics.slideH};
            parts.push_back(full);
            break;
        }
        case LayoutKind::HeaderBody: {
            int headerH = static_cast<int>(metrics.titleLineH) + 2 * PART_PADDING;
            SlidePart header;
            header.role = PartRole::Header;
            header.rect = {margin, 0, contentW, headerH};
            parts.push_back(header);

            int bodyY = headerH + PART_GAP;
            int footerH = metrics.bodyLineH + PART_PADDING;
            int bodyH = metrics.slideH - bodyY - footerH - PART_PADDING;
            SlidePart body;
            body.role = PartRole::Body;
            body.rect = {margin, bodyY, contentW, bodyH};
            parts.push_back(body);

            SlidePart footer;
            footer.role = PartRole::Footer;
            footer.rect = {margin, metrics.slideH - footerH, contentW, footerH};
            parts.push_back(footer);
            break;
        }
        case LayoutKind::HeaderTwoColumn: {
            int headerH = static_cast<int>(metrics.titleLineH) + 2 * PART_PADDING;
            SlidePart header;
            header.role = PartRole::Header;
            header.rect = {margin, 0, contentW, headerH};
            parts.push_back(header);

            int bodyY = headerH + PART_GAP;
            int footerH = metrics.bodyLineH + PART_PADDING;
            int bodyH = metrics.slideH - bodyY - footerH - PART_PADDING;
            int colW = (contentW - COLUMN_GAP) / 2;

            SlidePart left;
            left.role = PartRole::ColumnLeft;
            left.rect = {margin, bodyY, colW, bodyH};
            parts.push_back(left);

            SlidePart right;
            right.role = PartRole::ColumnRight;
            right.rect = {margin + colW + COLUMN_GAP, bodyY, colW, bodyH};
            parts.push_back(right);

            SlidePart footer;
            footer.role = PartRole::Footer;
            footer.rect = {margin, metrics.slideH - footerH, contentW, footerH};
            parts.push_back(footer);
            break;
        }
        case LayoutKind::HeaderImage: {
            int headerH = static_cast<int>(metrics.titleLineH) + 2 * PART_PADDING;
            SlidePart header;
            header.role = PartRole::Header;
            header.rect = {margin, 0, contentW, headerH};
            parts.push_back(header);

            int footerH = metrics.bodyLineH + PART_PADDING;
            int captionH = static_cast<int>(metrics.bodyLineH) + PART_PADDING;
            int imageAreaY = headerH + PART_GAP;
            int imageAreaH = metrics.slideH - imageAreaY - captionH - footerH - 2 * PART_PADDING;

            SlidePart image;
            image.role = PartRole::Image;
            image.rect = {margin, imageAreaY, contentW, imageAreaH};
            parts.push_back(image);

            SlidePart caption;
            caption.role = PartRole::Caption;
            caption.rect = {margin, imageAreaY + imageAreaH + PART_GAP, contentW, captionH};
            parts.push_back(caption);

            SlidePart footer;
            footer.role = PartRole::Footer;
            footer.rect = {margin, metrics.slideH - footerH, contentW, footerH};
            parts.push_back(footer);
            break;
        }
    }

    return parts;
}