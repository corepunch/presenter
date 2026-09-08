# Presentation themes

Studio is the default for decks without a custom style. Four editorial presets join
the eight classic themes. Switch using Shift+Left / Shift+Right.

| Theme | Direction |
| --- | --- |
| Studio | Midnight violet, warm gold, near-white headings |
| Porcelain | Warm ivory, plum ink, copper accent |
| Tidal | Deep ocean, mint, cool blue |
| Ember | Aubergine, peach, rose |

Studio uses Inter; Porcelain pairs Source Serif 4 headings with Source Sans 3
body text; Tidal uses Source Sans 3; Ember pairs Source Serif 4 with Inter.
All four use bold headings, JetBrains Mono code, 56px slide margins and 28px
rounded corners (14px in the notes window). Fonts ship with the app for offline use.

## Choose a preset

Use an inline style or a standalone style file:

```xml
<style theme="Studio" name="My studio">
  <fonts titleFamily="Inter" bodyFamily="Source Sans 3"
         codeFamily="JetBrains Mono" boldTitles="true"
         title="88" subtitle="36" content="32" bullet="40"
         small="20" childTitle="48"/>
  <colors bg="#090C16" bg2="#302943" title="#F5F3EE"
          text="#DDDCE5" subtitle="#FFD166" accent="#FFD166"
          dim="#AAA7B8" line="#393C51"/>
  <charts series1="#FFD166" series2="#6EE7DF" series3="#FF91B6"
          series4="#B5A4FF" series5="#FFAD80" series6="#A3DCAD"
          grid="#393C51" label="#DDDCE5"/>
  <syntax bg="#171A2B" border="#393C51" text="#DDDCE5"
          keyword="#FF91B6" type="#6EE7DF" string="#A3DCAD"
          comment="#AAA7B8" number="#FFAD80" builtin="#B5A4FF"
          punctuation="#DDDCE5"/>
  <layout margin="56" padding="24" gap="28" columnGap="32"
          linePadding="8" bulletGap="24" presenterMargin="20"
          cornerRadius="28" presenterCornerRadius="14"/>
</style>
```

Overrides apply after the preset. Setting bg without bg2 gives a solid background.
Child elements follow the order shown above; all are optional. A standalone file
uses the XML declaration and `<!DOCTYPE style SYSTEM
"https://corepunch.github.io/presenter/schemas/style.dtd">`.
The same style element works inline in presentation.xml. Existing style files
without a theme retain their legacy defaults.

Family names are exactly `Inter`, `Source Sans 3`, `Source Serif 4`, and
`JetBrains Mono`. Unknown families fail loading with a diagnostic.
Use JetBrains Mono for code to preserve monospace alignment. Regular, bold,
italic and bold-italic faces are bundled for both Source families. Font paths
are internal to the app; decks reference stable family names rather than
machine-specific paths. Colors use `#RRGGBB`; font sizes and layout values
are in pixels on the 1280×720 slide canvas.

The syntax palette also supplies the surface and border for native chart/icon
cards. `boldTitles` controls title and child-heading weight; inline bold and
italic body formatting continues to select the corresponding real font face.

bg2 adds a gentle gradient on title and section slides and the presenter background.
Body slides stay solid so nested content and rounded images compose cleanly.
Subtitle font size is now used on opening slides; long titles wrap within margins.

Try the portable demonstration:

```sh
./build/presenter "demo/Theme Studio.slides"
./build/presenter "demo/Theme Studio.slides" --style "demo/Theme Studio.slides/styles/Porcelain.style"
```
