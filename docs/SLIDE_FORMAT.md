# Slide Format Specification

A markdown-based format for creating presentations. Each `# heading` starts a new slide. Optional `layout:` and `notes:` directives appear before the heading.

---

## Slide Separation

Slides are separated by `# heading` at the start of a line. Each `# ` marks the beginning of a new slide.

```
# First Slide Title
content here

# Second Slide Title
content here
```

No `---` delimiters needed.

---

## Front Matter

Optional key-value pairs appear **before** the `# heading`:

| Key      | Required | Description                        |
| -------- | -------- | ---------------------------------- |
| `layout` | No       | One of: title, content, image, section, two-column (default: content) |
| `notes`  | No       | Presenter speaking notes           |
| `fit`    | No       | Image scaling: `fit` (default, centered) or `fill` (center-crop) |

Example:

```
layout: content
notes: Remind audience about Q3 targets

# Key Points
- First point
```

If no `layout:` is specified, the slide is auto-selected based on content:

1. If multiple blocks are present (separated by `---`) and no image → **two-column**
2. If an image is present → **image** (remaining text becomes caption)
3. If no content blocks at all → **section**
4. Otherwise → **content** (bullet list)

The `title` layout is only available via explicit `layout: title` directive.

---

## Footer

Content, two-column, and image slides automatically display a slide number (`N / total`) in the bottom-right corner. Title and section slides have no footer.

---

## Layout Types

### title

For opening and closing slides. Large centered text.

```
layout: title
notes: Welcome the audience

# Presentation Title
Optional subtitle or tagline
```

### content

For slides with a heading and bullet list (default type).

```
layout: content
notes: Walk through each point

# Key Points
- First point
- Second point
- Third point
```

### image

For slides featuring a large image with caption.

```
layout: image
notes: Describe the architecture diagram

# Architecture Overview
![System Architecture](./images/architecture.png)
Caption text describing the image
```

#### Image Scaling

By default, images use **fit** mode: the image is scaled to fit within the available area while preserving aspect ratio, centered in the part.

To use **fill** mode (center-crop), add `fit: fill` to the front matter:

```
layout: image
fit: fill

# Full-Bleed Photo
![Hero image](./images/hero.jpg)
```

In fill mode, the image is scaled to cover the entire area and center-cropped to fit.

### section

For section dividers (heading only).

```
layout: section

# Part One
```

### two-column

For side-by-side content. Separate text blocks with `---`. Each block becomes a column.

```
layout: two-column
notes: Compare the two approaches

# Comparison
## Option A
- Pros
- Cons
---
## Option B
- Pros
- Cons
```

---

## Image Paths

Image paths are relative to the markdown file's directory.

```
./images/photo.jpg     ← relative to current file
../shared/assets/fig.png  ← parent directory
```

Supported formats: PNG, JPG, JPEG, GIF, BMP.

---

## Complete Example

```
layout: title
notes: Welcome everyone. Today we're walking through what shipped in Q4 — 
       4 major wins, 12 engineers contributed, 87 PRs merged.

# Q4 Product Roadmap
Engineering Team — July 2026

layout: section

# What We Shipped

layout: content
notes: Dashboards used to reload on every filter change — 3-4 second spinner.
       Lazy loading cut that to under 200ms. The mobile crash was our #1
       support ticket — now zero crashes in 2 weeks. Mention that auth
       unblocked the enterprise deal we've been chasing.

# Recent Wins
- Dashboards respond in under 200ms (was 3-4s)
- Zero mobile crashes for 2 weeks (was #1 support ticket)
- SAML 2.0 auth unlocked enterprise deal
- CI builds finish 3x faster — saves 90 hrs/week across team

layout: two-column
notes: Left side is the old search — every keystroke triggered a full
       page reload with a 2-second spinner. Right side is after —
       debounced at 200ms with cached results. Point to the result count.

# Search: Before and After
![Old search page](./commits/abc123_before.png)
## Before
Full page reload on every keystroke
---
![New search page](./commits/abc123_after.png)
## After
Instant results, debounced 200ms

layout: content
notes: Users can now export any dashboard to PDF in one click — this was
       the #2 feature request. The onboarding flow went from 8 steps to 3.
       On the performance side, we cut cold start from 22s to under 5s by
       lazy-loading all non-critical modules.

# What Users Can Now Do
- Export any dashboard to PDF with one click
- Complete onboarding in 3 steps (was 8)
- Cold start under 5 seconds (was 22s)
- Search across all projects from home screen

layout: section

# What's Next

layout: two-column
notes: Dark mode was our most-requested feature this quarter — designs
       are done, implementation starts Monday. Real-time sync will let
       two users edit the same dashboard simultaneously. Ask the team
       if there are other priorities we're missing.

# Q1 Priorities
## This Quarter
- Dark mode rollout (most-requested feature)
- Real-time collaboration on dashboards
- Error budget tracking dashboard
---
## Backlog
- CLI v2 with plugin system
- Self-hosted option for enterprise
- Accessibility audit and fixes

layout: title
notes: Thanks everyone. We shipped more this quarter than any previous one.
       Open for questions — especially on the before/after demos or Q1 priorities.

# Thank You
Questions?
```

