#!/usr/bin/env bash
# Runs on a Mac. Sets up the case check every engine plugin must pass: a test game whose every file and folder name has
# its case swapped (`img/Title.png` becomes `IMG/tITLE.PNG`), in a library on a case-sensitive volume, as iPhones and
# iPads have. Every name the game asks for then differs in case from the disk, in both directions.
#   scripts/case-check.sh setup <folder> <game folder>...
#       mounts a case-sensitive volume at <folder>/volume (the image is <folder>/case-sensitive.sparseimage) and copies
#       each game into <folder>/volume/library/Games with its names swapped. The source games are only read.
#   scripts/case-check.sh teardown <folder>
#       unmounts the volume. Quit the app first.
# Then run the app on that library and play each game to its title, start, save, and load:
#   Mac:      VNDegen.app/Contents/MacOS/VNDegen -libraryRoot <folder>/volume/library -autolaunch "<game folder name>"
#   iPad simulator, through the Device panel's launcher:
#             open com.chronostriker1.vndegen --relaunch --launch-args -libraryRoot --launch-args <folder>/volume/library
#             --launch-args -autolaunch --launch-args "<game folder name>"
# A simulator app reads the Mac's disk, so the library on the case-sensitive volume is what makes the simulator check
# case. Names that Windows matches exactly too keep their case: Python modules and packages, which Python imports by
# exact name on Windows; Ren'Py's scripts and every folder above them, since Ren'Py names a script module by its path
# (`tl/None/common`) and compares names exactly; and the extension of a Ren'Py archive (`ARCHIVE.rpa` but not
# `archive.RPA`). Names inside archives (`.xp3`, `.rpa`, `.rgss3a`, `app.asar`) keep their case; the engines' archive readers
# are checked by their unit tests.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)

case "${1:?setup or teardown}" in
  setup)
    folder=${2:?folder for the volume and its image}
    shift 2
    [ $# -gt 0 ] || { echo "error: name at least one game folder" >&2; exit 2; }
    mkdir -p "$folder"
    folder=$(cd "$folder" && pwd)
    mount -t apfs | grep -q " on $folder/volume " || "$root/scripts/case-volume.sh" attach "$folder/case-sensitive.sparseimage" "$folder/volume"
    games="$folder/volume/library/Games"
    mkdir -p "$games"
    for game in "$@"; do
      name=$(basename "$game")
      rm -rf "${games:?}/$name"
      # The game's own folder keeps its name, so -autolaunch and the library list still name it as before.
      rsync -a --exclude .DS_Store "$game/" "$games/$name/"
      python3 - "$games/$name" <<'PY'
import os, sys
top = sys.argv[1]
renamed = 0
exact = {".py", ".pyc", ".pyo", ".pyd", ".so", ".rpy", ".rpyc", ".rpym", ".rpymc", ".rpyb"}
renpy = {".rpa", ".rpi"}
# Every folder that holds a script somewhere below it.
scripts = set()
for parent, folders, files in os.walk(top):
    if any(os.path.splitext(name)[1].lower() in exact for name in files):
        folder = parent
        while folder != top and folder not in scripts:
            scripts.add(folder)
            folder = os.path.dirname(folder)
def python(path):
    parts = os.path.relpath(path, top).split(os.sep)
    return path in scripts or "python-packages" in parts or os.path.exists(os.path.join(path, "__init__.py"))
# Deepest first, so a folder is renamed after everything inside it.
for parent, folders, files in os.walk(top, topdown=False):
    for name in folders + files:
        path = os.path.join(parent, name)
        stem, ext = os.path.splitext(name)
        if python(path) or ext.lower() in exact:
            continue
        swapped = stem.swapcase() + ext if ext.lower() in renpy else name.swapcase()
        if swapped != name:
            os.rename(os.path.join(parent, name), os.path.join(parent, swapped))
            renamed += 1
print(f"{os.path.basename(top)}: swapped the case of {renamed} names")
PY
    done
    echo "library: $folder/volume/library" ;;
  teardown)
    folder=$(cd "${2:?folder}" && pwd)
    "$root/scripts/case-volume.sh" detach "$folder/volume" ;;
  *) echo "unknown command $1" >&2; exit 2 ;;
esac
