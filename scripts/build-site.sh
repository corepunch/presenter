#!/bin/sh
set -eu

# GitHub Pages publishes docs/; include the skills and the schemas they link to.
site_dir="$(CDPATH= cd -- "$(dirname -- "$0")/../docs" && pwd)"
skills_dir="$(CDPATH= cd -- "$(dirname -- "$0")/../skills" && pwd)"
schemas_dir="$(CDPATH= cd -- "$(dirname -- "$0")/../schemas" && pwd)"

mkdir -p "$site_dir/skills" "$site_dir/schemas"
find "$site_dir/skills" -type f -name '*.md' -delete
for skill in "$skills_dir"/*.md; do
  # Source links are relative to skills/ in the repo; online, docs/ is the root.
  sed 's|(../docs/|(../|g' "$skill" > "$site_dir/skills/$(basename "$skill")"
done
cp "$schemas_dir"/*.dtd "$site_dir/schemas/"

echo "Published skills and schemas to $site_dir"