---

## Presenter Notes

- Notes are **only** shown in the presenter window, never on the main display.
- Use them to remind yourself of talking points, transitions, or audience interactions.
- Leave notes empty or omit them if not needed.

---

## Presentation Best Practices

Guidelines for creating impactful, audience-focused presentations — especially from git history and automated sources.

### Bullet Point Guidelines

**Limit each slide to 3-5 bullet points.** Audiences disengage with walls of text. Keep bullets short — one line each, 6-8 words maximum. Each bullet should be a prompt for you to talk, not a sentence to read aloud.

| Do | Don't |
|---|---|
| `- 50% faster cold start after lazy-load refactor` | `- We refactored the initialization code by introducing lazy loading for the user module which makes the app start 50% faster` |
| `- Auth now supports SAML 2.0` | `- Implemented support for SAML 2.0 authentication protocol` |

**Avoid list dumps.** If you have 12 bullet points, split them across 2-3 slides or group them under a single takeaway sentence.

### Writing Impact-Oriented Messages

Every bullet should answer **"so what?"** — don't just describe what changed, explain what is now possible.

| Weak (what was done) | Strong (why it matters) |
|---|---|
| Updated the onboarding flow | New users complete onboarding in 60 seconds |
| Added search indexing | Full-text search returns results in under 100ms |
| Migrated to PostgreSQL 16 | App can now handle 10x concurrent queries |
| Fixed memory leak in rendering | Dashboard stays responsive after 8+ hours of uptime |

**Use specific numbers and timeframes** whenever possible. "3 weeks saved per quarter" beats "faster workflow".

**Frame bullets as outcomes, not actions:**
- "Users can now..." instead of "Added ability to..."
- "Pages load 2x faster because..." instead of "Optimized rendering..."

### Using Screenshots Effectively

Screenshots are the highest-impact way to show progress on UI-heavy work. The best patterns:

**Before/after pairs (most powerful):**

```
layout: two-column
notes: Point out the old version was 3 clicks, now it's 1

# Dashboard Redesign
![Before](./commits/abc123_before.png)
## Before
3 clicks to find a report
---
![After](./commits/abc123_after.png)
## After
Reports one click from home
```

Always add a short caption telling the audience **what to notice**: "Before: 3 clicks. After: 1 click from home."

**Single screenshot with annotation:**

```
layout: image
notes: The new diff view was the main request from users

# Pull Request Diff
![PR Diff View](./commits/diff.png)
New side-by-side diff with syntax highlighting
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

### Presenter Notes: What to Actually Say

The notes field is your speaking script. The audience sees the bullets; you see the story behind them.

**Good notes tell you:**
- **Why** this change was made (the user pain, the blocker)
- **What to highlight** on a screenshot ("point to the new sidebar")
- **A specific number or story** that backs up the bullet
- **Transition phrases** between slides ("This next one was the hardest part...")
- **Questions to ask the audience** ("Any team facing this same issue?")

Example:

```
layout: content
notes: We were losing 2 hours/week per developer on slow CI. After profiling,
       found the test DB init was the bottleneck. Replaced with container snapshots
       — now 90% of builds finish in under 4 min. Mention that Jane from infra
       did the heavy lifting here.

