#!/usr/bin/env bash
# Builds the VP9 decoder framework, or the standalone converter for a focused check.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
platform=${1:?mac or ios-sim}
if [ "$platform" = bundle ]; then
  product="$root/Packages/VNDegenKit/build/VPX.xcframework"
  stamp=$(cat "$0" Engines/Shared/third_party/libvpx/CHANGELOG | shasum -a 256 | cut -c1-16)
  [ "$(cat "$product/.stamp" 2>/dev/null)" = "$stamp" ] && exit 0
  for slice in mac mac-x86 ios ios-sim sim-x86; do "$0" "$slice" library; done
  mkdir -p build/alpha/{universal-mac,universal-sim}
  lipo -create build/alpha/{mac,mac-x86}/vpx/libvpx.a -output build/alpha/universal-mac/libvpx.a
  lipo -create build/alpha/{ios-sim,sim-x86}/vpx/libvpx.a -output build/alpha/universal-sim/libvpx.a
  headers="$root/build/alpha/headers"
  python3 -c 'import shutil,sys; shutil.rmtree(sys.argv[1], ignore_errors=True)' "$headers"
  mkdir -p "$headers/vpx"
  cp Engines/Shared/third_party/libvpx/vpx/*.h "$headers/vpx/"
  python3 -c 'import shutil,sys; shutil.rmtree(sys.argv[1], ignore_errors=True)' "$product"
  xcodebuild -create-xcframework -output "$product" \
    -library "$root/build/alpha/ios/vpx/libvpx.a" -headers "$headers" \
    -library "$root/build/alpha/universal-sim/libvpx.a" -headers "$headers" \
    -library "$root/build/alpha/universal-mac/libvpx.a" -headers "$headers" >/dev/null
  echo "$stamp" > "$product/.stamp"
  exit 0
fi
arch=arm64
case "$platform" in
  mac) sdk=macosx; target=arm64-apple-macos15.0 ;;
  mac-x86) sdk=macosx; arch=x86_64; target=x86_64-apple-macos15.0 ;;
  ios) sdk=iphoneos; target=arm64-apple-ios18.0 ;;
  ios-sim) sdk=iphonesimulator; target=arm64-apple-ios18.0-simulator ;;
  sim-x86) sdk=iphonesimulator; arch=x86_64; target=x86_64-apple-ios18.0-simulator ;;
  *) exit 2 ;;
esac
out="$root/build/alpha/$platform"
mkdir -p "$out/vpx"
sdkroot=$(xcrun --sdk "$sdk" --show-sdk-path)
cc=$(xcrun --sdk "$sdk" --find clang)
cxx=$(xcrun --sdk "$sdk" --find clang++)
vpx_target=arm64-darwin24-gcc
[ "$arch" != x86_64 ] || vpx_target=generic-gnu
if [ ! -f "$out/vpx/Makefile" ]; then
  (cd "$out/vpx" && CC="$cc" CXX="$cxx" AR="$(xcrun --find ar)" LD="$cc" \
    LDFLAGS="-target $target -isysroot $sdkroot" \
    "$root/Engines/Shared/third_party/libvpx/configure" --target="$vpx_target" \
    --disable-vp8 --disable-vp9-encoder --disable-examples --disable-tools --disable-docs \
    --disable-unit-tests --disable-neon-asm --disable-neon-dotprod --disable-neon-i8mm \
    --disable-sve --disable-sve2 --disable-shared --enable-static \
    --disable-mmx --disable-sse --disable-sse2 --disable-sse3 --disable-ssse3 --disable-sse4_1 \
    --disable-avx --disable-avx2 --disable-avx512 \
    --extra-cflags="-O2 -Wno-overriding-option -target $target -isysroot $sdkroot" \
    --extra-cxxflags="-O2 -Wno-overriding-option -target $target -isysroot $sdkroot")
fi
make -s -C "$out/vpx" -j4 libvpx.a
[ "${2:-}" = library ] && exit 0
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
