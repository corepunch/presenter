# Presenter

A lightweight presentation renderer for AI agents. It reads XML slide definitions, lays out text and images, and renders them via SDL2 into two windows: an audience display and a presenter view with notes.

## Use Cases

**AI agents generating presentations** — the primary use case. An agent creates XML slide files from structured data (git history, project status, sprint summaries) and optionally captures screenshots of app states to embed as rich visual content. The result is a polished presentation combining text, images, and before/after comparisons.

Typical workflow:
1. Agent writes `scene.xml` with slides, images, and notes
2. Agent captures screenshots of the app: `./build/presenter scene.xml --screenshot=slides/scene.png`
3. Agent references screenshots in image slides for before/after demos
4. Presenter renders the final deck to screen or captures to file

## Features

- **XML slide format** with recursive composition — slides nest inside slides
- **6 layout types**: title, content, image, columns, section, blank
- **Dual-window output**: full audience screen + smaller presenter view with notes
- **Inline formatting**: `<b>bold</b>`, `<i>italic</i>`, `<code>code</code>` in text blocks
- **Image support**: PNG, JPG, JPEG, GIF, BMP with fit/fill scaling
- **Screenshot capture**: render slides to image files for embedding in other docs
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

## Screenshots

Capture a slide to an image file:

```bash
./build/presenter demo/demo.xml --screenshot=output/slide.png
```

This renders the current slide to a PNG and exits. Useful for:
- **Before/after demos** — capture app states and embed in image slides
- **Documentation** — generate slide images for READMEs or wikis
- **CI pipelines** — render presentations as part of automated reports

Example workflow for a sprint demo:

```bash
# 1. Generate the presentation XML
agent generate-sprint-demo --output=scenes/sprint23.xml

# 2. Capture key screenshots
./build/presenter scenes/sprint23.xml --screenshot=slides/sprint23_title.png

# 3. Present live
./build/presenter scenes/sprint23.xml
```

## Slide Format

Presentations are XML files with a `<presentation>` root containing `<slide>` children.

- **Full format reference**: [skills/presentation.md](https://corepunch.github.io/presenter/skills/presentation.md) — detailed guide with examples for all layouts
- **DTD schemas**: [schemas/presentation.dtd](https://corepunch.github.io/presenter/schemas/presentation.dtd) and [schemas/style.dtd](https://corepunch.github.io/presenter/schemas/style.dtd) — formal XML validation

### DOCTYPE Declaration

Include the DTD in your presentation files for validation:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/schemas/presentation.dtd">
<presentation>
  ...
</presentation>
```

### Quick Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/schemas/presentation.dtd">
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

When generating presentations, refer to the [Slide Format Reference](https://corepunch.github.io/presenter/skills/presentation.md) for the complete specification. Always include the DOCTYPE declaration pointing to `https://corepunch.github.io/presenter/schemas/presentation.dtd`.

Recommended workflow:
1. Write the XML slide file following the format spec
2. Capture screenshots with `--screenshot` flag
3. Reference screenshots in `<image>` elements with relative paths
4. Include `<notes>` for presenter context (talking points, metrics, transitions)
5. Present live or render to images

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

Style files use the format defined in [schemas/style.dtd](https://corepunch.github.io/presenter/schemas/style.dtd):

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
