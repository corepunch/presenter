#!/usr/bin/env python3
"""Parse FontAwesome-Free.css and generate include/fa_icons.h."""
import re, pathlib

root = pathlib.Path(__file__).resolve().parent.parent
css_path = root / "assets" / "FontAwesome-Free.css"
out_path = root / "include" / "fa_icons.h"

icons = {}
current = None
for line in css_path.read_text().split("\n"):
    m = re.match(r"\.fa-([a-z0-9-]+)\s*\{", line)
    if m:
        current = m.group(1)
        continue
    if current:
        m2 = re.search(r'--fa:\s*"\\([0-9a-fA-F]+)"', line)
        if m2:
            icons[current] = int(m2.group(1), 16)
            current = None
        elif "}" in line:
            current = None

lines = [
    "// Generated from FontAwesome-Free.css — do not edit.",
    "// Re-generate: python3 scripts/gen_fa_icons.py",
    "#pragma once",
    "#include <string>",
    "#include <unordered_map>",
    "",
    "inline const std::unordered_map<std::string, uint32_t>& faIcons() {",
    "    static const std::unordered_map<std::string, uint32_t> icons = {",
]
for name, cp in sorted(icons.items()):
    lines.append(f'        {{"{name}", 0x{cp:04X}}},')
lines += ["    };", "    return icons;", "}"]

out_path.write_text("\n".join(lines) + "\n")
print(f"Generated {out_path} with {len(icons)} icons")
