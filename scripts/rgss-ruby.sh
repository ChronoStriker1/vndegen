#!/usr/bin/env bash
# Builds the Ruby interpreter VNDegen's RGSS runtime runs games' scripts on: MRI Ruby, static, with no JIT and only the
# zlib and stringio extensions, patched as Engines/RGSSRuntime/patches says.
#   scripts/rgss-ruby.sh build/rgss        Apple: Packages/VNDegenKit/build/RGSSRuby.xcframework, which the CRGSS target
#                                          links, for macOS, iOS and the iOS simulator
#   scripts/rgss-ruby.sh build/rgss linux  Linux test host: build/rgss/linux, which Engines/RGSSRuntime/host links
# Does nothing when the output matches the version, options and patches. Writes Ruby's notice and the runtime's other
# third-party notices to build/rgss/licenses/.
set -euo pipefail

out=${1:?output folder}
platform=${2:-apple}
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)

version=3.4.9
sha256=7bb4d4f5e807cc27251d14d9d6086d182c5b25875191e44ab15b709cd7a7dd9c
url=https://cache.ruby-lang.org/pub/ruby/3.4/ruby-$version.tar.gz
patches=("$root"/Engines/RGSSRuntime/patches/ruby-*.patch)
jobs=${VNDEGEN_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}

# What decides the result: the version, the configure options and the patches.
options_list() { sed -n '/^options=(/,/)$/p' "$0"; }
stamp=$( { echo "$version"; options_list; cat "${patches[@]}"; } | shasum -a 256 | cut -c1-16)-$platform
case "$platform" in
  apple) product=$root/Packages/VNDegenKit/build/RGSSRuby.xcframework ;;
  linux) product=$out/linux ;;
  *) echo "unknown platform $platform" >&2; exit 2 ;;
esac
work=$out/ruby-work
ruby_notice="$work/Ruby (RGSS game scripts).txt"
# The notices are written afresh on every run: Ruby's, kept from its build, and the runtime's own third-party ones.
rm -rf "$out/licenses" && mkdir -p "$out/licenses"
cp "$root"/Engines/RGSSRuntime/licenses/*.txt "$out/licenses/"
[ -f "$ruby_notice" ] && cp "$ruby_notice" "$out/licenses/"
if [ "$(cat "$product/.vndegen-runtime" 2>/dev/null)" = "$stamp" ] && [ -f "$ruby_notice" ]; then
  echo "Ruby is up to date in $product"
  exit 0
fi

mkdir -p "$work"
tarball=$work/ruby-$version.tar.gz
if ! echo "$sha256  $tarball" | shasum -a 256 -c >/dev/null 2>&1; then
  curl -fsSL "$url" -o "$tarball"
  echo "$sha256  $tarball" | shasum -a 256 -c >/dev/null
fi

# A fresh, patched source tree for each slice, since configure writes into the tree.
unpack() {
  rm -rf "$1"
  mkdir -p "$1"
  tar -xzf "$tarball" -C "$1" --strip-components 1
  for patch in "${patches[@]}"; do patch -d "$1" -p1 -s < "$patch"; done
}

# The interpreter the app links: static, its encodings and two extensions built in, no JIT, no gems.
options=(--disable-shared --enable-static --disable-install-doc --disable-install-rdoc --disable-rubygems
         --with-static-linked-ext --without-gmp --disable-yjit --disable-rjit --disable-dtrace --disable-jit-support
         --with-ext=zlib,stringio
         --with-out-ext=openssl,psych,fiddle,readline,dbm,gdbm,win32ole,pty,syslog,nkf,racc,socket,etc,io/console,ripper,objspace,monitor,coverage)

# Builds one slice in $work/<name> and merges what the runtime links into one library, lib/libruby.a, with the
# headers it includes in include/.
slice() {
  local name=$1; shift
  local dir=$work/$name
  # A slice already built with the same stamp and arguments stays.
  local recipe="$stamp $*"
  if [ -f "$dir/out/libruby.a" ] && [ "$(cat "$dir/.vndegen-slice" 2>/dev/null)" = "$recipe" ]; then return; fi
  unpack "$dir"
  (cd "$dir" && ./configure "${options[@]}" "$@" >configure.log 2>&1) || { tail -n 30 "$dir/configure.log"; exit 1; }
  (cd "$dir" && make -j"$jobs" >make.log 2>&1) || { tail -n 40 "$dir/make.log"; exit 1; }
  # libruby-static.a carries stand-ins for the encoding and extension tables, which the real ones replace.
  cp "$(ls "$dir"/libruby*-static.a | head -n 1)" "$dir/libruby-core.a"
  ar d "$dir/libruby-core.a" dmyext.o dmyenc.o 2>/dev/null || true
  mkdir -p "$dir/out"
  local parts=("$dir/libruby-core.a" "$dir/enc/libenc.a" "$dir/enc/libtrans.a" "$dir/ext/zlib/zlib.a"
               "$dir/ext/stringio/stringio.a" "$dir/ext/extinit.o" "$dir/enc/encinit.o")
  if [ "$platform" = apple ]; then
    libtool -static -o "$dir/out/libruby.a" "${parts[@]}" 2>/dev/null
  else
    # GNU ar's script mode merges archives whole, keeping members that share a name, as Ruby's and Prism's
    # encoding.o do.
    rm -f "$dir/out/libruby.a"
    {
      echo "CREATE $dir/out/libruby.a"
      local part
      for part in "${parts[@]}"; do
        case "$part" in
          *.a) echo "ADDLIB $part" ;;
          *) echo "ADDMOD $part" ;;
        esac
      done
      echo "SAVE"
      echo "END"
    } | ar -M
  fi
  rm -rf "$dir/headers" && mkdir -p "$dir/headers"
  cp -R "$dir/include/ruby.h" "$dir/include/ruby" "$dir/headers/"
  cp "$dir"/.ext/include/*/ruby/config.h "$dir/headers/ruby/config.h"
  echo "$recipe" > "$dir/.vndegen-slice"
}

