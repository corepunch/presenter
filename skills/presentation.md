# Slide Format Specification

> **DTD schema**: [schemas/presentation.dtd](../schemas/presentation.dtd)  
> **Style DTD**: [schemas/style.dtd](../schemas/style.dtd)

An XML-based format for creating presentations. Each `<slide>` element represents one slide. The root element is `<presentation>`.

---

## Structure Overview

```xml
<?xml version="1.0" encoding="UTF-8"?>
<presentation>
  <slide layout="title" title="My Presentation">
    <notes>Presenter-only notes</notes>
    <subtitle>Optional tagline</subtitle>
  </slide>

  <slide layout="content" title="Agenda">
    <text>First topic</text>
    <text>Second topic</text>
  </slide>
</presentation>
```

Slides are separated by `<slide>` elements — no delimiter characters. The XML structure itself defines slide boundaries.

---

## Slide Attributes

| Attribute | Required | Values | Default | Description |
|-----------|----------|--------|---------|-------------|
| `layout`  | No       | `title`, `section`, `content`, `columns`, `image`, `blank` | `content` | How the layout engine arranges children |
| `title`   | No       | Text string | — | Slide heading, rendered prominently per layout type |
| `cols`    | No       | Integer | `2` | Number of columns when `layout="columns"` |
| `gap`     | No       | Integer | `24` | Gap between columns/slots in pixels |
| `fit`     | No       | `fit`, `fill` | `fit` | Image scaling: `fit` (contain) or `fill` (cover-crop) |
| `slot`    | No       | `left`, `right`, `center`, `top`, `bottom`, `fill`, `0`, `1`, `2`, … | — | Placement hint when parent has `layout="columns"` |

When `layout` is omitted, it defaults to `content`. The `title` layout requires explicit `layout="title"`.

---

## Slide Child Elements

| Element     | Count     | Description |
|-------------|-----------|-------------|
| `<notes>`   | 0 or 1    | Presenter-only speaking notes |
| `<subtitle>` | 0 or 1   | Secondary heading, used with `layout="title"` |
| `<text>`    | 0 or more | Text block with optional inline formatting |
| `<image>`   | 0 or more | Image (self-closing element) |
| `<slide>`   | 0 or more | Nested slide, positioned via `slot` when parent is `layout="columns"` |

`<notes>` should appear first, followed by visual elements. Within a slide, elements render in document order — top to bottom in default vertical flow, or into column slots when using `layout="columns"`.

---

## Layout Types

### title — Opening and Closing Slides

Large centered heading. Use `title` attribute for the headline, `<subtitle>` for the tagline.

```xml
<slide layout="title" title="Q4 Product Roadmap">
  <notes>Welcome everyone. Today we're walking through what shipped in Q4 — 
    4 major wins, 12 engineers contributed, 87 PRs merged.</notes>
  <subtitle>Engineering Team — July 2026</subtitle>
</slide>
```

```xml
<slide layout="title" title="Thank You">
  <subtitle>Questions?</subtitle>
</slide>
```

No footer (no slide numbers). No `<text>` or `<image>` children.

---

### content — Bullet Lists and General Text

The default layout. Each `<text>` element renders as a separate block.

```xml
<slide layout="content" title="Recent Wins">
  <notes>
    Dashboards used to reload on every filter change — 3-4 second spinner.
    Lazy loading cut that to under 200ms. The mobile crash was our #1
    support ticket — now zero crashes in 2 weeks.
  </notes>
  <text><b>50%</b> latency reduction — dashboards respond instantly</text>
  <text>Zero mobile crashes for 2 weeks (was <b>#1</b> support ticket)</text>
  <text>SAML 2.0 SSO unblocked enterprise deal</text>
  <text>CI builds finish 3x faster — saves 90 hrs/week across team</text>
</slide>
```

Footer: slide number `N / total` in bottom-right corner.

---

### section — Section Dividers

Heading-only slide to separate major sections. The `title` attribute is rendered large and centered.

```xml
<slide layout="section" title="What We Shipped"/>
```

```xml
<slide layout="section" title="What's Next"/>
```

No footer. No child elements other than optional `<notes>`.

---

### image — Full-Width Image with Caption

Displays a large image with optional caption text below.

```xml
<slide layout="image" title="System Architecture" fit="fit">
  <notes>Walk through the diagram — point to the new gateway service.</notes>
  <image src="./images/arch.png" alt="Architecture diagram"/>
  <text>Current state after Q3 microservices migration</text>
</slide>
```

