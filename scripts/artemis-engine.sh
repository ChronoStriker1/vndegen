#!/usr/bin/env bash
# Runs on a Mac. Builds the Artemis engine, art3m1s-core in Engines/Artemis/art3m1s-core, as a static library:
#   <dest>/ios/Art3m1s.xcframework   iOS device and simulator (arm64) and macOS (arm64 and x86_64), with the C header
#   <dest>/licenses/                 art3m1s-core's license and those of the crates and C libraries it builds in, one
#                                    text file each
#   scripts/artemis-engine.sh build/artemis
# The settings here are the ones Engines/Artemis/UPSTREAM.md explains: the Metal renderer without the default GL one,
# the app's deployment targets, and a clean build of a target whose deployment target changed, since lua-src's build
# script doesn't notice. Rust is pinned to the toolchain below, installed through rustup when missing. A build whose
# inputs have not changed since the last one is skipped.
set -euo pipefail
toolchain=1.99.1
targets=(aarch64-apple-ios aarch64-apple-ios-sim aarch64-apple-darwin x86_64-apple-darwin)
dest=${1:?destination folder, such as build/artemis}
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/Engines/Artemis/art3m1s-core"
mkdir -p "$dest"
dest=$(cd "$dest" && pwd)
ios_target=$(sed -n 's/^ *iOS: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)
mac_target=$(sed -n 's/^ *macOS: *"\([0-9.]*\)".*/\1/p' "$root/project.yml" | head -1)

stamp=$( (cat "$0"; echo "$ios_target $mac_target"; cd "$root/Engines/Artemis" &&
  find art3m1s-core licenses test-host/art3m1s_api.h \( -path art3m1s-core/target -o -path art3m1s-core/tests \) -prune -o -type f -print0 |
  LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "Artemis engine is up to date in $dest"
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
    *-ios | *-ios-sim) env=(IPHONEOS_DEPLOYMENT_TARGET="$ios_target") ;;
    *) env=(MACOSX_DEPLOYMENT_TARGET="$mac_target") ;;
  esac
  # A changed deployment target needs a clean build of that target.
  if [ "$(cat "$work/$target.deployment" 2>/dev/null)" != "${env[0]}" ]; then
    rm -rf "$work/target/$target"
    echo "${env[0]}" > "$work/$target.deployment"
  fi
  (cd "$src" && env "${env[@]}" CARGO_TARGET_DIR="$work/target" rustup run "$toolchain" cargo rustc --release --lib \
    --crate-type staticlib --target "$target" --no-default-features --features metal-backend)
done

lib=libart3m1s_core.a
mkdir -p "$work/mac-universal" "$work/include"
lipo -create "$work/target/aarch64-apple-darwin/release/$lib" "$work/target/x86_64-apple-darwin/release/$lib" \
  -output "$work/mac-universal/$lib"
cp -p "$root/Engines/Artemis/test-host/art3m1s_api.h" "$work/include/"
rm -rf "$dest/ios" "$dest/licenses"
mkdir -p "$dest/ios" "$dest/licenses"
xcodebuild -create-xcframework \
  -library "$work/target/aarch64-apple-ios/release/$lib" -headers "$work/include" \
  -library "$work/target/aarch64-apple-ios-sim/release/$lib" -headers "$work/include" \
  -library "$work/mac-universal/$lib" -headers "$work/include" \
  -output "$dest/ios/Art3m1s.xcframework" >/dev/null

# Notices: each crate in the iOS build with its own license files, or, when a crate ships none, its license's text
# from Engines/Artemis/licenses under a header naming it; then the C and C++ libraries shaderc and spirv-cross build in.
(cd "$src" && rustup run "$toolchain" cargo metadata --format-version 1 >"$work/metadata.json")
(cd "$src" && rustup run "$toolchain" cargo tree --target aarch64-apple-ios --no-default-features --features metal-backend \
  -e normal,build --prefix none --format '{p}' >"$work/crates.txt")
python3 - "$src" "$root/Engines/Artemis/licenses" "$work" "$dest/licenses" <<'PY'
import json, os, re, shutil, sys
src, texts, work, out = sys.argv[1:]
meta = json.load(open(os.path.join(work, "metadata.json")))
packages = {(p["name"], p["version"]): p for p in meta["packages"]}
license_file = re.compile(r"(?i)^(licen[cs]e|copying|notice|unlicense|copyright)")
def write(name, text):
    with open(os.path.join(out, name + ".txt"), "w") as f:
        f.write(text)
own = open(os.path.join(src, "LICENSE")).read()
write("Artemis - art3m1s-core", "art3m1s-core, https://github.com/Alphaly2K/art3m1s-core. Its source, with VNDegen's "
      "changes, is Engines/Artemis/art3m1s-core in VNDegen's repository.\n\n" + own)
missing = []
seen = set()
for line in open(os.path.join(work, "crates.txt")):
    parts = line.split()
    if len(parts) < 2 or (parts[0], parts[1]) in seen:
        continue
    seen.add((parts[0], parts[1]))
    package = packages[(parts[0], parts[1].lstrip("v"))]
    folder = os.path.dirname(package["manifest_path"])
    files = sorted(f for f in os.listdir(folder) if license_file.match(f) and os.path.isfile(os.path.join(folder, f)))
    title = f"{package['name']} {package['version']}"
    if folder == src or (folder.startswith(src) and not files):
        continue  # art3m1s-core and its own crates, under its license above
    header = f"{title}, {package.get('repository') or package.get('homepage') or 'crates.io'}\n"
    if files:
        body = "\n\n".join(open(os.path.join(folder, f), errors="replace").read() for f in files)
    else:
        expression = package.get("license") or ""
        chosen = next((spdx for spdx in ("MIT", "Zlib", "Apache-2.0") if re.search(rf"\b{re.escape(spdx)}\b", expression)), None)
        if not chosen:
            missing.append(f"{title} ({expression or 'no license'})")
            continue
        authors = ", ".join(re.sub(r"\s*<[^>]*>", "", a) for a in package.get("authors") or []) or f"the {package['name']} authors"
        body = f"License: {expression}, used under {chosen}.\n\n"
        if chosen != "Apache-2.0":
            body += f"Copyright (c) {authors}\n\n"
        body += open(os.path.join(texts, chosen + ".txt")).read()
    write(f"Artemis - {title}", header + "\n" + body)
if missing:
    sys.exit("error: no license text for " + ", ".join(missing))
registry = {p["name"]: os.path.dirname(p["manifest_path"]) for p in meta["packages"]}
bundled = {
    "shaderc": ["build/shaderc/LICENSE"],
    "glslang": ["build/glslang/LICENSE.txt"],
    "SPIRV-Tools": ["build/spirv-tools/LICENSE"],
    "SPIRV-Headers": ["build/spirv-headers/LICENSE"],
}
for name, paths in bundled.items():
    write(f"Artemis - {name} (in shaderc)", "\n\n".join(open(os.path.join(registry["shaderc-sys"], p)).read() for p in paths))
cross = os.path.join(registry["spirv-cross-sys"], "native/SPIRV-Cross")
extra = sorted(os.listdir(os.path.join(cross, "LICENSES")))
write("Artemis - SPIRV-Cross", open(os.path.join(cross, "LICENSE")).read() + "".join(
    "\n\n" + open(os.path.join(cross, "LICENSES", f)).read() for f in extra))
print(f"{len(os.listdir(out))} notices")
PY
echo "$stamp" > "$dest/.vndegen-engine"
echo "Artemis engine built in $dest in $(($(date +%s) - start)) s"
