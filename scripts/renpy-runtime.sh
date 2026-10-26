#!/usr/bin/env bash
# Runs on a Mac. Installs the Ren'Py runtimes VNDegen bundles into <dest>/8 and <dest>/7. Each is an official Ren'Py
# SDK trimmed to what runs games on macOS: renpy.py, renpy/, the standard library, and the macOS python and
# librenpython.dylib.
#   scripts/renpy-runtime.sh build/renpy
#   scripts/renpy-runtime.sh build/renpy ios
# The ios argument installs <dest>/8 and, in <dest>/ios, the native side of Ren'Py 8 for iPhone and iPad from Ren'Py's
# renios package: RenPyRuntime.xcframework, one static library per platform made of renios's prebuilt libraries, and
# MetalANGLE.xcframework. iOS runs Ren'Py 7 games on Ren'Py 8, so it needs no Ren'Py 7 runtime.
# The build phase in project.yml runs this, then copies the result into the app. Downloads are checked against the
# SHA-256 sums below and cached in ~/Library/Caches/VNDegen/renpy-sdk. A runtime already installed by this version of
# the script is left alone.
# It then writes <dest>/licenses/, the notices of the third-party code in the runtimes it installed for the platform,
# one text file each. See write_licenses.
# Ren'Py 7.8.7 gets one patch. Its loader compiles a game's python-packages modules from text, which Python 2 rejects
# when the module declares an encoding. 7.5.1 compiled bytes, and the patch restores that.
set -euo pipefail
dest=${1:?destination folder, such as build/renpy}
platform=${2:-mac}
cache=${VNDEGEN_RENPY_CACHE:-$HOME/Library/Caches/VNDegen/renpy-sdk}
stamp=$(shasum -a 256 "$0" | cut -c1-16)
notices=$(cd "$(dirname "$0")" && pwd)/licenses/renpy

fetch() { # <file name> <sha256> <url>
  local file="$cache/$1"
  echo "$2  $file" | shasum -a 256 -c --status 2>/dev/null && return
  mkdir -p "$cache"
  curl -fsSL "$3" -o "$file.part"
  echo "$2  $file.part" | shasum -a 256 -c --status || { echo "$1 checksum mismatch" >&2; exit 1; }
  mv "$file.part" "$file"
}

install() { # <version> <sha256> <folder> <python line>
  local version=$1 sum=$2 folder=$3 py=$4
  local out="$dest/$folder" tarball="$cache/renpy-$version-sdk.tar.bz2"
  [ "$(cat "$out/.vndegen-runtime" 2>/dev/null)" = "$version $stamp" ] && return
  echo "Ren'Py $version -> $out"
  fetch "renpy-$version-sdk.tar.bz2" "$sum" "https://www.renpy.org/dl/$version/renpy-$version-sdk.tar.bz2"
  local work sdk
  work=$(mktemp -d)
  sdk="$work/renpy-$version-sdk"
  tar -xjf "$tarball" -C "$work"
  # Copies keep their dates, so the SDK's compiled Python stays current and Python never recompiles inside the app.
  rm -rf "$out.part" && mkdir -p "$out.part/lib/$py-mac-universal"
  # LICENSE.txt is Ren'Py's own license and those of the libraries in its binaries. It ships with the runtime.
  cp -Rp "$sdk/renpy.py" "$sdk/renpy" "$sdk/LICENSE.txt" "$out.part/"
  cp -Rp "$sdk/lib/python"* "$out.part/lib/"
  cp -p "$sdk/lib/$py-mac-universal/python" "$sdk/lib/$py-mac-universal/librenpython.dylib" "$out.part/lib/$py-mac-universal/"
  rm -rf "$work"
  [ "$folder" = 7 ] && patch_loader "$out.part/renpy/loader.py"
  echo "$version $stamp" > "$out.part/.vndegen-runtime"
  rm -rf "$out" && mv "$out.part" "$out"
}

patch_loader() {
  python3 -I - "$1" <<'EOF'
import sys
path = sys.argv[1]
source = open(path).read()
anchor = "                code = compile(source, filename, 'exec', renpy.python.old_compile_flags, 1)"
assert source.count(anchor) == 1
fix = '                source = source.encode("raw_unicode_escape").replace(b"\\r", b"")\n'
open(path, "w").write(source.replace(anchor, fix + anchor))
EOF
  rm -f "${1%.py}.pyo" "${1%.py}.pyc"
}

# The libraries renios's own Xcode project links, minus librenpython, whose launcher VNDegen replaces.
ios_libraries=(renpy python3.12 assimp avformat avcodec swscale swresample avutil SDL2_image SDL2 avif aom yuv turbojpeg
  png16 webp harfbuzz brotlidec brotlicommon fribidi freetype ffi ssl crypto lzma bz2 z)

