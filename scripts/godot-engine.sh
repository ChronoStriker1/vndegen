#!/usr/bin/env bash
# Runs on a Mac. Builds the Godot engines from Engines/Godot:
#   <dest>/Godot4.xcframework   Godot 4 for every Godot 4 game: iOS device and simulator, and macOS, all arm64. It
#                               holds Godot built as static libraries with VNDegen's module (Engines/Godot/4/vndegen),
#                               and exports only the functions in vngd.h
#   <dest>/licenses/            Godot's license and its third-party notices, one text file each
#   scripts/godot-engine.sh build/godot
# Godot comes from the pinned commit below, with Engines/Godot/4/patches applied. SCons comes from PyPI into a venv.
# Sources, SCons, and the build folders are cached in ~/Library/Caches/VNDegen/godot, so a change rebuilds only what
# it touches. A build whose inputs have not changed since the last one is skipped. VNDEGEN_GODOT_SLICES picks slices,
# such as "ios-sim", for a quicker build while testing.
set -euo pipefail
dest=${1:?destination folder, such as build/godot}
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/Engines/Godot"
api="$root/Packages/VNDegenKit/Sources/CGodot/include"
cache=${VNDEGEN_GODOT_CACHE:-$HOME/Library/Caches/VNDegen/godot}
jobs=${VNDEGEN_GODOT_JOBS:-$(sysctl -n hw.ncpu)}
slices=${VNDEGEN_GODOT_SLICES:-ios ios-sim mac}

godot4_repo=https://github.com/godotengine/godot
godot4_commit=ed1daf0bf001b61586d9930840f2f1394092c079 # 4.7.2-stable
scons_version=4.10.1

