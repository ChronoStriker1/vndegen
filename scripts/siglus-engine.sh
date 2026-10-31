#!/usr/bin/env bash
# Runs on a Mac. Builds the SiglusEngine and RealLive engine, siglus_rs in Engines/Siglus/siglus_rs behind VNDegen's
# C interface in Engines/Siglus/vnsiglus, as a dynamic framework:
#   <dest>/VNSiglus.xcframework   iOS device and simulator (arm64) and macOS (arm64 and x86_64)
#   <dest>/licenses/              siglus_rs's license and those of the crates it builds in, one text file each
#   scripts/siglus-engine.sh build/siglus
# A framework rather than a static library keeps its copy of Rust's standard library to itself: a second Rust static
# library beside the Artemis engine's would define the same symbols twice. The settings are the ones
# Engines/Siglus/UPSTREAM.md explains. Rust is pinned to the toolchain below, installed through rustup when missing. A
# build whose inputs have not changed since the last one is skipped.
set -euo pipefail
toolchain=1.99.1
targets=(aarch64-apple-ios aarch64-apple-ios-sim aarch64-apple-darwin x86_64-apple-darwin)
dest=${1:?destination folder, such as build/siglus}
root=$(cd "$(dirname "$0")/.." && pwd)
engines="$root/Engines/Siglus"
src="$engines/siglus_rs"
header="$root/Packages/VNDegenKit/Sources/CSiglus/include/vnsiglus.h"
mkdir -p "$dest"
dest=$(cd "$dest" && pwd)
ios_target=$(sed -n 's/^ *iOS: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)
mac_target=$(sed -n 's/^ *macOS: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)
version=$(sed -n 's/^ *MARKETING_VERSION: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)

stamp=$( (cat "$0"; echo "$ios_target $mac_target $version"; cd "$engines" &&
  find siglus_rs vnsiglus wmv_stub licenses -name target -prune -o -type f -print0 |
  LC_ALL=C sort -z | xargs -0 shasum -a 256; shasum -a 256 "$header") | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "SiglusEngine and RealLive engine is up to date in $dest"
  exit 0
fi

# rustup comes from Homebrew on these Macs; its cargo, not Homebrew's own, honours the pinned toolchain.
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
      install_name=@rpath/VNSiglus.framework/VNSiglus ;;
    *)
      env=(MACOSX_DEPLOYMENT_TARGET="$mac_target")
      install_name=@rpath/VNSiglus.framework/Versions/A/VNSiglus ;;
  esac
  # A changed deployment target needs a clean build of that target, since build scripts don't notice it.
  if [ "$(cat "$work/$target.deployment" 2>/dev/null)" != "${env[0]}" ]; then
    rm -rf "$work/target/$target"
    echo "${env[0]}" > "$work/$target.deployment"
  fi
  (cd "$src" && env "${env[@]}" CARGO_TARGET_DIR="$work/target" rustup run "$toolchain" cargo rustc --release --locked \
    -p vnsiglus --lib --crate-type cdylib --target "$target" -- -C link-arg=-Wl,-install_name,"$install_name")
done

# Info.plist for a framework on `platform` (iPhoneOS, iPhoneSimulator, or MacOSX).
plist() {
  local platform=$1 minimum=""
  [ "$platform" = MacOSX ] || minimum="<key>MinimumOSVersion</key><string>$ios_target</string>"
  cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleDevelopmentRegion</key><string>en</string>
<key>CFBundleExecutable</key><string>VNSiglus</string>
<key>CFBundleIdentifier</key><string>com.chronostriker1.vndegen.VNSiglus</string>
<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
<key>CFBundleName</key><string>VNSiglus</string>
<key>CFBundlePackageType</key><string>FMWK</string>
<key>CFBundleShortVersionString</key><string>$version</string>
<key>CFBundleVersion</key><string>1</string>
<key>CFBundleSupportedPlatforms</key><array><string>$platform</string></array>
$minimum
</dict></plist>
EOF
}

lib=libvnsiglus.dylib
frameworks="$work/frameworks"
rm -rf "$frameworks"
# iOS frameworks are flat.
for slice in aarch64-apple-ios:iPhoneOS aarch64-apple-ios-sim:iPhoneSimulator; do
  target=${slice%%:*} platform=${slice#*:}
  framework="$frameworks/$target/VNSiglus.framework"
  mkdir -p "$framework/Headers"
  cp -p "$work/target/$target/release/$lib" "$framework/VNSiglus"
  cp -p "$header" "$framework/Headers/"
  plist "$platform" > "$framework/Info.plist"
done
# A macOS framework keeps its files in Versions/A, with links to them at the top.
framework="$frameworks/macos/VNSiglus.framework"
mkdir -p "$framework/Versions/A/Headers" "$framework/Versions/A/Resources"
lipo -create "$work/target/aarch64-apple-darwin/release/$lib" "$work/target/x86_64-apple-darwin/release/$lib" \
  -output "$framework/Versions/A/VNSiglus"
cp -p "$header" "$framework/Versions/A/Headers/"
plist MacOSX > "$framework/Versions/A/Resources/Info.plist"
ln -s A "$framework/Versions/Current"
for link in VNSiglus Headers Resources; do ln -s "Versions/Current/$link" "$framework/$link"; done

rm -rf "$dest/VNSiglus.xcframework" "$dest/licenses"
mkdir -p "$dest/licenses"
xcodebuild -create-xcframework -framework "$frameworks/aarch64-apple-ios/VNSiglus.framework" \
  -framework "$frameworks/aarch64-apple-ios-sim/VNSiglus.framework" -framework "$framework" \
  -output "$dest/VNSiglus.xcframework" >/dev/null

# Notices: siglus_rs's license, then each crate in the iOS build with its own license files, or, when a crate ships
# none, its license's text from Engines/Siglus/licenses under a header naming it. siglus_rs's own crates are under its
# license, and theora-rs, a rewrite of libtheora, carries libtheora's too.
(cd "$src" && rustup run "$toolchain" cargo metadata --locked --format-version 1 >"$work/metadata.json")
(cd "$src" && rustup run "$toolchain" cargo tree --locked -p vnsiglus --target aarch64-apple-ios -e normal,build \
  --prefix none --format '{p}' >"$work/crates.txt")
python3 - "$engines" "$work" "$dest/licenses" <<'PY'
import json, os, re, sys
engines, work, out = sys.argv[1:]
src = os.path.join(engines, "siglus_rs")
texts = os.path.join(engines, "licenses")
meta = json.load(open(os.path.join(work, "metadata.json")))
packages = {(p["name"], p["version"]): p for p in meta["packages"]}
license_file = re.compile(r"(?i)^(licen[cs]e|copying|notice|unlicense|copyright)")
def write(name, text):
    with open(os.path.join(out, name + ".txt"), "w") as f:
        f.write(text)
write("SiglusEngine and RealLive - siglus_rs", "siglus_rs, https://github.com/xmoezzz/siglus_rs. Its source, with "
      "VNDegen's changes, is Engines/Siglus/siglus_rs in VNDegen's repository.\n\n"
      + open(os.path.join(src, "LICENSE-MPL-2.0")).read())
write("SiglusEngine and RealLive - libtheora", "theora-rs in siglus_rs is a rewrite of libtheora 1.2.0, "
      "https://gitlab.xiph.org/xiph/theora.\n\n" + open(os.path.join(texts, "libtheora.txt")).read())
missing = []
seen = set()
for line in open(os.path.join(work, "crates.txt")):
    parts = line.split()
    if len(parts) < 2 or (parts[0], parts[1]) in seen:
        continue
    seen.add((parts[0], parts[1]))
    package = packages[(parts[0], parts[1].lstrip("v"))]
    folder = os.path.dirname(package["manifest_path"])
    if folder.startswith(engines + os.sep):
        continue  # siglus_rs's crates, under its license above, and VNDegen's own, under VNDegen's
    files = sorted(f for f in os.listdir(folder) if license_file.match(f) and os.path.isfile(os.path.join(folder, f)))
    title = f"{package['name']} {package['version']}"
    header = f"{title}, {package.get('repository') or package.get('homepage') or 'crates.io'}\n"
    if files:
        body = "\n\n".join(open(os.path.join(folder, f), errors="replace").read() for f in files)
    else:
        expression = package.get("license") or ""
        chosen = next((spdx for spdx in ("MIT", "Zlib", "Apache-2.0", "MPL-2.0", "CC0-1.0") if re.search(rf"\b{re.escape(spdx)}\b", expression)), None)
        if not chosen:
            missing.append(f"{title} ({expression or 'no license'})")
            continue
        authors = ", ".join(re.sub(r"\s*<[^>]*>", "", a) for a in package.get("authors") or []) or f"the {package['name']} authors"
        body = f"License: {expression}, used under {chosen}.\n\n"
        if chosen in ("MIT", "Zlib"):
            body += f"Copyright (c) {authors}\n\n"
        body += open(os.path.join(texts, chosen + ".txt")).read()
    write(f"SiglusEngine and RealLive - {title}", header + "\n" + body)
if missing:
    sys.exit("error: no license text for " + ", ".join(missing))
print(f"{len(os.listdir(out))} notices")
PY
echo "$stamp" > "$dest/.vndegen-engine"
echo "SiglusEngine and RealLive engine built in $dest in $(($(date +%s) - start)) s"
