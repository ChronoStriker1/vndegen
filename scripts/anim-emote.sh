#!/usr/bin/env bash
# Runs on a Mac. Builds the ANIM engine's E-mote library, eluna behind VNDegen's C interface in Engines/ANIM/vnemote,
# as a dynamic framework:
#   <dest>/VNEmote.xcframework   iOS device and simulator (arm64) and macOS (arm64 and x86_64)
#   <dest>/licenses/             eluna's license and those of the crates it builds in, one text file each
#   scripts/anim-emote.sh build/anim
# A framework rather than a static library, as scripts/siglus-engine.sh explains: a second Rust static library beside
# the Artemis engine's would define the same symbols twice. The crates come from Engines/ANIM/vendor, so the build
# never fetches. Rust is pinned to the toolchain below, installed through rustup when missing. A build whose inputs
# have not changed since the last one is skipped.
set -euo pipefail
toolchain=1.99.1
targets=(aarch64-apple-ios aarch64-apple-ios-sim aarch64-apple-darwin x86_64-apple-darwin)
dest=${1:?destination folder, such as build/anim}
root=$(cd "$(dirname "$0")/.." && pwd)
engine="$root/Engines/ANIM"
src="$engine/vnemote"
header="$root/Packages/VNDegenKit/Sources/CANIM/include/vnemote.h"
mkdir -p "$dest"
dest=$(cd "$dest" && pwd)
ios_target=$(sed -n 's/^ *iOS: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)
mac_target=$(sed -n 's/^ *macOS: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)
version=$(sed -n 's/^ *MARKETING_VERSION: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)

stamp=$( (cat "$0"; echo "$ios_target $mac_target $version"; cd "$engine" &&
  find vnemote third_party -name target -prune -o -type f -print0 |
  LC_ALL=C sort -z | xargs -0 shasum -a 256; shasum -a 256 "$header") | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "E-mote library is up to date in $dest"
  exit 0
fi

PATH="/opt/homebrew/opt/rustup/bin:$HOME/.cargo/bin:$PATH"
command -v rustup >/dev/null || { echo "error: install rustup (brew install rustup)" >&2; exit 1; }
rustup toolchain list | grep -q "^$toolchain" || rustup toolchain install "$toolchain" --profile minimal
installed=$(rustup +"$toolchain" target list --installed)
for target in "${targets[@]}"; do
  grep -qx "$target" <<<"$installed" || rustup +"$toolchain" target add "$target"
done

start=$(date +%s)
work="$dest/.build"
mkdir -p "$work"
for target in "${targets[@]}"; do
  case $target in
    *-ios | *-ios-sim)
      env=(IPHONEOS_DEPLOYMENT_TARGET="$ios_target")
      install_name=@rpath/VNEmote.framework/VNEmote ;;
    *)
      env=(MACOSX_DEPLOYMENT_TARGET="$mac_target")
      install_name=@rpath/VNEmote.framework/Versions/A/VNEmote ;;
  esac
  if [ "$(cat "$work/$target.deployment" 2>/dev/null)" != "${env[0]}" ]; then
    rm -rf "$work/target/$target"
    echo "${env[0]}" > "$work/$target.deployment"
  fi
  (cd "$src" && env "${env[@]}" CARGO_TARGET_DIR="$work/target" rustup run "$toolchain" cargo rustc --release --offline \
    --locked --lib --crate-type cdylib --target "$target" -- -C link-arg=-Wl,-install_name,"$install_name")
done

plist() {
  local platform=$1 minimum=""
  [ "$platform" = MacOSX ] || minimum="<key>MinimumOSVersion</key><string>$ios_target</string>"
  cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleDevelopmentRegion</key><string>en</string>
<key>CFBundleExecutable</key><string>VNEmote</string>
<key>CFBundleIdentifier</key><string>com.chronostriker1.vndegen.VNEmote</string>
<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
<key>CFBundleName</key><string>VNEmote</string>
<key>CFBundlePackageType</key><string>FMWK</string>
<key>CFBundleShortVersionString</key><string>$version</string>
<key>CFBundleVersion</key><string>1</string>
<key>CFBundleSupportedPlatforms</key><array><string>$platform</string></array>
$minimum
</dict></plist>
EOF
}

lib=libvnemote.dylib
frameworks="$work/frameworks"
rm -rf "$frameworks"
for slice in aarch64-apple-ios:iPhoneOS aarch64-apple-ios-sim:iPhoneSimulator; do
  target=${slice%%:*} platform=${slice#*:}
  framework="$frameworks/$target/VNEmote.framework"
  mkdir -p "$framework/Headers"
  cp -p "$work/target/$target/release/$lib" "$framework/VNEmote"
  cp -p "$header" "$framework/Headers/"
  plist "$platform" > "$framework/Info.plist"
done
framework="$frameworks/macos/VNEmote.framework"
mkdir -p "$framework/Versions/A/Headers" "$framework/Versions/A/Resources"
lipo -create "$work/target/aarch64-apple-darwin/release/$lib" "$work/target/x86_64-apple-darwin/release/$lib" \
  -output "$framework/Versions/A/VNEmote"
cp -p "$header" "$framework/Versions/A/Headers/"
plist MacOSX > "$framework/Versions/A/Resources/Info.plist"
ln -s A "$framework/Versions/Current"
for link in VNEmote Headers Resources; do ln -s "Versions/Current/$link" "$framework/$link"; done

rm -rf "$dest/VNEmote.xcframework" "$dest/licenses"
mkdir -p "$dest/licenses"
xcodebuild -create-xcframework -framework "$frameworks/aarch64-apple-ios/VNEmote.framework" \
  -framework "$frameworks/aarch64-apple-ios-sim/VNEmote.framework" -framework "$framework" \
  -output "$dest/VNEmote.xcframework" >/dev/null

# Notices: eluna's licenses, then each crate in the iOS build with its own license files.
(cd "$src" && rustup run "$toolchain" cargo metadata --offline --locked --format-version 1 >"$work/metadata.json")
(cd "$src" && rustup run "$toolchain" cargo tree --offline --locked --target aarch64-apple-ios -e normal,build \
  --prefix none --format '{p}' >"$work/crates.txt")
python3 - "$engine" "$work" "$dest/licenses" <<'PY'
import json, os, re, sys
engine, work, out = sys.argv[1:]
eluna = os.path.join(engine, "third_party", "eluna_rs")
meta = json.load(open(os.path.join(work, "metadata.json")))
packages = {(p["name"], p["version"]): p for p in meta["packages"]}
license_file = re.compile(r"(?i)^(licen[cs]e|copying|notice|unlicense|copyright)")
def write(name, text):
    with open(os.path.join(out, name + ".txt"), "w") as f:
        f.write(text)
write("ANIM E-mote - eluna", "eluna, https://github.com/xmoezzz/eluna. Its library's source is "
      "Engines/ANIM/third_party/eluna_rs in VNDegen's repository.\n\n" + open(os.path.join(eluna, "LICENSE-MPL-2.0")).read())
write("ANIM E-mote - eluna player", "The ANIM runtime's E-mote drawing follows eluna's player, "
      "https://github.com/xmoezzz/eluna, under the MIT license.\n\n" + open(os.path.join(eluna, "LICENSE-MIT.txt")).read())
missing = []
seen = set()
for line in open(os.path.join(work, "crates.txt")):
    parts = line.split()
    if len(parts) < 2 or (parts[0], parts[1]) in seen:
        continue
    seen.add((parts[0], parts[1]))
    package = packages[(parts[0], parts[1].lstrip("v"))]
    folder = os.path.dirname(os.path.realpath(package["manifest_path"]))
    if not folder.startswith(os.path.join(os.path.realpath(engine), "vendor") + os.sep):
        continue
    files = sorted(f for f in os.listdir(folder) if license_file.match(f) and os.path.isfile(os.path.join(folder, f)))
    title = f"{package['name']} {package['version']}"
    if not files:
        missing.append(title)
        continue
    header = f"{title}, {package.get('repository') or package.get('homepage') or 'crates.io'}\n"
    body = "\n\n".join(open(os.path.join(folder, f), errors="replace").read() for f in files)
    write(f"ANIM E-mote - {title}", header + "\n" + body)
if missing:
    sys.exit("error: no license text for " + ", ".join(missing))
print(f"{len(os.listdir(out))} notices")
PY
echo "$stamp" > "$dest/.vndegen-engine"
echo "E-mote library built in $dest in $(($(date +%s) - start)) s"
