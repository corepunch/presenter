# AGENTS.md

Instructions for AI agents working on this codebase.

## Project Overview

**Presenter** is a C++17 presentation renderer. It reads XML slide definitions, lays out text and images, and renders them via SDL2 into two windows: an audience display and a presenter view with notes.

## Build & Test Commands

```bash
# Build
cmake -B build && cmake --build build

# Run
./build/presenter demo/demo.xml

# Tests
./build/test_textbounds
./build/test_layout
./build/test_xml_parser
./build/test_image

# Lint (if clang-tidy is available)
clang-tidy src/*.cpp -- -Iinclude -Ithird_party $(pkg-config --cflags sdl2 tinyxml2)
```

## Architecture

### Source Layout

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point. Parses CLI args, creates SDL windows, runs event loop |
| `src/xml_parser.cpp` | Parses XML into `Presentation`/`Slide` structs |
| `src/style.cpp` | Loads theme XML into `PresentationStyle` |
| `src/font.cpp` | TTF font loading via SDL_ttf-style API (bundled Inter + JetBrains Mono) |
| `src/renderer.cpp` | SDL2 rendering — slides, presenter view, text layout |
| `src/layout.cpp` | Recursive layout engine: sizes slides, positions children |
| `src/image.cpp` | Image loading (SDL_image) and scaling (fit/fill) |
| `include/common.h` | Core types: `Slide`, `Presentation`, `SlideLayout`, `ImageFit` enums |
| `include/style.h` | `PresentationStyle` and `Color` structs |
| `include/constants.h` | Compile-time layout constants (margins, font scales) |

### Data Flow

1. `xml_parser.cpp` reads XML → fills `Presentation` with `Slide` tree
2. `style.cpp` loads theme → `PresentationStyle` (colors, font sizes, spacing)
3. `font.cpp` loads TTF fonts at sizes from style
4. `layout.cpp` computes bounding boxes recursively (no rendering)
5. `renderer.cpp` walks laid-out tree, draws to SDL textures
6. `main.cpp` presents textures to both windows

### Key Types

- `Presentation` — owns slide vector, current index, style
- `Slide` — recursive: has `children` vector for column layouts
- `SlideLayout` enum: Title, Content, Image, Columns, Section, Blank
- `ImageFit` enum: Fit (contain) or Fill (cover/crop)

## Conventions

- C++17, no exceptions, no RTTI
- No external deps beyond SDL2 and tinyxml2
- Headers use `#pragma once`
- Enum classes for type safety
- Build artifacts in `build/` (gitignored)
- Fonts bundled in `assets/` (Inter family + JetBrains Mono)

## Common Tasks

### Adding a new layout type

1. Add enum value to `SlideLayout` in `include/common.h`
2. Add layout handling in `src/layout.cpp` (compute bounds)
3. Add rendering in `src/renderer.cpp` (draw to SDL)
4. Add DTD attribute value in `docs/presentation.dtd`
5. Test with a sample XML slide

### Modifying slide styles

Edit `PresentationStyle` defaults in `include/style.h`, or create/modify theme XML in `demo/styles/`.

### Adding image formats

Extend `src/image.cpp` — currently supports PNG, JPG, GIF, BMP via SDL_image.
