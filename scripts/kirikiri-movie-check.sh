#!/usr/bin/env bash
# Runs on a Mac. Plays generated movies through the KiriKiri engine's VideoOverlay in overlay and
# layer mode, with loops, a segment loop, a period event, a seek, a script pause, and an engine pause,
# using the script-only game in Engines/KiriKiri/vndegen/movie-check/, and checks what its script saw:
#   scripts/kirikiri-movie-check.sh build/kirikiri/mac/KiriKiriHelper <work folder>
# The work folder gets the game, its movies (generated with Homebrew's ffmpeg), and check.log.
# Exits nonzero if a check fails.
set -euo pipefail
helper=${1:?KiriKiriHelper path}
work=${2:?work folder}
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$work"
work=$(cd "$work" && pwd)
log="$work/check.log"

cp "$root/Engines/KiriKiri/vndegen/movie-check/startup.tjs" "$work/"
ffmpeg -v error -y -f lavfi -i color=c=red:s=320x240:r=30:d=3 -f lavfi -i sine=f=440:d=3:sample_rate=44100 \
  -c:v mpeg1video -b:v 1M -c:a mp2 -ac 2 -f mpeg "$work/red.mpg"
ffmpeg -v error -y -f lavfi -i color=c=blue:s=320x240:r=30:d=3 -f lavfi -i sine=f=660:d=3:sample_rate=44100 \
  -c:v wmv2 -b:v 1M -c:a wmav2 -ac 2 -f asf "$work/blue.wmv"

rm -f "$log"
"$helper" --game-root "$work" --data "$work/" --log "$log" >"$work/helper.out" 2>&1 &
pid=$!
seen() { grep -aq "$1" "$log" 2>/dev/null; }
now() { perl -MTime::HiRes=time -e 'printf "%.2f", time'; }

deadline=$(($(date +%s) + 120))
paused=false
while kill -0 $pid 2>/dev/null; do
  if ! $paused && seen "check: engine pause step playing"; then
    started=$(now)
    sleep 1
    kill -USR1 $pid
    sleep 2
    kill -USR2 $pid
    paused=true
  fi
  if $paused && [ -z "${stopped:-}" ] && seen "check: engine pause step stopped"; then
    stopped=$(now)
  fi
  if [ "$(date +%s)" -gt "$deadline" ]; then
    # A script error opens a modal dialog, which SIGTERM cannot close.
    echo "fail: the check did not finish in 120 s" >&2
    kill -KILL $pid
    break
  fi
  sleep 0.1
done
wait $pid || true

grep -a "check: \|movie: \|exception\|rror" "$log" | sed 's/^[0-9:]* //' || true
failed=0
grep -aq "check: fail" "$log" && failed=1
seen "check: done" || { echo "fail: the check did not reach its end"; failed=1; }

# The movie the engine pause interrupted is the last one the player summarized.
summary=$(grep -a "movie: red.mpg ended at" "$log" | tail -1 || true)
dropped=$(sed -n 's/.* \([0-9]*\) dropped.*/\1/p' <<<"$summary")
elapsed=$(perl -e "printf '%.1f', ${stopped:-0} - ${started:-0}")
if [ -n "${stopped:-}" ] && [ "${dropped:-99}" -le 3 ] && perl -e "exit !($elapsed >= 4.5)"; then
  echo "check: pass engine pause holds the movie ($elapsed s for a 3 s movie paused 2 s, $dropped frames dropped)"
else
  echo "check: fail engine pause holds the movie (${elapsed} s for a 3 s movie paused 2 s, ${dropped:-?} frames dropped)"
  failed=1
fi
[ $failed = 0 ] && echo "movie check passed" || echo "movie check failed"
exit $failed
