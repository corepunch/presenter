# Presenter: manual layout format and agent workflow

Schema: [presentation.dtd](../schemas/presentation.dtd).
Theme schema: [style.dtd](../schemas/style.dtd).
Theme guide: [themes](../themes.md).

## Breaking format change

Slides now use explicit visual trees. Preset attributes such as
`layout="title"`, `cols`, and `slot`, nested slides, and `subtitle` elements
are rejected. There is no compatibility mode. Use the current build for
these files; v1.2.0 and earlier do not support this format.

A slide contains optional plain-text notes followed by exactly one
`stack`, `grid`, or `border`. Its `title` attribute is presenter metadata;
to display a heading, add a text element. No header, bullet, footer, margin,
or slide number is inserted automatically. XML order is preserved.

## Research and story

Before writing XML, collect the audience, duration, objective, evidence,
source links, candidate visuals, assumptions, and slide outline in
`presentation-source.md`. Distinguish verified facts from inference.
Never invent measurements, quotations, or sources.

Give each slide one takeaway. Organize the story as context → claim →
evidence → implication → decision. Use concise visible text and expand
the meaning in notes. Read the headings in sequence to check the argument.

Write ready-to-say notes for every slide: a natural opening, relevant
context and evidence, a brief visual cue when useful, and a transition.
Notes are plain text and private to Presenter View. Review the notes
window too; split or shorten scripts that do not fit.

## Portable package

Create `My Talk.slides/presentation.xml`, with images and styles inside
the same directory. Asset paths are relative to presentation.xml. A
single XML file can also be opened, but it must use the new layout format.
Always include the XML declaration and DOCTYPE:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/schemas/presentation.dtd">
<presentation name="Project review">
  <style theme="Studio"/>
  <slide title="The next milestone">
    <notes>Today we will agree on the next milestone and its owner.</notes>
    <stack margin="60" gap="24" verticalAlignment="center">
      <text role="title">The next milestone</text>
      <text role="subtitle" color="muted">One decision. A clear next step.</text>
    </stack>
  </slide>
</presentation>
```

## Layout contract

All audience layouts use a 1280 × 720 logical canvas. Window resizing
scales the completed image, not individual text or layout rules.

1. `measure(availableSize)` applies constraints and margins, calls
   `measureOverride`, and records the desired size.
2. Containers measure their children. Grids resolve column widths before
   measuring wrapped text heights.
3. `arrange(finalRect)` applies alignment and calls `arrangeOverride`,
   assigning each child a rectangle.
4. Render uses those rectangles and intersects parent/child clips.

Text and code do not shrink to fit. Overflow is clipped and reported to
stderr as `[layout] overflow`. Long unbreakable words can overflow: shorten
them, add a break, or allocate more width. Keep content within the available
space; clipping is a diagnostic, not a design technique.

### Shared properties

| Property | Default | Meaning |
|---|---|---|
| width, height | intrinsic/stretch | Explicit size in pixels |
| minWidth, minHeight | 0 | Lower desired-size constraints |
| maxWidth, maxHeight | unbounded | Upper size constraints |
| margin | 0 | Space outside the element |
| horizontalAlignment, verticalAlignment | stretch | stretch, start, center, end |
| row, column | 0 | Zero-based position in a parent grid |
| rowSpan, columnSpan | 1 | Number of grid tracks occupied |

Lengths are nonnegative integers. Margins and padding accept one number
(all sides), two numbers (horizontal vertical), or four numbers
(left top right bottom). Explicit sizes exclude margin. An explicit size
prevents stretch along that axis. An element cannot draw outside its parent.

### Stack

`<stack orientation="vertical" gap="20">` puts children in declaration
order. Orientation may be horizontal. The default gap is 0.

A stack measures children without a limit along its stacking axis, sums
their desired sizes plus gaps, and uses the largest cross-axis size.
It does not wrap, distribute leftover space, or shrink children.
For wrapping text in a horizontal stack, give it a width; for responsive
columns, use a grid. A centered stack needs
`verticalAlignment="center"` (or horizontalAlignment for horizontal centering).

### Grid

`<grid rows="auto * auto" columns="2* *" gap="24">` declares tracks.
Rows and columns default to a single `*` track. Entries are separated by
spaces, not commas.

| Track | Meaning |
|---|---|
| 120 | Exactly 120 pixels |
| auto | Measured content size |
| * | One share of remaining space |
| 2* | Two shares of remaining space |

Subtract gaps, fixed tracks, and auto tracks first; divide the remainder
by star weights. Integer rounding preserves the total available extent.
With a 1200-pixel grid, gap 24, and columns `2* *`, columns are 784 and
392 pixels. In an unbounded stack axis, star tracks size to content.
Auto columns measure intrinsic width, so use star columns for wrapping
paragraphs. Auto rows measure height after column widths are known.

Specify each child's row/column explicitly when needed. Children default
to cell (0,0); they do not auto-flow. Children sharing a cell overlap in
declaration order. Spans include intervening gaps. Invalid cells and spans
are rejected. Avoid overlapping content unless it is intentional.

```xml
<slide title="Evidence and implication">
  <notes>The chart establishes the trend. The right column explains the next action.</notes>
  <grid margin="40" rows="auto * auto" columns="2* *" gap="24">
    <text role="heading" columnSpan="2">Evidence and implication</text>
    <chart row="1" type="bar" title="Illustrative milestones">
      <point label="Plan" value="24"/>
      <point label="Build" value="72"/>
    </chart>
    <border row="1" column="1" padding="24" background="panel"
            borderColor="line" cornerRadius="20">
      <stack gap="16" verticalAlignment="center">
        <text role="heading">Next step</text>
        <text>Agree on the owner and review date.</text>
      </stack>
    </border>
    <text row="2" columnSpan="2" role="small" color="muted">Illustrative data</text>
  </grid>
