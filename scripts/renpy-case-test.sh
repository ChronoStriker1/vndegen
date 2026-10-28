#!/usr/bin/env bash
# Runs scripts/renpy-case-test.py, the check of filecase.py, on a case-sensitive disk such as Linux's, under python3 and
# Python 2.7 (Ren'Py 7's Python on macOS). Without a python2, Python 2.7 runs in Docker's python:2.7-slim image when
# Docker is available.
#   scripts/renpy-case-test.sh [Ren'Py folder ...]
# A Ren'Py folder holds renpy.py and renpy/, such as an SDK. The default is build/renpy/8 and build/renpy/7 if present.
set -euo pipefail
cd "$(dirname "$0")/.."
# The ${a[@]+...} forms keep empty arrays working under set -u in macOS's bash 3.2.
folders=("$@")
if [ $# = 0 ]; then
  for folder in build/renpy/8 build/renpy/7; do
    if [ -f "$folder/renpy.py" ]; then folders+=("$PWD/$folder"); fi
  done
fi

failed=0
python3 -I scripts/renpy-case-test.py ${folders[@]+"${folders[@]}"} || failed=1
if command -v python2 > /dev/null; then
  python2 -E -s scripts/renpy-case-test.py ${folders[@]+"${folders[@]}"} || failed=1
elif command -v docker > /dev/null; then
  mounts=() inside=()
  for folder in ${folders[@]+"${folders[@]}"}; do
    folder=$(cd "$folder" && pwd)
    mounts+=(-v "$folder:/renpy/${#inside[@]}-$(basename "$folder"):ro")
    inside+=("/renpy/${#inside[@]}-$(basename "$folder")")
  done
  docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/src:ro" ${mounts[@]+"${mounts[@]}"} python:2.7-slim \
    python /src/scripts/renpy-case-test.py ${inside[@]+"${inside[@]}"} || failed=1
else
  echo "Python 2.7 skipped: no python2 or docker"
fi
[ "$failed" = 0 ] && echo PASS
exit "$failed"