#### Image Scaling Modes

- **`fit` (default):** Scale to fit within available area, preserve aspect ratio, center. Empty space shows the slide background.
- **`fill`:** Scale to cover entire area, center-crop if aspect ratios differ. Best for full-bleed photography.

```xml
<slide layout="image" title="Matterhorn at Dawn" fit="fill">
  <image src="./images/mountain.jpg" alt="Matterhorn reflected in Riffelsee"/>
  <text>The Matterhorn mirrored in Riffelsee — 6:15 AM, zero wind</text>
</slide>
```

Footer: slide number `N / total`.

---

### columns — Side-by-Side Layouts

The most flexible layout. Children with a `slot` attribute are placed into columns. Each slotted child is a `<slide>` that can contain any content.

#### Two-Column Default (`cols="2"`)

```xml
<slide layout="columns" gap="24" title="Dashboard: Before and After">
  <notes>
    Left = old (3 clicks), right = new (1 click).
    Point to the search bar difference.
  </notes>
  <slide slot="left" title="Before">
    <image src="./commits/abc123_before.png" alt="Old dashboard"/>
    <text>3 clicks to find a report</text>
  </slide>
  <slide slot="right" title="After">
    <image src="./commits/abc123_after.png" alt="New dashboard"/>
    <text>Reports one click from home</text>
  </slide>
</slide>
```

#### Two-Column Text-Only

```xml
<slide layout="columns" gap="24" title="Q1 Priorities">
  <slide slot="left" title="This Quarter">
    <text>Dark mode rollout — most-requested feature</text>
    <text>Real-time collaboration on dashboards</text>
    <text>Error budget tracking dashboard</text>
  </slide>
  <slide slot="right" title="Backlog">
    <text>CLI v2 with plugin system</text>
    <text>Self-hosted option for enterprise</text>
    <text>Accessibility audit and fixes</text>
  </slide>
</slide>
```

#### Image + Text Side by Side

```xml
<slide layout="columns" gap="24" title="Feature Highlight">
  <slide slot="left">
    <image src="./images/screenshot.png" alt="Feature screenshot"/>
  </slide>
  <slide slot="right">
    <text><b>Faster</b> — loads in under 200ms</text>
    <text><b>Simpler</b> — export to PDF in 3 clicks</text>
    <text><b>Accessible</b> — WCAG 2.1 AA compliant</text>
  </slide>
</slide>
```

#### Three Columns

```xml
<slide layout="columns" cols="3" gap="16" title="From Alps to Sea">
  <notes>Three perspectives from the last day of the trek.</notes>
  <slide slot="left" title="Alpine Meadow">
    <image src="./images/meadow.jpg" alt="Alpine meadow"/>
    <text>2200m, blooming wildflowers</text>
  </slide>
  <slide slot="center" title="Ligurian Coast">
    <image src="./images/coast.jpg" alt="Ligurian coast"/>
    <text>Golden hour, Portofino</text>
  </slide>
  <slide slot="right" title="Matterhorn">
    <image src="./images/mountain.jpg" alt="Matterhorn alpenglow"/>
    <text>Alpenglow, 8-minute window</text>
  </slide>
</slide>
```

#### Numeric Slots (Grid Layout)

Numeric slots wrap by `cols`. Slot `0`→col0-row0, `1`→col1-row0, `2`→col0-row1, `3`→col1-row1, etc.

```xml
<slide layout="columns" cols="2" gap="12" title="Screenshots">
  <slide slot="0"><image src="./1.png" alt="First"/></slide>
  <slide slot="1"><image src="./2.png" alt="Second"/></slide>
  <slide slot="2"><image src="./3.png" alt="Third"/></slide>
  <slide slot="3"><image src="./4.png" alt="Fourth"/></slide>
</slide>
```

#### Three-Way Comparison (Named Slots with `cols="3"`)

Named slots also work: `left`, `center`, `right`.

```xml
<slide layout="columns" cols="3" gap="16" title="Font Rendering Options">
  <slide slot="left" title="Bitmap">
    <image src="./bitmap.png" alt="Bitmap rendering"/>
    <text>Sharp at 1x, blurry scaled</text>
  </slide>
  <slide slot="center" title="SDF">
    <image src="./sdf.png" alt="SDF rendering"/>
    <text>Crisp at any scale</text>
  </slide>
  <slide slot="right" title="MSDF">
    <image src="./msdf.png" alt="MSDF rendering"/>
    <text>Crisp + sharp corners</text>
  </slide>
</slide>
```

