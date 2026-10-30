#!/usr/bin/env bash
# Runs on a Mac. Builds the Godot engines from Engines/Godot:
#   <dest>/Godot4.xcframework   Godot 4.7 for every Godot 4 game
#   <dest>/Godot3.xcframework   Godot 3.6 for every Godot 3 game
#   <dest>/licenses/            Godot's license and its third-party notices, one text file each
#   scripts/godot-engine.sh build/godot
# Each framework holds iOS device and simulator and macOS slices, all arm64, of Godot built as static libraries with
# VNDegen's module (Engines/Godot/<major>/vndegen), and exports only the functions in vngd.h. Godot comes from the pinned
# commits below, with Engines/Godot/<major>/patches applied. SCons comes from PyPI into a venv. Sources, SCons, and the
# build folders are cached in ~/Library/Caches/VNDegen/godot, so a change rebuilds only what it touches. A build whose
# inputs have not changed since the last one is skipped. VNDEGEN_GODOT_SLICES picks slices, such as "ios-sim mac", and
# VNDEGEN_GODOT_MAJORS picks engines, such as "4", for a quicker build while testing.
set -euo pipefail
dest=${1:?destination folder, such as build/godot}
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/Engines/Godot"
api="$root/Packages/VNDegenKit/Sources/CGodot/include"
cache=${VNDEGEN_GODOT_CACHE:-$HOME/Library/Caches/VNDegen/godot}
jobs=${VNDEGEN_GODOT_JOBS:-$(sysctl -n hw.ncpu)}
slices=${VNDEGEN_GODOT_SLICES:-ios ios-sim mac}
majors=${VNDEGEN_GODOT_MAJORS:-4 3}

godot_repo=https://github.com/godotengine/godot
godot4_commit=ed1daf0bf001b61586d9930840f2f1394092c079 # 4.7.2-stable
godot3_commit=a3378686f7196db78d7b3cc7947f72a13bc4864c # 3.6.3-stable
scons_version=4.10.1

