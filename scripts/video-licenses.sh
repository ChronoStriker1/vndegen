#!/usr/bin/env bash
# Writes build/video/licenses/, the notices of vnvideo, the movie decoder in Engines/Shared that native engines compile:
# pl_mpeg for MPEG-1, and libogg, libtheora and libopus for Ogg Theora with Opus sound.
#   scripts/video-licenses.sh
set -euo pipefail
cd "$(dirname "$0")/.."
lib=Engines/Shared/third_party
out=build/video/licenses
rm -rf "$out.part" && mkdir -p "$out.part"
cp "$lib/pl_mpeg/LICENSE.txt" "$out.part/pl_mpeg (movies).txt"
cp "$lib/ogg/COPYING" "$out.part/libogg 1.3.6 (movies).txt"
cat "$lib/theora/COPYING" "$lib/theora/LICENSE" > "$out.part/libtheora 1.2.0 (movies).txt"
cp "$lib/opus/COPYING" "$out.part/libopus 1.5.2 (movie sound).txt"
rm -rf "$out" && mv "$out.part" "$out"