#### Recursive Nesting

Columns can contain nested columns for arbitrarily complex layouts.

```xml
<slide layout="columns" gap="24" title="Deep Comparison">
  <slide slot="left">
    <image src="./before.png" alt="Before refactor"/>
    <text>Before the refactor</text>
  </slide>
  <slide slot="right">
    <slide layout="columns" gap="16">
      <slide slot="left">
        <text><b>2x</b> faster response time</text>
        <text><b>40%</b> less memory usage</text>
      </slide>
      <slide slot="right">
        <image src="./chart.png" alt="Performance chart"/>
      </slide>
    </slide>
  </slide>
</slide>
```

Footer: slide number `N / total`.

---

### blank — Freeform Layout

No header. Children fill the full slide area.

```xml
<slide layout="blank">
  <image src="./hero.jpg" alt="Full-slide hero image"/>
</slide>
```

No footer.

---

## Text Formatting

Text inside `<text>`, `<title>`, `<notes>`, and `<subtitle>` uses HTML-like inline tags:

| Tag | Effect | Example |
|-----|--------|---------|
| `<b>` | Bold | `<text><b>Important</b> finding</text>` |
| `<i>` | Italic | `<text>Species: <i>Homo sapiens</i></text>` |
| `<code>` | Monospace | `<text>Run <code>npm install</code> first</text>` |

**Markdown markers (`**bold**`, `_italic_`, `` `code` ``) are not supported.** Always use the XML tags.

Long text wraps automatically. For paragraph breaks, use separate `<text>` elements:

```xml
<text>First paragraph of text.</text>
<text>Second paragraph, rendered below the first.</text>
```

Empty `<text/>` elements add vertical spacing:

```xml
<text>Section A</text>
<text/>
<text>Section B (with extra gap above)</text>
```

---

## Images

Self-closing element. Supported formats: PNG, JPG, JPEG, GIF, BMP.

```xml
<image src="./images/photo.jpg" alt="Description of the image"/>
```

| Attribute | Required | Description |
|-----------|----------|-------------|
| `src`     | Yes      | Path relative to the XML file's directory |
| `alt`     | No       | Accessible text description |

### Image Paths

Paths are relative to the XML file:

```
./images/photo.jpg           — same directory
../shared/assets/fig.png     — parent directory
subdir/chart.png             — relative path
```

### Image Placement by Layout

| Layout | Image behavior |
|--------|---------------|
| `image` | First `<image>` fills the available content area; `<text>` elements render as captions below |
| `content` | Images render inline between `<text>` elements in document order |
| `columns` | Images inside slotted child slides fill their column's allocated space |
| `blank` | Images fill the full slide area |

---

## Presenter Notes

The `<notes>` element is only shown in the presenter window, never on the audience display.

```xml
<slide layout="content" title="Performance Gains">
  <notes>We were losing 2 hours/week per developer on slow CI.
    After profiling, found the test DB init was the bottleneck.
    Replaced with container snapshots — now 90% of builds
    finish in under 4 min. Mention that Jane from infra
    did the heavy lifting here.</notes>
  <text>Build time reduced from 22 min to 4 min</text>
  <text>Test DB init now uses container snapshots</text>
  <text>All 60 engineers affected — 2 hrs/week saved each</text>
</slide>
```

**Good notes tell you:**
- **Why** the change was made (the user pain, the blocker)
- **What to highlight** on a screenshot ("point to the new sidebar")
- **Specific numbers** that back up the bullet points
- **Transition phrases** between slides
- **Questions to ask the audience** ("Any team facing this same issue?")

---