mkdir -p "$dest" "$cache"
dest=$(cd "$dest" && pwd)
stamp=$( (cat "$0" "$api/vngd.h" "$root/Engines/Shared/vnfont.c" "$root/Engines/Shared/vnfont.h" "$root/Engines/Shared/casepath.c" "$root/Engines/Shared/casepath.h"; echo "$slices $majors"; cd "$src" && find . -path ./test-host -prune -o -type f ! -name "*.o" -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "Godot engines are up to date in $dest"
  exit 0
fi
start=$(date +%s)

# Every checkout shares the cache's work folder, so one build at a time holds this lock. A lock whose build has ended
# is taken over.
lock="$cache/lock"
until mkdir "$lock" 2>/dev/null; do
  holder=$(cat "$lock/pid" 2>/dev/null || true)
  if [ -n "$holder" ] && ! kill -0 "$holder" 2>/dev/null; then rm -rf "$lock"; continue; fi
  echo "Waiting for another Godot build${holder:+ (pid $holder)}"
  sleep 15
done
echo $$ > "$lock/pid"
trap 'rm -rf "$lock"' EXIT

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
work="$cache/work"

# prepare <major> <commit>: the patched sources in $work/godot<major>. They are made fresh, then copied over the
# sources the builds use by content, so unchanged files keep their times and SCons rebuilds only what changed.
prepare() {
  local major=$1 commit=$2
  fetch "$godot_repo" "$commit" "$cache/src/godot-$commit"
  rm -rf "$work/fresh" && mkdir -p "$work/fresh" "$work/godot$major"
  rsync -a --exclude .git "$cache/src/godot-$commit/" "$work/fresh/godot/"
  for patch in "$src/$major"/patches/*.patch; do
    [ -f "$patch" ] && patch -s -p1 -d "$work/fresh/godot" < "$patch"
  done
  # --delete removes sources the patched tree doesn't have, such as another version's. Build outputs and the files
  # SCons generates are excluded, so they stay.
  rsync -a --checksum --delete --exclude bin --exclude '*.o' --exclude '*.a' --exclude '.sconsign*' --exclude '*.gen.*' "$work/fresh/godot/" "$work/godot$major/"
  rm -rf "$work/fresh"
}

# MetalANGLE, OpenGL ES on Metal, which the app already embeds for Ren'Py. Godot 3 draws through it on iPhone and
# iPad. Godot 4 does in the iOS Simulator, whose Metal device offers too little of Metal for Godot's renderer.
angle_framework() { # <slice>
  case "$1" in
    ios-sim) echo "$root/build/renpy/ios/MetalANGLE.xcframework/ios-arm64_i386_x86_64-simulator" ;;
    ios) echo "$root/build/renpy/ios/MetalANGLE.xcframework/ios-arm64_armv7" ;;
  esac
}
case " $slices " in
  *" ios"*) [ -d "$(angle_framework ios)" ] || "$root/scripts/renpy-runtime.sh" "$root/build/renpy" ios >/dev/null ;;
esac
angle_flags() { # <slice>: compiler flags that find MetalANGLE's headers
  echo "-DVNDEGEN_ANGLE -I$(angle_framework "$1")/MetalANGLE.framework/Headers -F$(angle_framework "$1")"
}

# build <major> <slice> <scons args...>: the static libraries.
build() {
  local major=$1 slice=$2
  shift 2
  echo "Building Godot $major for $slice"
  (cd "$work/godot$major" && "$scons" -j "$jobs" custom_modules="$src/$major" "$@") >"$work/godot$major-$slice.log" 2>&1 ||
    { grep -E 'error|ERROR' "$work/godot$major-$slice.log" | head -n 40; tail -n 20 "$work/godot$major-$slice.log"; exit 1; }
}

# link_framework <major> <slice> <library> [more libraries]: the framework, from a library it loads whole, or none
# when <library> is empty, and the libraries it takes what it needs from. vngd.h's functions are what it needs first.
link_framework() {
  local major=$1 slice=$2 library=$3 name="Godot$1"
  shift 3
  local framework="$work/framework-$major-$slice/$name.framework" binary resources sdk triple
  case "$slice" in
    ios-sim) sdk=iphonesimulator triple=arm64-apple-ios18.0-simulator ;;
    ios) sdk=iphoneos triple=arm64-apple-ios18.0 ;;
    mac) sdk=macosx triple=arm64-apple-macos15.0 ;;
  esac
  rm -rf "$framework" && mkdir -p "$framework"
  if [ "$sdk" = macosx ]; then
    # A macOS framework keeps its files in a version folder, with links at its top.
    mkdir -p "$framework/Versions/A/Resources"
    ln -s A "$framework/Versions/Current"
    ln -s "Versions/Current/$name" "$framework/$name"
    ln -s Versions/Current/Resources "$framework/Resources"
    binary="$framework/Versions/A/$name"
    resources="$framework/Versions/A/Resources"
  else
    binary="$framework/$name"
    resources="$framework"
  fi
  sed -n 's/^VNGD_API [^(]* \**\(vngd_[a-z_]*\)(.*/_\1/p' "$api/vngd.h" > "$work/exports.txt"
  local frameworks=(Foundation CoreFoundation CoreGraphics CoreText CoreServices QuartzCore Metal IOSurface
    AudioToolbox AVFoundation CoreAudio CoreMedia CoreVideo GameController Security SystemConfiguration ImageIO
    UniformTypeIdentifiers)
  local extra=()
  if [ "$sdk" = macosx ]; then
    frameworks+=(AppKit Carbon IOKit CoreHaptics AudioUnit CoreMIDI OpenGL)
  else
    frameworks+=(UIKit CoreMotion CoreHaptics MediaPlayer)
  fi
  if [ "$major" = 4 ] && [ "$sdk" = iphonesimulator ]; then
    # MetalANGLE before OpenGLES, so the renderer's OpenGL calls bind to MetalANGLE. Godot's own iOS view, which VNDegen
    # doesn't use, still needs OpenGLES's EAGL classes. The simulator's Metal has neither MetalFX nor two error domains
    # that metal-cpp imports weakly and checks for null.
    extra=(-Wl,-U,_MTLIOErrorDomain -Wl,-U,_MTLTensorDomain -F"$(angle_framework "$slice")" -framework MetalANGLE -framework OpenGLES)
  elif [ "$major" = 4 ]; then
    # Godot 4 upscales with MetalFX where the system has it.
    frameworks+=(MetalFX)
  elif [ "$sdk" != macosx ]; then
    extra=(-F"$(angle_framework "$slice")" -framework MetalANGLE)
  fi
  local load=()
  [ -n "$library" ] && load=(-Wl,-force_load,"$library")
  xcrun -sdk "$sdk" clang++ -target "$triple" -dynamiclib -o "$binary" -install_name "@rpath/$name.framework/$name" \
    ${load[@]+"${load[@]}"} $(sed 's/^/-Wl,-u,/' "$work/exports.txt") "$@" \
    -Wl,-exported_symbols_list,"$work/exports.txt" -Wl,-dead_strip \
    $(printf -- '-framework %s ' "${frameworks[@]}") ${extra[@]+"${extra[@]}"} -lz \
    >"$work/link-$major-$slice.log" 2>&1 || { grep -v 'no longer supported' "$work/link-$major-$slice.log" | head -n 60 >&2; exit 1; }
  local platform=iPhoneOS
  case "$slice" in ios-sim) platform=iPhoneSimulator ;; mac) platform=MacOSX ;; esac
  sed -e "s/@PLATFORM@/$platform/" -e "s/@NAME@/$name/g" "$src/Info.plist" > "$resources/Info.plist"
  if [ "$sdk" = macosx ]; then
    plutil -remove MinimumOSVersion "$resources/Info.plist"
    plutil -insert LSMinimumSystemVersion -string 15.0 "$resources/Info.plist"
  fi
}

