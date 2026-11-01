#!/usr/bin/env bash
# Builds the standalone alpha converter for its feasibility check. No app build is needed.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
platform=${1:?mac or ios-sim}
case "$platform" in
  mac) sdk=macosx; target=arm64-apple-macos15.0 ;;
  ios-sim) sdk=iphonesimulator; target=arm64-apple-ios18.0-simulator ;;
  *) exit 2 ;;
esac
out="$root/build/alpha/$platform"
mkdir -p "$out/vpx"
sdkroot=$(xcrun --sdk "$sdk" --show-sdk-path)
cc=$(xcrun --sdk "$sdk" --find clang)
cxx=$(xcrun --sdk "$sdk" --find clang++)
if [ ! -f "$out/vpx/Makefile" ]; then
  (cd "$out/vpx" && CC="$cc" CXX="$cxx" AR="$(xcrun --find ar)" LD="$cc" \
    LDFLAGS="-target $target -isysroot $sdkroot" \
    "$root/Engines/Shared/third_party/libvpx/configure" --target=arm64-darwin24-gcc \
    --disable-vp8 --disable-vp9-encoder --disable-examples --disable-tools --disable-docs \
    --disable-unit-tests --disable-neon-asm --disable-neon-dotprod --disable-neon-i8mm \
    --disable-sve --disable-sve2 --disable-shared --enable-static \
    --extra-cflags="-O2 -Wno-overriding-option -target $target -isysroot $sdkroot" \
    --extra-cxxflags="-O2 -Wno-overriding-option -target $target -isysroot $sdkroot")
fi
make -s -C "$out/vpx" -j4 libvpx.a
lib="$root/Engines/Shared/third_party/libwebm"
opus="$root/Engines/Shared/third_party/opus"
mkdir -p "$out/opus"
if [ ! -f "$out/libopus.a" ]; then
  while IFS= read -r file; do
    "$cc" -O2 -w -DHAVE_CONFIG_H -target "$target" -isysroot "$sdkroot" \
      -I"$opus" -I"$opus/include" -I"$opus/celt" -I"$opus/silk" \
      -c "$file" -o "$out/opus/$(basename "$file" .c).o"
  done < <(find "$opus/celt" "$opus/silk" "$opus/src" -name '*.c')
  xcrun ar rcs "$out/libopus.a" "$out/opus/"*.o
fi
for name in mkvparser mkvreader; do
  "$cxx" -O2 -std=c++17 -target "$target" -isysroot "$sdkroot" -I"$lib" \
    -c "$lib/mkvparser/$name.cc" -o "$out/$name.o"
done
"$cxx" -O2 -std=c++17 -fobjc-arc -DVNALPHA_TOOL -target "$target" -isysroot "$sdkroot" \
  -I"$root/Engines/Shared/third_party/libvpx" -I"$out/vpx" -I"$lib" \
  "$root/Engines/Shared/vnalpha.mm" "$out/mkvparser.o" "$out/mkvreader.o" "$out/vpx/libvpx.a" "$out/libopus.a" \
  -framework Foundation -framework AVFoundation -framework Accelerate -framework VideoToolbox \
  -framework CoreVideo -framework CoreMedia -Wl,-dead_strip -o "$out/vnalpha"
codesign --force --sign - "$out/vnalpha"
size "$out/vnalpha" "$out/vpx/libvpx.a" | tail -1
