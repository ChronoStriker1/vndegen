#!/bin/bash
# run.sh <core> <game folder> <run name> <frames> <shots> [inputs]: runs the harness with the RTPs and VNDegen's preload.
set -u
W=~/vndegen-rgss-work
out=$W/runs/$3
rm -rf "$out"; mkdir -p "$out/system/mkxp-z/Scripts/Preload" "$out/system/mkxp-z"
cp $W/harness/preload/zz_vndegen.rb "$out/system/mkxp-z/Scripts/Preload/"
[ -n "${SAVES_FROM:-}" ] && cp -R "$W/runs/$SAVES_FROM/saves" "$out/saves"
ln -s ~/vndegen-testgames/rgss/rtp "$out/system/mkxp-z/RTP"
env "RGSS_OPT_$(cat $W/harness/preload/key)=enabled" timeout 600 ${HARNESS:-$W/angle-only/build-mac/rgss-harness} "$1" "$2/Game.ini" "$out" "$4" "$5" "${6:-}" > "$out.log" 2>&1
echo "exit=$?"
grep -v -E 'unhandled environment|Skipping nonexistent|VM memory grown' "$out.log" | tail -${TAILN:-15}
ls "$out" | grep png
