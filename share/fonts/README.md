# Bundled presentation fonts

Static TrueType fonts downloaded from Adobe's release branches on 2026-09-08:

- Source Serif 4: https://github.com/adobe-fonts/source-serif/tree/release/TTF
- Source Sans 3: https://github.com/adobe-fonts/source-sans/tree/release/TTF

Files are unmodified. Each family includes its upstream SIL Open Font License
in LICENSE.md. Regular, Bold, It (italic), and BoldIt faces are included.
Inter, JetBrains Mono and Font Awesome remain in assets/ for compatibility.

The Makefile copies share/ beside the binary. Release archives and install.sh
already include this directory. Font loading also resolves paths beside the
executable, so an installed app works from any working directory.
