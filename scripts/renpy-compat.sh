#!/usr/bin/env bash
# Runs on a Mac with a logged-in desktop. Boots every Ren'Py game in a folder on the runtime VNDegen would pick, with the
# probe in zz_vndegen.rpy, and prints a Markdown table: the runtime and why, whether the game reached its main menu,
# and whether its folder stayed unchanged. Screenshots and logs go to --out (default: a temporary folder).
#   scripts/renpy-compat.sh <games folder> [--runtime 7|8] [--timeout 45] [--out <folder>]
# --runtime forces one runtime for every game, to see which games the other one would also run.
set -euo pipefail
cd "$(dirname "$0")/.."
scripts/renpy-runtime.sh build/renpy >&2
swift build --package-path Packages/VNDegenKit --product RenPyCompat -q >&2
"$(swift build --package-path Packages/VNDegenKit --show-bin-path)/RenPyCompat" "${1:?games folder}" --runtimes build/renpy "${@:2}"