## Complete Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<presentation>
  <slide layout="title" title="Q4 Product Roadmap">
    <notes>Welcome everyone. Today we're walking through what shipped
      in Q4 — 4 major wins, 12 engineers contributed, 87 PRs merged.</notes>
    <subtitle>Engineering Team — July 2026</subtitle>
  </slide>

  <slide layout="section" title="What We Shipped"/>

  <slide layout="content" title="Recent Wins">
    <notes>Dashboards used to reload on every filter change — 3-4 second
      spinner. Lazy loading cut that to under 200ms.</notes>
    <text>Dashboards respond in under 200ms (was 3-4s)</text>
    <text>Zero mobile crashes for 2 weeks (was <b>#1</b> support ticket)</text>
    <text>SAML 2.0 auth unlocked enterprise deal</text>
    <text>CI builds finish 3x faster</text>
  </slide>

  <slide layout="columns" gap="24" title="Search: Before and After">
    <notes>Left side is the old search — every keystroke triggered a full
      page reload with a 2-second spinner. Right side is after —
      debounced at 200ms with cached results.</notes>
    <slide slot="left" title="Before">
      <image src="./commits/abc123_before.png" alt="Old search page"/>
      <text>Full page reload on every keystroke</text>
    </slide>
    <slide slot="right" title="After">
      <image src="./commits/abc123_after.png" alt="New search page"/>
      <text>Instant results, debounced 200ms</text>
    </slide>
  </slide>

  <slide layout="content" title="What Users Can Now Do">
    <notes>Users can now export any dashboard to PDF in one click.</notes>
    <text>Export any dashboard to PDF with one click</text>
    <text>Complete onboarding in 3 steps (was 8)</text>
    <text>Cold start under 5 seconds (was 22s)</text>
    <text>Search across all projects from home screen</text>
  </slide>

  <slide layout="section" title="What's Next"/>

  <slide layout="columns" gap="24" title="Q1 Priorities">
    <notes>Dark mode was our most-requested feature this quarter.</notes>
    <slide slot="left" title="This Quarter">
      <text>Dark mode rollout — most-requested feature</text>
      <text>Real-time collaboration on dashboards</text>
      <text>Error budget tracking dashboard</text>
    </slide>
    <slide slot="right" title="Backlog">
      <text>CLI v2 with plugin system</text>
      <text>Self-hosted option for enterprise</text>
      <text>Accessibility audit and fixes</text>
    </slide>
  </slide>

  <slide layout="title" title="Thank You">
    <notes>Thanks everyone. We shipped more this quarter than any
      previous one.</notes>
    <subtitle>Questions?</subtitle>
  </slide>
</presentation>
```

---

## Style

Presentation styling (fonts, colors, layout metrics) is configured via an external style file. See [schemas/style.dtd](../schemas/style.dtd) for the full style format.

Style files can be referenced in two ways:

1. **Inline attribute:**
   ```xml
   <presentation style="./styles/dark.xml">
     ...
   </presentation>
   ```

2. **CLI flag:**
   ```
   presenter demo.xml --style=styles/light.xml
   ```

If no style is specified, the built-in dark theme is used.

---

## Presentation Best Practices

### Bullet Point Guidelines

**Limit each slide to 3-5 `<text>` elements.** Audiences disengage with walls of text. Keep bullet text short — one line each, 6-8 words maximum. Each bullet should be a prompt for you to talk, not a sentence to read aloud.

| Do | Don't |
|---|---|
| `<text>50% faster cold start after lazy-load refactor</text>` | `<text>We refactored the initialization code by introducing lazy loading for the user module which makes the app start 50% faster</text>` |
| `<text>Auth now supports SAML 2.0</text>` | `<text>Implemented support for SAML 2.0 authentication protocol</text>` |

**Avoid list dumps.** If you have 12 bullet points, split them across 2-3 slides or group them under a single takeaway sentence.

### Writing Impact-Oriented Messages

Every text block should answer **"so what?"** — don't just describe what changed, explain what is now possible.

| Weak (what was done) | Strong (why it matters) |
|---|---|
| Updated the onboarding flow | New users complete onboarding in 60 seconds |
| Added search indexing | Full-text search returns results in under 100ms |
| Migrated to PostgreSQL 16 | App can now handle 10x concurrent queries |
| Fixed memory leak in rendering | Dashboard stays responsive after 8+ hours of uptime |

**Use specific numbers and timeframes.** "3 weeks saved per quarter" beats "faster workflow".

**Frame text as outcomes, not actions:**
- "Users can now..." instead of "Added ability to..."
- "Pages load 2x faster because..." instead of "Optimized rendering..."

### Using Screenshots Effectively

**Before/after pairs (most powerful):**

```xml
<slide layout="columns" gap="24" title="Dashboard Redesign">
  <notes>Point out the old version was 3 clicks, now it's 1.</notes>
  <slide slot="left" title="Before">
    <image src="./commits/abc123_before.png" alt="Old dashboard"/>
    <text>3 clicks to find a report</text>
  </slide>
  <slide slot="right" title="After">
    <image src="./commits/abc123_after.png" alt="New dashboard"/>
    <text>Reports one click from home</text>
  </slide>
</slide>
```

Always add a short caption telling the audience **what to notice**: "Before: 3 clicks. After: 1 click from home."

**Single screenshot:**

```xml
<slide layout="image" title="Pull Request Diff View" fit="fill">
  <notes>The new diff view was the main request from users.</notes>
  <image src="./commits/diff.png" alt="PR diff view"/>
  <text>New side-by-side diff with syntax highlighting</text>
</slide>
```

**When to include screenshots:**
- UI components, style changes, layout updates
- Charts, dashboards, analytics views
- CLI output showing before/after benchmarks
- Diagrams of architecture changes
- Error states → fixed states (before/after)

**When to skip screenshots:**
- Refactors with no visual change (describe the outcome instead)
- Configuration or CI changes (use a status badge or log snippet)
- Purely backend work with no UI surface

### Structuring a Presentation

Follow this narrative arc:

1. **Title slide** — Sprint, release, or time period
2. **Section: Highlights** — 2-3 biggest wins, framed as outcomes
3. **Section: What Shipped** — 4-8 key changes, grouped by theme (UX, performance, infra)
4. **Section: Deep Dives** (optional) — Before/after screenshot slides for visible changes
5. **Section: Numbers** — Metrics: lines changed, PRs merged, bugs closed, perf numbers
6. **Title slide (close)** — Summary + next sprint preview

### Before/After Slide Pattern for Commits

For any commit labeled "improved X" or "updated component X", generate a two-column before/after slide:

```xml
<slide layout="columns" gap="24" title="Search: Before and After">
  <notes>Before — the search page reloaded on every keystroke.
    After — debounced queries with cached results. Show the latency number.</notes>
  <slide slot="left" title="Before">
    <image src="./commits/d4e5f6_before.png" alt="Old search"/>
    <text>Keystroke → page reload → 2s wait</text>
  </slide>
  <slide slot="right" title="After">
    <image src="./commits/d4e5f6_after.png" alt="New search"/>
    <text>Instant results, debounced at 200ms</text>
  </slide>
</slide>
```

**What makes a good before/after slide:**
- Both screenshots cropped to the same area of the UI
- Caption on each side states the **user experience**, not the technical change
- Presenter notes guide the speaker to the specific difference

### Content Slide Anti-Patterns

| Avoid | Why | Do instead |
|---|---|---|
| Raw commit messages as bullets | "fix: stuff" means nothing | Rewrite as outcome: "Login edge case resolved" |
| Technical jargon on slides | Audience may not understand | Keep slides plain-language; put technical detail in notes |
| More than 5 `<text>` elements | Eyes glaze over | Split into 2 slides or group under a theme |
| Full-sentence bullets | You'll read them verbatim | Use 4-6 word prompts and elaborate verbally |
| Listing every commit | Most commits are noise | Curate to highlight wins, fixes, and blocked work |

### Visual Quality Checklist

- [ ] Each slide has a clear, action-oriented `title`
- [ ] 3-5 `<text>` elements max, under 8 words each
- [ ] At least one numeric metric per section ("40% faster", "120 engineers affected")
- [ ] Every bullet answers "so what?"
- [ ] Images have descriptive `alt` text and captions
- [ ] `<notes>` written for each slide (at minimum: the one thing to say)
- [ ] One clear narrative thread from first slide to last

---

## Git History Use Case

An AI agent generating slides from git history should follow this mapping:

1. **Section slides** — from version tags or release branches
2. **Content slides** — from commit messages; group related commits under a heading, rewrite each message as an impact-oriented bullet
3. **Notes** — from detailed commit bodies or PR descriptions, enriched with context from the git diff (files changed, metrics)
4. **Before/after image slides** — for commits tagged "improved", "updated", "redesigned"; capture screenshots at both parent and target commit
5. **Title slides** — from release names or changelog entries

### Screenshot Strategy for Git History

When encountering commits that changed UI code:

1. Check out the parent commit, capture screenshot → save as `<hash>_before.png`
2. Check out the target commit, capture screenshot → save as `<hash>_after.png`
3. Generate a `columns` slide with both screenshots side by side
4. Write a caption for each side describing the user experience, not the code change
5. Add presenter notes pointing to the specific visual differences

Example mapping:

```
git log --oneline --since="2 weeks ago"
```

Each commit message becomes an impact-oriented `<text>` element. Presenter notes pull from commit bodies, PR descriptions, and git diff stats (e.g. "Changed 12 files, +230 -45 lines"). For UI commits, the agent automatically generates before/after screenshot slides.
