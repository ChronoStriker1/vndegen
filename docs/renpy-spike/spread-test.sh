#!/usr/bin/env bash
# Runs on the Mac mini. Boots every game under spread/games on the Ren'Py 8.5.3 and patched 7.8.7 engines with the
# probe script, and prints one result line per game and engine.
set -uo pipefail
R=~/vndegen-testgames/renpy
cd "$R/spread"
for dir in games/*/; do
  g=$(basename "$dir")
  base=$(find "$dir" -maxdepth 3 -type d -name renpy -prune -print | while read -r r; do p=$(dirname "$r"); [ -d "$p/game" ] && echo "$p"; done | head -1)
  [ -n "$base" ] || { echo "$g | no base dir"; continue; }
  own=$(grep -ho "version_tuple = ([0-9, ]*" "$base/renpy/__init__.py" 2>/dev/null | head -1 | tr -dc '0-9,' | cut -d, -f1-3 | tr , .)
  cp "$R/run/zz_vndegen_probe.rpy" "$base/game/"
  for engine in 8.5.3 7.8.7; do
    if [ $engine = 8.5.3 ]; then S=$R/sdk/renpy-8.5.3-sdk; PY=$S/lib/py3-mac-universal/python; else S=$R/sdk/renpy-7.8.7-patched; PY=$S/lib/py2-mac-universal/python; fi
    out="$R/spread/out/$g/$engine"; rm -rf "$out"; mkdir -p "$out/saves"
    rm -f "$base/traceback.txt" "$base/log.txt"
    VND_PROBE_DELAY=10 VND_PROBE_OUT="$out" RENPY_PATH_TO_SAVES="$out/saves" timeout 75 "$PY" "$S/renpy.py" "$base" > "$out/stdout.txt" 2>&1
    cp "$base/log.txt" "$out/" 2>/dev/null; cp "$base/traceback.txt" "$out/" 2>/dev/null
    steps=$(grep -c "VNDPROBE step" "$out/log.txt" 2>/dev/null)
    started=$(grep -q "VNDPROBE step start" "$out/log.txt" 2>/dev/null && ! grep -q "no main menu" "$out/log.txt" && echo started || echo no-start)
    err=$( [ -f "$out/traceback.txt" ] && grep -m1 -E "^[A-Za-z]+(Error|Exception)[^ ]*:" "$out/traceback.txt" | cut -c1-110 )
    [ -z "$err" ] && err=$(grep -m1 -E "Error|Exception" "$out/stdout.txt" | cut -c1-110)
    echo "$g | own $own | engine $engine | probe steps ${steps:-0}/6 | $started | ${err:-no error}"
  done
done
