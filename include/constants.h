#pragma once

// Font sizes (pixels). Base size can be overridden with --size.
#define FONT_SIZE_BASE 28.0f

// Slide titles (# headings) and ## / ### sub-headings render at this
// multiple of the base font size.
#define FONT_SIZE_TITLE_SCALE 2.0f

// Per-role font sizes (pixels)
#define FONT_TITLE_SIZE 72.0f
#define FONT_SUBTITLE_SIZE 48.0f
#define FONT_CONTENT_SIZE 32.0f
#define FONT_BULLET_SIZE 48.0f
#define FONT_SMALL_SIZE 24.0f
#define FONT_CHILD_TITLE_SIZE 48.0f

// Presenter-view secondary text renders at this multiple of the base size.
#define FONT_SIZE_SMALL_SCALE 0.7f

// Layout (pixels)
#define SLIDE_MARGIN 40
#define PRESENTER_MARGIN 20
#define LINE_PADDING 6

// Part-based layout constants
#define PART_PADDING 20
#define PART_GAP 24
#define COLUMN_GAP 24
#define BULLET_GAP 24
#define CORNER_RADIUS 24
#define PRESENTER_CORNER_RADIUS (CORNER_RADIUS / 2)
