#!/usr/bin/env bash
# Runs on a Mac. Installs the Ren'Py runtimes the EngineRenPy spike launches, into
# ~/Library/Application Support/VNDegen/RenPy/{8,7}. Each is an official Ren'Py SDK trimmed to what runs games.
#   scripts/renpy-runtime.sh
# Ren'Py 7.8.7 gets one patch. Its loader compiles a game's python-packages modules from text, which Python 2
# rejects when the module declares an encoding. 7.5.1 compiled bytes, and the patch restores that.
set -euo pipefail
dest="$HOME/Library/Application Support/VNDegen/RenPy"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

install() { # <version> <folder> <python line>
  local version=$1 folder=$2 py=$3
  echo "Ren'Py $version -> $dest/$folder"
  curl -fsSL "https://www.renpy.org/dl/$version/renpy-$version-sdk.tar.bz2" -o "$work/sdk.tar.bz2"
  curl -fsSL "https://www.renpy.org/dl/$version/checksums.txt" -o "$work/checksums.txt"
  grep -qx "$(shasum -a 256 "$work/sdk.tar.bz2" | cut -d' ' -f1) renpy-$version-sdk.tar.bz2" "$work/checksums.txt"
  tar -xjf "$work/sdk.tar.bz2" -C "$work"
  local sdk="$work/renpy-$version-sdk"
  rm -rf "$dest/$folder" && mkdir -p "$dest/$folder/lib"
  cp -R "$sdk/renpy.py" "$sdk/renpy" "$dest/$folder/"
  cp -R "$sdk/lib/$py-mac-universal" "$sdk/lib/python"* "$dest/$folder/lib/"
}

install 8.5.3 8 py3
install 7.8.7 7 py2

loader="$dest/7/renpy/loader.py"
python3 - "$loader" <<'EOF'
import sys
path = sys.argv[1]
source = open(path).read()
anchor = "                code = compile(source, filename, 'exec', renpy.python.old_compile_flags, 1)"
assert source.count(anchor) == 1
fix = '                source = source.encode("raw_unicode_escape").replace(b"\\r", b"")\n'
open(path, "w").write(source.replace(anchor, fix + anchor))
EOF
rm -f "$dest/7/renpy/loader.pyo" "$dest/7/renpy/loader.pyc"
du -sh "$dest"/*