install_ios() { # <version> <sha256>
  local version=$1 sum=$2 out="$dest/ios"
  [ "$(cat "$out/.vndegen-runtime" 2>/dev/null)" = "$version $stamp" ] && return
  echo "Ren'Py $version renios -> $out"
  fetch "renpy-$version-renios.zip" "$sum" "https://www.renpy.org/dl/$version/renpy-$version-renios.zip"
  local work prebuilt config library libraries
  work=$(mktemp -d)
  unzip -q "$cache/renpy-$version-renios.zip" 'renios/prototype/prebuilt/*' 'renios/prototype/Frameworks/*' -d "$work"
  prebuilt="$work/renios/prototype/prebuilt"
  # renios builds simulator libraries as debug and device libraries as release.
  for config in debug release; do
    libraries=()
    for library in "${ios_libraries[@]}"; do
      [ -f "$prebuilt/$config/lib$library.a" ] || { echo "renios has no lib$library.a" >&2; exit 1; }
      libraries+=("$prebuilt/$config/lib$library.a")
    done
    mkdir -p "$work/$config"
    libtool -static -no_warning_for_no_symbols -o "$work/$config/libRenPyRuntime.a" "${libraries[@]}"
  done
  rm -rf "$out.part" && mkdir -p "$out.part"
  xcodebuild -create-xcframework -library "$work/release/libRenPyRuntime.a" -library "$work/debug/libRenPyRuntime.a" \
    -output "$out.part/RenPyRuntime.xcframework" >/dev/null
  cp -Rp "$work/renios/prototype/Frameworks/MetalANGLE.xcframework" "$out.part/"
  rm -rf "$work"
  echo "$version $stamp" > "$out.part/.vndegen-runtime"
  rm -rf "$out" && mv "$out.part" "$out"
}

# Each line names a notice, the runtime it applies to (8, 7, ios, or * for 8 and 7), and the evidence that the runtime
# holds its code: a symbol the runtime's native library defines, a file in the runtime, or a file in the runtime to copy
# as the notice. Other notices are scripts/licenses/renpy/<notice>.txt, where the README says where each came from. The
# last field lists the renios libraries a notice covers. Every library in ios_libraries needs one, and every notice
# that covers one must find its evidence in Ren'Py 8 on both platforms, so a renamed symbol fails instead of dropping a
# notice.
notice_table() {
  cat <<'EOF'
Ren'Py|8|copy LICENSE.txt|renpy
Ren'Py 7|7|copy LICENSE.txt
How VNDegen meets the LGPL|*|sym ^_avcodec_open2$
Python 3.12|8|sym ^_Py_Initialize$|python3.12
Python 2.7|7|sym ^_Py_Initialize$
SDL2|*|sym ^_SDL_Init$|SDL2
SDL2_image|*|sym ^_IMG_Load$|SDL2_image
FFmpeg|*|sym ^_avcodec_open2$|avformat avcodec swscale swresample avutil
FriBidi|*|sym ^_fribidi_get_bidi_types$|fribidi
FreeType|*|sym ^_FT_Init_FreeType$|freetype
HarfBuzz|*|sym ^_hb_buffer_create$|harfbuzz
libpng|*|sym ^_png_create_read_struct$|png16
libjpeg-turbo|*|sym ^_jpeg_CreateDecompress$|turbojpeg
libwebp|*|sym ^_WebPDecode$|webp
libavif|*|sym ^_avifDecoderCreate$|avif
aom|*|sym ^_aom_codec_decode$|aom
libyuv|*|sym ^_I420ToARGB$|yuv
Brotli|*|sym ^_BrotliDecoderCreateInstance$|brotlidec brotlicommon
XZ Utils (liblzma)|*|sym ^_lzma_code$|lzma
bzip2|*|sym ^_BZ2_bzDecompress$|bz2
zlib|*|sym ^_inflate$|z
libffi|*|sym ^_ffi_call$|ffi
OpenSSL 3|8|sym ^_OSSL_PROVIDER_load$|ssl crypto
OpenSSL 1.1.1|7|sym ^_SSL_new$
libhydrogen|*|sym ^_hydro_init$
tinyfiledialogs|*|sym ^_tinyfd_messageBox$
Open Asset Import Library (assimp)|*|sym Assimp8Importer|assimp
pygame_sdl2|8|sym ^_PyInit_renpy_pygame_surface$
pygame_sdl2|7|file lib/python2.7/pygame_sdl2
MetalANGLE|ios|file MetalANGLE.xcframework
SDL_GameControllerDB|*|file renpy/common/gamecontrollerdb.txt
DejaVu Sans font|*|copy renpy/common/DejaVuSans.txt
Twemoji font|*|copy renpy/common/TwemojiCOLRv0.txt
OpenDyslexic font|*|copy renpy/common/_OpenDyslexic3-Regular.txt
Quicksand font|*|copy renpy/common/_theme_awt/OFL.txt
future|*|file lib/python*/future
six|*|file lib/python*/six.py*
rsa|*|file lib/python*/rsa
pyasn1|*|file lib/python*/pyasn1
ecdsa|*|file lib/python*/ecdsa
urllib3|*|file lib/python*/urllib3
chardet|*|file lib/python*/chardet
certifi|*|file lib/python*/certifi
idna|*|file lib/python*/idna
requests|*|file lib/python*/requests
pefile|*|file lib/python*/pefile.py*
websockets|*|file lib/python*/websockets
PySocks|*|file lib/python*/socks.py*
typing|7|file lib/python2.7/typing.pyo
win_inet_pton|*|file lib/python*/win_inet_pton.py*
pyobjus|*|file lib/python*/pyobjus
pyjnius|*|file lib/python*/jnius
EOF
}

