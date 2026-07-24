/**
 * presenter_api.h  —  flat C interface over the C++ presenter core.
 *
 * Both sides of the boundary use this header:
 *   • C++ (src/presenter_api.cpp) implements it.
 *   • Objective-C++ (Application/Application/PresenterCore.mm) calls it.
 *   • The standalone SDL2 executable (src/main.cpp) continues to work
 *     unchanged — it never includes this header.
 *
 * The API is intentionally minimal: lifecycle + navigation + presentation
 * metadata + rendering.  SDL2 renderer integration (audience window) lives in the
 * SDL-specific layer that calls presenter_render_slide_sdl / presenter_render_presenter_sdl.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Opaque session handle. */
typedef struct PresenterSession* PresenterHandle;

typedef struct PresenterPresentationInfo {
    const char* title;
    int slide_count;
    int current_slide_index;
    const char* current_slide_label;
    int can_go_next;
    int can_go_prev;
} PresenterPresentationInfo;

typedef struct PresenterSlideData {
    const char* title;
    const char* notes;
} PresenterSlideData;

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */

/**
 * Load a .slides file.  Returns NULL on failure.
 * cwd is the directory used as base for relative asset paths; pass NULL to
 * use the directory containing slides_path.
 */
PresenterHandle presenter_load(const char* slides_path, const char* cwd);

/** Free the session and all resources. */
void presenter_free(PresenterHandle h);

/* -------------------------------------------------------------------------
 * Navigation
 * ---------------------------------------------------------------------- */

int  presenter_count(PresenterHandle h);
int  presenter_current(PresenterHandle h);   /* 0-based */
int  presenter_can_go_next(PresenterHandle h);
int  presenter_can_go_prev(PresenterHandle h);
void presenter_next(PresenterHandle h);
void presenter_prev(PresenterHandle h);
void presenter_go_to(PresenterHandle h, int index);  /* 0-based */

/* -------------------------------------------------------------------------
 * Presentation metadata
 * ---------------------------------------------------------------------- */

/** Snapshot of presentation-level state. Pointers are valid until the session mutates. */
PresenterPresentationInfo presenter_presentation_info(PresenterHandle h);
/** Slide metadata by 0-based index. Pointers are valid until the session mutates. */
PresenterSlideData presenter_slide_info(PresenterHandle h, int index);
/** Current slide metadata. Pointers are valid until the session mutates. */
PresenterSlideData presenter_current_slide_info(PresenterHandle h);

/* Compatibility helpers for older call sites. Prefer the snapshot API above. */
const char* presenter_title(PresenterHandle h);
const char* presenter_notes(PresenterHandle h);
const char* presenter_slide_title(PresenterHandle h, int index);
const char* presenter_slide_label(PresenterHandle h);

/* -------------------------------------------------------------------------
 * Pixel-buffer rendering (no SDL window needed; used by SwiftUI)
 * ---------------------------------------------------------------------- */

/**
 * Render the current audience slide into a caller-supplied RGBA8888 buffer.
 * Slide layout is always computed on the fixed 1280x720 virtual canvas; the
 * completed raster is scaled to width x height when those dimensions differ.
 * Returns 0 on failure.
 */
int presenter_render_slide_rgba(PresenterHandle h,
                                uint8_t* pixels, int width, int height,
                                int stride);

/**
 * Render the presenter view (current + next + notes) into a caller-supplied
 * RGBA8888 buffer.  Returns 0 on failure.
 */
int presenter_render_presenter_rgba(PresenterHandle h,
                                    uint8_t* pixels, int width, int height,
                                    int stride);

/* -------------------------------------------------------------------------
 * SDL2-backed rendering (used by the standalone executable and optionally by
 * the macOS app for the audience window)
 * ---------------------------------------------------------------------- */

/**
 * Render the current audience slide into an SDL_Renderer*.
 * The renderer must already be initialised; w/h are the output viewport size.
 * Slide layout remains fixed at 1280x720 and the resulting texture is scaled
 * uniformly and centered in that viewport.
 * Returns 0 on failure.
 *
 * sdl_renderer is typed as void* so this header stays SDL-free when included
 * from Swift/ObjC translation units that don't import SDL2.
 */
int presenter_render_slide_sdl(PresenterHandle h,
                               void* sdl_renderer, int w, int height_px);

/**
 * Render the presenter view into an SDL_Renderer*.
 * Returns 0 on failure.
 */
int presenter_render_presenter_sdl(PresenterHandle h,
                                   void* sdl_renderer, int w, int height_px);

/* -------------------------------------------------------------------------
 * Theme cycling
 * ---------------------------------------------------------------------- */

int         presenter_theme_count(PresenterHandle h);
const char* presenter_theme_name(PresenterHandle h, int index);
int         presenter_current_theme(PresenterHandle h);
void        presenter_set_theme(PresenterHandle h, int index);

#ifdef __cplusplus
}
#endif