write_licenses() {
  local src=$1
  {
    echo "Ruby $version, the interpreter that runs RPG Maker XP, VX and VX Ace games' scripts."
    echo "Source: $url"
    echo "VNDegen links it under the 2-clause BSD license (BSDL). It changes two files; Engines/RGSSRuntime/patches"
    echo "in VNDegen's repository has the changes. Ruby's LEGAL file lists the parts under other permissive terms."
    echo
    cat "$src/BSDL"
    echo
    echo "---- LEGAL ----"
    cat "$src/LEGAL"
  } >"$ruby_notice"
  cp "$ruby_notice" "$out/licenses/"
}

case "$platform" in
  linux)
    slice linux CFLAGS="-O2 -g -fPIC"
    rm -rf "$product" && mkdir -p "$product"
    mkdir -p "$product/lib"
    cp "$work/linux/out/libruby.a" "$product/lib/"
    cp -R "$work/linux/headers" "$product/"
    write_licenses "$work/linux"
    ;;
  apple)
    # Cross builds need a Ruby of the same version to run their build tools, so one is built for this Mac first.
    host=$work/host
    if [ "$("$host/bin/ruby" -e 'print RUBY_VERSION' 2>/dev/null)" != "$version" ]; then
      unpack "$work/host-src"
      # Only the build tools run on it, so it leaves out extensions that need libraries the Mac may lack.
      (cd "$work/host-src" && ./configure --prefix="$host" --disable-install-doc --disable-yjit \
        --with-out-ext=openssl,psych,fiddle,readline,dbm,gdbm,win32ole >configure.log 2>&1 \
        && make -j"$jobs" >make.log 2>&1 && make install >install.log 2>&1) || { tail -n 40 "$work/host-src/make.log"; exit 1; }
    fi
    base=(--with-baseruby="$host/bin/ruby")
    sdk() { xcrun --sdk "$1" --show-sdk-path; }
    clang=$(xcrun -f clang)
    tools=(AR="$(xcrun -f ar)" RANLIB="$(xcrun -f ranlib)" NM="$(xcrun -f nm)" STRIP="$(xcrun -f strip)")
    # iOS declares fork and exec, which its sandbox refuses at run time; Ruby's process functions then raise, and RGSS
    # games don't call them. system() and daemon() aren't available to iOS apps, and getentropy has no header there.
    ios_cache=(ac_cv_func_system=no ac_cv_func_daemon=no ac_cv_func_getentropy=no)
    slice mac-arm64 "${base[@]}" --host=aarch64-apple-darwin "${tools[@]}" \
      CC="$clang -target arm64-apple-macos15.0 -isysroot $(sdk macosx)" CFLAGS="-O2"
    slice mac-x86_64 "${base[@]}" --host=x86_64-apple-darwin "${tools[@]}" \
      CC="$clang -target x86_64-apple-macos15.0 -isysroot $(sdk macosx)" CFLAGS="-O2"
    slice ios-arm64 "${base[@]}" --host=aarch64-apple-darwin "${tools[@]}" \
      CC="$clang -target arm64-apple-ios18.0 -isysroot $(sdk iphoneos)" CFLAGS="-O2" CPPFLAGS="-I$work/ios-shim" \
      "${ios_cache[@]}"
    slice sim-arm64 "${base[@]}" --host=aarch64-apple-darwin "${tools[@]}" \
      CC="$clang -target arm64-apple-ios18.0-simulator -isysroot $(sdk iphonesimulator)" CFLAGS="-O2" \
      CPPFLAGS="-I$work/ios-shim" "${ios_cache[@]}"
    mkdir -p "$work/mac/out"
    lipo -create "$work/mac-arm64/out/libruby.a" "$work/mac-x86_64/out/libruby.a" -output "$work/mac/out/libruby.a"
    rm -rf "$product"
    xcodebuild -create-xcframework -output "$product" \
      -library "$work/ios-arm64/out/libruby.a" -headers "$work/ios-arm64/headers" \
      -library "$work/sim-arm64/out/libruby.a" -headers "$work/sim-arm64/headers" \
      -library "$work/mac/out/libruby.a" -headers "$work/mac-arm64/headers" >/dev/null
    write_licenses "$work/mac-arm64"
    ;;
esac
echo "$stamp" > "$product/.vndegen-runtime"
echo "Ruby $version -> $product"