write_licenses() {
  local out="$dest/licenses" lstamp runtimes work r name scope evidence libraries kind arg target library
  # The stamp's name keeps it out of the app, since the build phase's copy skips files named .vndegen-runtime.
  lstamp="$platform $(cat "$0" "$notices"/* | shasum -a 256 | cut -c1-16)"
  [ "$(cat "$dest/.vndegen-runtime" 2>/dev/null)" = "$lstamp" ] && [ -d "$out" ] && return
  echo "Ren'Py notices -> $out"
  work=$(mktemp -d)
  # The native library of each runtime, which links Ren'Py's libraries statically. On iOS that is Ren'Py 8's.
  case "$platform" in
    mac)
      runtimes="8 7"
      nm -arch arm64 -U -j "$dest/8/lib/py3-mac-universal/librenpython.dylib" > "$work/8.symbols"
      nm -arch arm64 -U -j "$dest/7/lib/py2-mac-universal/librenpython.dylib" > "$work/7.symbols" ;;
    ios)
      runtimes="8 ios"
      nm -arch arm64 -U -j "$dest/ios/RenPyRuntime.xcframework/ios-arm64/libRenPyRuntime.a" > "$work/8.symbols" ;;
  esac
  rm -rf "$out.part" && mkdir -p "$out.part"
  while IFS='|' read -r name scope evidence libraries; do
    kind=${evidence%% *} arg=${evidence#* }
    case "$name" in "Ren'Py"*) target="$out.part/$name.txt" ;; *) target="$out.part/Ren'Py - $name.txt" ;; esac
    [ "$scope" = '*' ] && scope="8 7"
    for r in $scope; do
      case " $runtimes " in *" $r "*) ;; *) continue ;; esac
      case "$kind" in
        sym) grep -q -E "$arg" "$work/$r.symbols" ;;
        file) ls -d "$dest/$r/"$arg >/dev/null 2>&1 ;;
        copy) [ -f "$dest/$r/$arg" ] ;;
      esac || {
        if [ -n "$libraries" ] && [ "$r" = 8 ]; then
          echo "Ren'Py 8 links $name, but its evidence ($evidence) matched nothing. Update the notice table." >&2
          exit 1
        fi
        continue
      }
      [ -f "$target" ] && continue
      if [ "$kind" = copy ]; then cp "$dest/$r/$arg" "$target"; else cp "$notices/$name.txt" "$target"; fi
      touch "$work/written $name"
    done
  done < <(notice_table)
  if [ "$platform" = ios ]; then
    for library in "${ios_libraries[@]}"; do
      name=$(notice_table | awk -F'|' -v l="$library" '{ n = split($4, a, " "); for (i = 1; i <= n; i++) if (a[i] == l && found == "") found = $1 } END { print found }')
      [ -n "$name" ] && [ -f "$work/written $name" ] || { echo "no notice covers renios's lib$library.a" >&2; exit 1; }
    done
  fi
  rm -rf "$work" "$out"
  mv "$out.part" "$out"
  echo "$lstamp" > "$dest/.vndegen-runtime"
}

install 8.5.3 eb0a9be7f0fb13632fe25ceade9a8bed5a1b4d6b6e83bd19eeeb29e1a1bb4a45 8 py3
case "$platform" in
  mac) install 7.8.7 65466068af7c181a143f13d84b0ddf7a6aa58dd8d26e7ccd97cdcadecabcae5a 7 py2 ;;
  ios) install_ios 8.5.3 c4fae153e8276ed0faed5e84ea3e0b7c4bf337f0e3208e9130c6a41748a83b2b ;;
  *) echo "unknown platform $platform" >&2; exit 2 ;;
esac
write_licenses
