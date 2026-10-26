#!/usr/bin/env bash
# Fails if committed files name a game from the maintainer's private lists or contain private network details.
# The lists live only in the local, gitignored .audit/library/ folder; without them only the network checks run.
# The engine survey and its per-game list name games on purpose, so the title check skips those two files.
set -euo pipefail
root=$(git rev-parse --show-toplevel)
cd "$root"
lists=(.audit/library/engine-survey-games.tsv .audit/library/ludusavi-titles.txt)
terms=$(mktemp)
trap 'rm -f "$terms"' EXIT
for list in "${lists[@]}"; do
  if [ -f "$list" ]; then cut -f1 "$list" | tail -n +2; fi
done | { grep -v -x -i "No Socks RPG" || true; } | awk 'length($0) >= 5' | sort -u > "$terms"
# Short forms of titles, such as a name before a subtitle separator, slip past exact matching.
short=$(sed -E 's/[_:~(].*$//; s/ +$//' "$terms" | awk 'length($0) >= 8' | sort -u)
[ -n "$short" ] && printf '%s\n' "$short" >> "$terms"
found=0
if [ -s "$terms" ] && git grep -I -n -F -f "$terms" HEAD -- . ':!*.json' ':!**/Fixtures/**' ':!docs/engine-survey.md' ':!docs/engine-survey-games.tsv'; then found=1; fi
if git grep -I -n -E '\.arpa\b|/mnt/user|192\.168\.[0-9]+\.[0-9]+|/Users/chronostriker1|chronostriker1@gmail' HEAD -- . ':!scripts/check-public.sh'; then found=1; fi
[ "$found" = 0 ] && echo "public check passed" || { echo "public check failed: remove the lines above before publishing" >&2; exit 1; }
