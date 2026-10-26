#!/usr/bin/env bash
# Fails if committed files name a game from the maintainer's private lists or contain private network details.
# The lists live only in the local, gitignored .audit/library/ folder; without them only the network checks run.
set -euo pipefail
root=$(git rev-parse --show-toplevel)
cd "$root"
lists=(.audit/library/engine-survey-games.tsv .audit/library/ludusavi-titles.txt)
terms=$(mktemp)
trap 'rm -f "$terms"' EXIT
for list in "${lists[@]}"; do
  [ -f "$list" ] && cut -f1 "$list" | tail -n +2
done | grep -v -x -i "No Socks RPG" | awk 'length($0) >= 5' | sort -u > "$terms"
# Short forms of titles, such as a name before a subtitle separator, slip past exact matching.
sed -E 's/[_:~(].*$//; s/ +$//' "$terms" | awk 'length($0) >= 8' | sort -u >> "$terms"
found=0
if [ -s "$terms" ] && git grep -I -n -F -f "$terms" HEAD -- . ':!*.json' ':!**/Fixtures/**'; then found=1; fi
if git grep -I -n -E '\.arpa\b|/mnt/user|192\.168\.[0-9]+\.[0-9]+|/Users/chronostriker1|chronostriker1@gmail' HEAD -- . ':!scripts/check-public.sh'; then found=1; fi
[ "$found" = 0 ] && echo "public check passed" || { echo "public check failed: remove the lines above before publishing" >&2; exit 1; }