</slide>
```

### Border

`border` contains exactly one element. It adds explicit `padding`,
optional `background`, optional `borderColor`, and `cornerRadius`
(default 0). Use a stack or grid inside it for multiple children.

Colors accept `#rrggbb` or theme names: `background`, `panel`,
`text`, `title`, `muted`, `accent`, `line`.

## Leaves

- `text`: formatted text. Roles `title`, `subtitle`, `heading`, `body`
  (default), and `small` select theme font sizes. `wrap="true"` by default;
  `textAlignment="start|center|end"` aligns lines inside the element.
  `color` uses the palette above. Use `<b>`, `<i>`, and `<code>`, not
  Markdown formatting. There are no automatic bullets.
- `image`: required `src`, optional `alt`, `fit="fit|fill"`.
  Fit preserves the entire image and aligns it to the bottom of the
  allocated rectangle; fill crops centrally. PNG, JPG, GIF and BMP work.
  Put an image in a star grid row and a caption in an auto row.
  Images are square by default. A theme can opt into rounded corners with
  `<layout imageCornerRadius="20"/>` inside its `style` element; this is
  independent of code/chart card corner radii.
- `code`: preformatted text, optional `lang` for syntax highlighting.
  Code does not wrap or shrink. Escape XML characters.
- `chart`: `type="bar|line|pie|donut"`, optional `title`, `icon`,
  `showValues="true|false"`; children are `point` with `label` and a
  finite numeric `value`. Natural height is 340; use grid space or height
  to choose a different size.
- `icon name="rocket"`: a short formatted label with a bundled Font
  Awesome Free Solid icon. Keep labels short. For multiline explanations,
  use a text element beside the icon card.

## Themes

Studio is the default. Use `<style theme="Porcelain"/>` before slides or
`<presentation style="./styles/custom.style">`. CLI `--style` overrides
the presentation style. Inline style overrides a referenced style.
Font roles, families, and colors are documented in the theme guide.
Theme layout defaults do not insert margins or gaps into manual trees.
Switching themes can change text metrics; re-render after changing fonts.

## Verification and delivery

Validate against the local schema when developing with this checkout:

```sh
xmllint --noout --dtdvalid schemas/presentation.dtd "My Talk.slides/presentation.xml"
./build/presenter --check --json --strict "My Talk.slides"
./build/presenter "My Talk.slides" --slide 1 --screenshot /tmp/slide.png
./build/presenter "My Talk.slides" --slide 1 --presenter-screenshot /tmp/notes.png
```

Run the sanity checker before rendering. Use each issue's stable `code`,
one-based `slide`, element path, bounds, and measurements to revise the XML.
Suggestions are options, not mandatory changes: avoid blindly shrinking text
or enlarging every downscaled image. Enlargement to 200% or more and crops
discarding over half the source area are warnings; downscaling to 10% or less
is informational. Scale describes dimensions, not area. `--strict` exits 2
on warnings/errors (0 otherwise); invalid input exits 1. `--slide N` checks
only one slide. JSON stdout contains only the report, with `schemaVersion: 1`.

`underpopulated_slide` warns when at least 45% of the canvas height is empty
below visible content, with less than half that much space above. It measures
text lines and displayed image bounds rather than stretched containers or
panel backgrounds; balanced centered layouts are exempt. Review the reported
`contentTop`, `contentBottom`, `emptyBelowPx`, and `emptyBelowFraction` before
enlarging or redistributing content, combining slides, or keeping intentional
whitespace. This heuristic detects trailing empty space, not all sparse layouts.

Render every slide and inspect the images. Check for overflow diagnostics,
missing images, unreadable chart labels, unsuitable crops, and excessive
text. Fix constraints or edit the content; do not rely on automatic scaling.
The parser checks structure and layout properties without downloading the
remote DTD. Use xmllint for full schema validation.

For comparisons, use two equal star columns with matching image/caption
grids. For an opening, center a stack. For a card dashboard, use explicit
rows/columns and padded borders. Build these compositions yourself; no
preset names exist.

Deliver the portable package with its source notes. Tell the presenter to
share only the audience window, leaving Presenter View private.