# CI Pipeline Optimized
- Build time reduced from 22 min to 4 min
- Test DB init now uses container snapshots
- All 60 engineers affected — 2 hrs/week saved each
```

### Structuring a Git-History Presentation

An AI agent generating slides from git history should follow this narrative arc:

1. **Title slide** — The sprint, release, or time period (e.g. "Sprint 12 Wrap-Up")
2. **Section: Highlights** — 2-3 biggest wins, framed as outcomes
3. **Section: What Shipped** — 4-8 key changes, grouped by theme (UX, performance, infra)
4. **Section: Deep Dives (optional)** — Before/after screenshot slides for the most visible changes
5. **Section: Numbers** — Metrics: lines changed, PRs merged, bugs closed, perf numbers
6. **Title slide (close)** — Summary + next sprint preview

### Before/After Slide Pattern for Commits

For any commit labeled "improved X" or "updated component X", generate a **two-column before/after slide**:

```
layout: two-column
notes: Before — the search page reloaded on every keystroke.
       After — debounced queries with cached results. Show the latency number.

# Search: Before and After
![Before](./commits/d4e5f6_before.png)
## Before
Keystroke → page reload → 2s wait
---
![After](./commits/d4e5f6_after.png)
## After
Instant results, debounced at 200ms
```

**What makes a good before/after slide:**
- Both screenshots cropped to the same area of the UI
- Caption on each side states the **user experience**, not the technical change
- Presenter notes guide the speaker to the specific difference

### Content Slide Anti-Patterns

| Avoid | Why | Do instead |
|---|---|---|
| Raw commit messages as bullets | "fix: stuff" means nothing to an audience | Rewrite as outcome: "Login edge case resolved" |
| Technical jargon on slides | Audience may not know what "refactored DI container" means | Keep slides plain-language; put technical detail in notes |
| More than 5 bullets | Eyes glaze over | Split into 2 slides or group under a theme |
| Bullets that are full sentences | You'll read them word-for-word | Use 4-6 word prompts and elaborate verbally |
| Listing every commit | Most commits are noise | Curate to highlight wins, fixes, and blocked work |

### Visual Quality Checklist

- [ ] Each slide has a clear, action-oriented heading
- [ ] 3-5 bullet points max, under 8 words each
- [ ] At least one numeric metric per section ("40% faster", "120 engineers affected")
- [ ] Every bullet answers "so what?"
- [ ] Images cropped to focus area, captioned with what to notice
- [ ] Presenter notes written for each slide (at minimum: the one thing to say)
- [ ] One clear narrative thread from first slide to last

---

## Git History Use Case

An AI agent can populate slides from git history:

1. **Section slides** from version tags or release branches.
2. **Content slides** from commit messages — group related commits under a heading, rewriting each message as an impact-oriented bullet (see Best Practices above).
3. **Notes** from detailed commit bodies or PR descriptions — enriched with context from the git diff (files changed, metrics).
4. **Before/after image slides** for commits tagged "improved", "updated", "redesigned" — take screenshots of the UI at both the parent and target commit.
5. **Title slides** from release names or changelog entries.

### Screenshot Strategy for Git History

When an agent encounters commits that changed UI code, it should:

1. Check out the parent commit, capture screenshot → save as `_before.png`
2. Check out the target commit, capture screenshot → save as `_after.png`
3. Generate a `two-column` slide with both screenshots side by side
4. Write a caption for each side describing the user experience, not the code change
5. Add presenter notes pointing to the specific visual differences

Example mapping:

```
git log --oneline --since="2 weeks ago"
```

Each commit message becomes an impact-oriented bullet. Presenter notes pull from commit bodies, PR descriptions, and git diff stats (e.g. "Changed 12 files, +230 -45 lines"). For UI commits, the agent automatically generates before/after screenshot slides.
