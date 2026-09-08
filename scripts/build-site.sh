#!/bin/sh
set -eu

# GitHub Pages publishes docs/; keep the agent-facing skills in that published
# tree without maintaining a second copy by hand.
site_dir="$(CDPATH= cd -- "$(dirname -- "$0")/../docs" && pwd)"
skills_dir="$(CDPATH= cd -- "$(dirname -- "$0")/../skills" && pwd)"

mkdir -p "$site_dir/skills"
find "$site_dir/skills" -type f -name '*.md' -delete
cp "$skills_dir"/*.md "$site_dir/skills/"

echo "Published skills to $site_dir/skills"
