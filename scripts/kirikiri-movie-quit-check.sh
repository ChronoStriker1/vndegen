#!/usr/bin/env bash
# Runs on a Mac. Quits a game in KiriKiriRestart while its movie plays, runs times in one process,
# using the script-only game in Engines/KiriKiri/vndegen/movie-quit/:
#   scripts/kirikiri-movie-quit-check.sh build/kirikiri/restart-asan/KiriKiriRestart <work folder> [runs]
# The movie is in movie.xp3, read through the game's xp3filter.tjs, and the game never closes it,
# so its decoder runs until engine shutdown. With the restart-asan build, any sanitizer report
# fails the check. The work folder gets the game, its movie (generated with Homebrew's ffmpeg), and
# the logs. Exits nonzero if the harness fails or a sanitizer reports.
set -euo pipefail
harness=${1:?KiriKiriRestart path}
work=${2:?work folder}
runs=${3:-3}
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$work/game" "$work/logs"
work=$(cd "$work" && pwd)
game="$work/game"

cp "$root/Engines/KiriKiri/vndegen/movie-quit/"*.tjs "$game/"
ffmpeg -v error -y -f lavfi -i testsrc=s=640x480:r=30:d=10 -f lavfi -i sine=f=440:d=10:sample_rate=44100 \
  -c:v mpeg1video -b:v 4M -c:a mp2 -ac 2 -f mpeg "$work/loop.mpg"
# An XP3 archive holding loop.mpg uncompressed: the header, the file, then the index.
python3 - "$work/loop.mpg" "$game/movie.xp3" <<'PY'
import struct, sys, zlib
data = open(sys.argv[1], 'rb').read()
name = 'loop.mpg'.encode('utf-16-le')
def chunk(tag, body): return tag + struct.pack('<Q', len(body)) + body
info = struct.pack('<IQQH', 0, len(data), len(data), len(name) // 2) + name
segm = struct.pack('<IQQQ', 0, 19, len(data), len(data))
index = chunk(b'File', chunk(b'info', info) + chunk(b'segm', segm) + chunk(b'adlr', struct.pack('<I', zlib.adler32(data))))
header = b'XP3\r\n \n\x1a\x8b\x67\x01' + struct.pack('<Q', 19 + len(data))
open(sys.argv[2], 'wb').write(header + data + b'\0' + struct.pack('<Q', len(index)) + index)
PY

args=()
for _ in $(seq "$runs"); do args+=("$game" "$game/" movie); done
status=0
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 "$harness" "$work/logs" "${args[@]}" >"$work/harness.out" 2>&1 || status=$?
grep -a "^game \|games failed" "$work/harness.out" || true
reports=$(grep -a -c "ERROR: AddressSanitizer\|runtime error" "$work/harness.out" || true)
grep -a -m1 -A20 "ERROR: AddressSanitizer\|runtime error" "$work/harness.out" || true
if [ "$status" = 0 ] && [ "$reports" = 0 ]; then
  echo "movie quit check passed"
else
  echo "movie quit check failed: harness exit $status, $reports sanitizer reports"
  exit 1
fi