mkdir -p "$dest" "$cache"
dest=$(cd "$dest" && pwd)
stamp=$( (cat "$0" "$api/vngd.h"; echo "$slices"; cd "$src" && find . -path ./4/test-host -prune -o -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "Godot engines are up to date in $dest"
  exit 0
fi
start=$(date +%s)

# fetch <repo> <commit> <folder>: a clean checkout of one commit, kept in the cache.
fetch() {
  [ -f "$3/.vndegen-commit" ] && [ "$(cat "$3/.vndegen-commit")" = "$2" ] && return
  rm -rf "$3.part" && mkdir -p "$3.part"
  git -C "$3.part" init --quiet
  git -C "$3.part" fetch --quiet --depth 1 "$1" "$2"
  git -C "$3.part" checkout --quiet FETCH_HEAD
  echo "$2" > "$3.part/.vndegen-commit"
  rm -rf "$3" && mv "$3.part" "$3"
}

if [ ! -x "$cache/venv/bin/scons" ] || [ "$("$cache/venv/bin/scons" --version | sed -n 's/.*SCons: v\([0-9.]*\).*/\1/p' | head -1)" != "$scons_version" ]; then
  rm -rf "$cache/venv"
  python3 -m venv "$cache/venv"
  "$cache/venv/bin/pip" install --quiet "scons==$scons_version"
fi
scons="$cache/venv/bin/scons"

fetch "$godot4_repo" "$godot4_commit" "$cache/src/godot-$godot4_commit"

# The patched sources are made fresh, then copied over the sources the builds use by content, so unchanged files keep
# their times and SCons rebuilds only what changed.
work="$cache/work"
rm -rf "$work/fresh" && mkdir -p "$work/fresh"
rsync -a --exclude .git "$cache/src/godot-$godot4_commit/" "$work/fresh/godot4/"
for patch in "$src"/4/patches/*.patch; do patch -s -p1 -d "$work/fresh/godot4" < "$patch"; done
mkdir -p "$work/godot4"
rsync -a --checksum --exclude bin --exclude '*.o' --exclude '*.a' --exclude '.sconsign*' "$work/fresh/godot4/" "$work/godot4/"
rm -rf "$work/fresh"

# iOS builds are always a static library, which Godot builds without its library flag, so the restart support that
# the flag turns on is turned on by hand there.
ios_library=(ccflags=-DLIBGODOT_ENABLED)
# What VNDegen's Godot leaves out: other renderers and input, the editor's and VR's modules, and the parts the
# permissive license choice drops (graphite).
common=(target=template_release custom_modules="$src/4" disable_path_overrides=no
  sdl=no graphite=no vulkan=no opengl3=no accesskit=no module_openxr_enabled=no module_mono_enabled=no
  module_camera_enabled=no module_webxr_enabled=no module_mobile_vr_enabled=no debug_symbols=no)

# build_slice <name> <scons platform args...>: the static library, then the framework around it.
build_slice() {
  local name=$1
  shift
  echo "Building Godot 4 for $name"
  (cd "$work/godot4" && "$scons" -j "$jobs" "${common[@]}" "$@") >"$work/godot4-$name.log" 2>&1 ||
    { grep -E 'error|ERROR' "$work/godot4-$name.log" | head -n 40; tail -n 20 "$work/godot4-$name.log"; exit 1; }
}

link_framework() { # <name> <sdk> <target triple> <library>
  local name=$1 sdk=$2 triple=$3 library=$4 out="$work/framework-$1/Godot4.framework"
  rm -rf "$out" && mkdir -p "$out"
  sed -n 's/^VNGD_API [^(]* \**\(vngd_[a-z_]*\)(.*/_\1/p' "$api/vngd.h" > "$work/exports.txt"
  local frameworks=(Foundation CoreFoundation CoreGraphics CoreText CoreServices QuartzCore Metal IOSurface
    AudioToolbox AVFoundation CoreAudio CoreMedia CoreVideo GameController Security SystemConfiguration ImageIO
    UniformTypeIdentifiers)
  # Godot upscales with MetalFX where the system has it. The simulator SDK doesn't, nor does its Metal have two error
  # domains that metal-cpp imports weakly and checks for null.
  local extra=()
  if [ "$sdk" = iphonesimulator ]; then
    extra=(-Wl,-U,_MTLIOErrorDomain -Wl,-U,_MTLTensorDomain)
  else
    frameworks+=(MetalFX)
  fi
  if [ "$sdk" = macosx ]; then
    frameworks+=(AppKit Carbon IOKit CoreHaptics)
  else
    frameworks+=(UIKit CoreMotion CoreHaptics MediaPlayer)
  fi
  xcrun -sdk "$sdk" clang++ -target "$triple" -dynamiclib -o "$out/Godot4" -install_name @rpath/Godot4.framework/Godot4 \
    -Wl,-force_load,"$library" -Wl,-exported_symbols_list,"$work/exports.txt" -Wl,-dead_strip \
    $(printf -- '-framework %s ' "${frameworks[@]}") ${extra[@]+"${extra[@]}"} -lz \
    >"$work/link-$name.log" 2>&1 || { head -n 60 "$work/link-$name.log" >&2; exit 1; }
  local platform=iPhoneOS
  case "$name" in ios-sim) platform=iPhoneSimulator ;; mac) platform=MacOSX ;; esac
  sed -e "s/@PLATFORM@/$platform/" "$src/Info.plist" > "$out/Info.plist"
}

for slice in $slices; do
  case "$slice" in
    ios-sim)
      build_slice ios-sim platform=ios arch=arm64 simulator=yes "${ios_library[@]}"
      link_framework ios-sim iphonesimulator arm64-apple-ios18.0-simulator "$(ls -t "$work"/godot4/bin/libgodot.ios.template_release.arm64.simulator.a | head -1)" ;;
    ios)
      build_slice ios platform=ios arch=arm64 "${ios_library[@]}"
      link_framework ios iphoneos arm64-apple-ios18.0 "$(ls -t "$work"/godot4/bin/libgodot.ios.template_release.arm64.a | head -1)" ;;
    mac)
      build_slice mac platform=macos arch=arm64 library_type=static_library
      link_framework mac macosx arm64-apple-macos15.0 "$(ls -t "$work"/godot4/bin/libgodot.macos.template_release.arm64.a | head -1)" ;;
    *) echo "unknown slice $slice" >&2; exit 2 ;;
  esac
done

rm -rf "$dest/Godot4.xcframework"
args=()
for slice in $slices; do args+=(-framework "$work/framework-$slice/Godot4.framework"); done
xcodebuild -create-xcframework "${args[@]}" -output "$dest/Godot4.xcframework" >/dev/null

mkdir -p "$dest/licenses"
cp "$work/godot4/LICENSE.txt" "$dest/licenses/Godot.txt"
cp "$work/godot4/COPYRIGHT.txt" "$dest/licenses/Godot third-party notices.txt"
echo "$stamp" > "$dest/.vndegen-engine"
echo "Godot engines built in $dest in $(($(date +%s) - start)) s"
