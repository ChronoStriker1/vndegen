#!/usr/bin/env bash
# Stages Ruffle, the Flash player, for the app: checks the files scripts/ruffle-web.sh committed to Engines/Flash/ruffle/
# against their SHA256SUMS, then copies them to <dest> with their notices in <dest>/licenses/.
#   scripts/ruffle-runtime.sh build/ruffle
set -euo pipefail
dest=${1:?destination folder, such as build/ruffle}
source=$(cd "$(dirname "$0")/.." && pwd)/Engines/Flash/ruffle
(cd "$source" && shasum -a 256 -c --quiet SHA256SUMS) || { echo "error: Engines/Flash/ruffle doesn't match its SHA256SUMS" >&2; exit 1; }
mkdir -p "$dest"
rsync -a --delete "$source/" "$dest/"