# Godot 4. What VNDegen's build leaves out: other renderers and input, the editor's and VR's modules, and the parts the
# permissive license choice drops (graphite). iOS builds are always a static library, which Godot builds without its
# library flag, so the restart support that the flag turns on is turned on by hand there.
godot4() {
  prepare 4 "$godot4_commit"
  local common=(target=template_release disable_path_overrides=no sdl=no graphite=no vulkan=no opengl3=no accesskit=no
    module_openxr_enabled=no module_mono_enabled=no module_camera_enabled=no module_webxr_enabled=no
    module_mobile_vr_enabled=no debug_symbols=no)
  for slice in $slices; do
    case "$slice" in
      ios-sim)
        build 4 ios-sim "${common[@]}" platform=ios arch=arm64 simulator=yes opengl3=yes "ccflags=-DLIBGODOT_ENABLED $(angle_flags ios-sim)"
        link_framework 4 ios-sim "$work/godot4/bin/libgodot.ios.template_release.arm64.simulator.a" ;;
      ios)
        build 4 ios "${common[@]}" platform=ios arch=arm64 ccflags=-DLIBGODOT_ENABLED
        link_framework 4 ios "$work/godot4/bin/libgodot.ios.template_release.arm64.a" ;;
      mac)
        build 4 mac "${common[@]}" platform=macos arch=arm64 library_type=static_library
        # The macOS build leaves Godot in one library per part. VNDegen's module holds vngd.h's functions.
        link_framework 4 mac "$work/godot4/bin/obj/modules/libmodule_vndegen.macos.template_release.arm64.a" \
          "$work/godot4/bin/libgodot.macos.template_release.arm64.a" \
          $(find "$work/godot4/bin/obj" -name '*.macos.template_release.arm64.a' ! -name 'libmodule_vndegen.*') ;;
      *) echo "unknown slice $slice" >&2; exit 2 ;;
    esac
  done
}

# Godot 3. It draws with OpenGL: OpenGL ES through MetalANGLE on iPhone and iPad, Apple's OpenGL on the Mac. Godot 3
# leaves each part in a library of its own folder. The framework takes what it needs from every one but the iOS and
# macOS platforms' own, whose OS and app VNDegen's module replaces.
godot3() {
  prepare 3 "$godot3_commit"
  local common=(target=release tools=no debug_symbols=no module_mono_enabled=no module_camera_enabled=no
    module_mobile_vr_enabled=no module_webxr_enabled=no module_upnp_enabled=no)
  local suffix
  for slice in $slices; do
    case "$slice" in
      ios-sim)
        build 3 ios-sim "${common[@]}" platform=iphone arch=arm64 ios_simulator=yes "CCFLAGS=$(angle_flags ios-sim)"
        suffix=iphone.opt.arm64.simulator ;;
      ios)
        build 3 ios "${common[@]}" platform=iphone arch=arm64 "CCFLAGS=$(angle_flags ios)"
        suffix=iphone.opt.arm64 ;;
      mac)
        build 3 mac "${common[@]}" platform=osx arch=arm64
        suffix=osx.opt.arm64 ;;
      *) echo "unknown slice $slice" >&2; exit 2 ;;
    esac
    link_framework 3 "$slice" "" \
      $(cd "$work/godot3" && find . -name "*.$suffix.a" ! -path './platform/iphone/*' ! -path './platform/osx/*' ! -path './bin/*' | sed "s|^\.|$work/godot3|")
  done
}

for major in $majors; do "godot$major"; done

for major in $majors; do
  rm -rf "$dest/Godot$major.xcframework"
  args=()
  for slice in $slices; do args+=(-framework "$work/framework-$major-$slice/Godot$major.framework"); done
  xcodebuild -create-xcframework "${args[@]}" -output "$dest/Godot$major.xcframework" >/dev/null
done

# The notices of the majors built. Both versions' LICENSE.txt is Godot's MIT license.
rm -rf "$dest/licenses" && mkdir -p "$dest/licenses"
for major in $majors; do
  cp "$work/godot$major/LICENSE.txt" "$dest/licenses/Godot.txt"
  cp "$work/godot$major/COPYRIGHT.txt" "$dest/licenses/Godot $major third-party notices.txt"
done
echo "$stamp" > "$dest/.vndegen-engine"
echo "Godot engines built in $dest in $(($(date +%s) - start)) s"
