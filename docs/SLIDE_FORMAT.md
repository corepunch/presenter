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

Example:

```
layout: content
notes: Remind audience about Q3 targets

# Key Points
- First point
```

If no `layout:` is specified, the slide defaults to `content` type.

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

### section

For section dividers (heading only).

```
layout: section

# Part One
```

### two-column

For side-by-side content. Use `---left---` and `---right---` as separators.

```
layout: two-column
notes: Compare the two approaches

# Comparison
---left---
## Option A
- Pros
- Cons
---right---
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
notes: Introduce myself and the topic

# Q4 Product Roadmap
Engineering Team — July 2026

layout: section

# What We Shipped

layout: content
notes: Highlight the performance gains

# Recent Wins
- 50% latency reduction
- New auth system shipped
- Mobile app v2.1 released

layout: image
notes: Walk through the diagram

# System Architecture
![Architecture Diagram](./images/arch.png)
Current state after migration

layout: two-column
notes: Ask team for input on priorities

# Q4 Priorities
---left---
## Must Do
- Fix billing pipeline
- Hire 3 backend engineers
---right---
## Nice to Have
- Dashboard redesign
- CI/CD improvements

layout: title
notes: Thank everyone, open for questions

# Thank You
Questions?
```

---

## Presenter Notes

- Notes are **only** shown in the presenter window, never on the main display.
- Use them to remind yourself of talking points, transitions, or audience interactions.
- Leave notes empty or omit them if not needed.

---

## Git History Use Case

An AI agent can populate slides from git history:

1. **Section slides** from version tags or release branches.
2. **Content slides** from commit messages — group related commits under a heading.
3. **Notes** from detailed commit bodies or PR descriptions.
4. **Title slides** from release names or changelog entries.
5. **Image slides** for screenshots or diagrams added in commits.

Example mapping:

```
git log --oneline --since="2 weeks ago"
```

becomes a content slide with the commit list as bullets, and the PR body becomes presenter notes.
