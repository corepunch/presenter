# Presenter

A lightweight presentation tool that renders slides from XML definitions with dual-window support: an audience display and a presenter view.

## Features

- **XML slide format** with recursive composition — slides nest inside slides
- **6 layout types**: title, content, image, columns, section, blank
- **Dual-window output**: full audience screen + smaller presenter view with notes
- **Inline formatting**: `**bold**`, `_italic_`, `` `code` `` in text blocks
- **Image support**: PNG, JPG, JPEG, GIF, BMP with fit/fill scaling
- **Themeable**: XML style files for colors, fonts, and spacing
- **Keyboard controls**: arrow keys, space/enter to navigate, F5 fullscreen, Escape to quit

## Prerequisites

- C++17 compiler
- CMake >= 3.10
- SDL2
- tinyxml2

### macOS (Homebrew)

```bash
brew install cmake sdl2 tinyxml2
```

### Ubuntu/Debian

```bash
sudo apt install cmake libsdl2-dev libtinyxml2-dev
```

## Build

```bash
cmake -B build
cmake --build build
```

## Run

```bash
./build/presenter demo/demo.xml
```

Override the theme at runtime:

```bash
./build/presenter demo/demo.xml --style demo/styles/light.xml
```

### Controls

| Key | Action |
|-----|--------|
| Right / Space / Enter | Next slide |
| Left / Backspace | Previous slide |
| Home | First slide |
| End | Last slide |
| F5 | Toggle audience fullscreen |
| Escape | Quit |

## Slide Format

Presentations are XML files with a `<presentation>` root containing `<slide>` children.

- **Full format reference**: [docs/SLIDE_FORMAT.md](https://corepunch.github.io/presenter/docs/SLIDE_FORMAT.html) — detailed guide with examples for all layouts
- **DTD schema**: [docs/presentation.dtd](https://corepunch.github.io/presenter/docs/presentation.dtd) — formal XML validation

### DOCTYPE Declaration

Include the DTD in your presentation files for validation:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/docs/presentation.dtd">
<presentation>
  ...
</presentation>
```

### Quick Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/docs/presentation.dtd">
<presentation>
  <slide layout="title">
    <title>My Talk</title>
    <subtitle>Subtitle here</subtitle>
  </slide>

  <slide layout="content">
    <title>Key Points</title>
    <text><b>Bold</b> point one</text>
    <text>Point two</text>
    <text>Point three</text>
  </slide>
</presentation>
```

### For AI Agents

When generating presentations, refer to the [Slide Format Reference](https://corepunch.github.io/presenter/docs/SLIDE_FORMAT.html) for the complete specification. Always include the DOCTYPE declaration pointing to `https://corepunch.github.io/presenter/docs/presentation.dtd`.

## Layouts

| Layout | Description |
|--------|-------------|
| `title` | Centered title + optional subtitle (opening/closing slides) |
| `content` | Title + vertical stack of text blocks (default) |
| `image` | Title + full-width image + caption |
| `columns` | Title + horizontal slots via child slides with `slot` attribute |
| `section` | Centered title only (divider) |
| `blank` | No header, children fill full area |

## Theming

Style files use the format defined in `docs/style.dtd`:

```xml
<style>
  <colors bg="#1E1E28" text="#C8C8D2" accent="#FFCC00"/>
  <fonts title="48" content="28"/>
  <layout margin="40" gap="24"/>
</style>
```

Reference a style from the presentation:

```xml
<presentation style="./styles/dark.xml">
```

## Project Structure

```
presenter/
├── src/            # Source files
├── include/        # Headers
├── demo/           # Example presentation and styles
├── docs/           # DTD schemas and format spec
├── test/           # Test executables
├── assets/         # Bundled fonts (Inter, JetBrains Mono)
├── examples/       # Markdown-format demo
└── CMakeLists.txt
```

## Testing

```bash
cmake --build build
./build/test_textbounds
./build/test_layout
./build/test_xml_parser
./build/test_image
```

## License

See repository for license details.
