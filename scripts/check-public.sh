#!/usr/bin/env bash
# Fails if committed files contain private network details, home or NAS paths, or the maintainer's email.
# Game titles are allowed in public files.
set -euo pipefail
root=$(git rev-parse --show-toplevel)
cd "$root"
found=0
if git grep -I -n -E '\.arpa\b|/mnt/user|192\.168\.[0-9]+\.[0-9]+|/Users/chronostriker1|chronostriker1@gmail' HEAD -- . ':!scripts/check-public.sh'; then found=1; fi
names="${VNDEGEN_PRIVATE_NAMES:-$HOME/.config/vndegen/private-names}"
if [ -s "$names" ] && git grep -I -n -i -w -F -f "$names" HEAD -- . ':!scripts/check-public.sh'; then found=1; fi
[ "$found" = 0 ] && echo "public check passed" || { echo "public check failed: remove the lines above before publishing" >&2; exit 1; }
