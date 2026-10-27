#!/usr/bin/env bash
# Runs on Linux. Builds Ruffle's web player into Engines/Flash/ruffle/, which is committed, so the app's build needs no
# Rust, Java or Node. It builds the commit pinned below with the patches in Engines/Flash/patches/, then writes
# ruffle.js, the wasm module, SHA256SUMS, and licenses/, the notices of everything the build contains.
#   scripts/ruffle-web.sh            build and replace Engines/Flash/ruffle/
#   scripts/ruffle-web.sh --check    build in a scratch folder and compare with the committed SHA256SUMS
# The build is reproducible: the source tree and Cargo's home are remapped to fixed paths, and Ruffle's version seal
# pins its version text and build date to the commit's. The toolchain must match the pins below, on PATH:
#   rustup with Rust $rust and its wasm32-unknown-unknown target, wasm-bindgen $wasm_bindgen
#   (cargo install wasm-bindgen-cli --version $wasm_bindgen --locked), wasm-opt from binaryen $binaryen, a Java 21
#   runtime for Ruffle's AS3 library, and Node with npm.
# Sources are cloned into $RUFFLE_WORK, ~/.cache/vndegen/ruffle by default.
set -euo pipefail
repo=https://github.com/ruffle-rs/ruffle.git
commit=07d3e430631333f80f754df01a968bdcd80d98c5
rust=1.99.1
wasm_bindgen=0.2.127
binaryen=version_133

root=$(cd "$(dirname "$0")/.." && pwd)
flash=$root/Engines/Flash
work=${RUFFLE_WORK:-$HOME/.cache/vndegen/ruffle}
src=$work/src
cargo_home=${CARGO_HOME:-$HOME/.cargo}

check=false
[ "${1:-}" = --check ] && check=true

need() { # <command> <version text it must print>
  "$1" --version 2>&1 | grep -qF "$2" || { echo "error: $1 $2 is required, found: $("$1" --version 2>&1 | head -1)" >&2; exit 1; }
}
export RUSTUP_TOOLCHAIN=$rust
need rustc "rustc $rust"
rustup target list --installed | grep -qx wasm32-unknown-unknown || { echo "error: rustup target add wasm32-unknown-unknown --toolchain $rust" >&2; exit 1; }
need wasm-bindgen "wasm-bindgen $wasm_bindgen"
need wasm-opt "version ${binaryen#version_}"
java -version 2>&1 | grep -q 'version "21' || { echo "error: a Java 21 runtime is required" >&2; exit 1; }
need node v

if [ ! -d "$src/.git" ]; then
  mkdir -p "$work"
  git clone --quiet --filter=blob:none "$repo" "$src"
fi
git -C "$src" fetch --quiet origin "$commit" 2>/dev/null || true
git -C "$src" checkout --quiet --force "$commit"
git -C "$src" clean --quiet -fdx -e node_modules -e target
for patch in "$flash"/patches/*.patch; do
  git -C "$src" apply "$patch"
done

# The seal replaces the build date and version text that Ruffle would take from the clock.
version=$(node -p "require('$src/web/package.json').version")
date=$(git -C "$src" log -1 --format=%cI)
cat > "$src/web/version_seal.json" <<EOF
{"version_number":"$version","version_name":"$version VNDegen ${commit:0:9}","version_channel":"local","build_date":"$date","commitHash":"$commit","version4":"","firefox_extension_id":"ruffle@ruffle.rs"}
EOF

(
  cd "$src/web"
  npm ci --no-audit --no-fund --loglevel=error
  export ENABLE_VERSION_SEAL=true
  export RUSTFLAGS="--remap-path-prefix=$src=/ruffle --remap-path-prefix=$cargo_home=/cargo"
  npm run build --workspace=ruffle-core
  npm run build --workspace=ruffle-selfhosted
)

dist=$src/web/packages/selfhosted/dist
if grep -rlF "$HOME" "$dist"/ruffle.js "$dist"/*.wasm; then
  echo "error: the build holds a path from this machine" >&2
  exit 1
fi

out=$work/out
rm -rf "$out" && mkdir -p "$out/licenses"
cp "$dist"/ruffle.js "$dist"/*.wasm "$out/"
(cd "$out" && sha256sum ruffle.js ./*.wasm | sed 's| \./| |' > SHA256SUMS)

# The notices: Ruffle's own, each Rust crate in the wasm module with the license files it ships, the JavaScript
# packages bundled into ruffle.js, and the texts in Engines/Flash/licenses/ for what has none in the source tree.
{
  echo "Ruffle $version, commit $commit, with VNDegen's patches. Source: $repo"
  echo
  cat "$src/LICENSE.md" "$src/web/packages/selfhosted/LICENSE_MIT" "$src/web/packages/selfhosted/LICENSE_APACHE"
} > "$out/licenses/Ruffle (Flash player).txt"
(cd "$src" && cargo metadata --format-version 1 --locked --filter-platform wasm32-unknown-unknown > "$work/metadata.json")
(cd "$src" && cargo tree -p ruffle_web --target wasm32-unknown-unknown -e normal,no-proc-macro --prefix none --locked \
  -f '{p}' | sed 's/ (.*//; s/ (\*)//' | sort -u > "$work/crates.txt")
python3 -I - "$work/metadata.json" "$work/crates.txt" "$out/licenses/Ruffle's Rust libraries.txt" <<'EOF'
import json, os, re, sys
metadata, crates, target = sys.argv[1:4]
packages = {(p["name"], p["version"]): p for p in json.load(open(metadata))["packages"]}
# Crates under a choice of licenses, recorded with the one VNDegen takes where the choice includes a copyleft one.
chosen = {"self_cell": "Apache-2.0"}
out = []
for line in sorted(set(open(crates).read().split("\n")) - {""}):
    name, version = line.split()[0], line.split()[1].lstrip("v")
    package = packages[(name, version)]
    if package["source"] is None:
        continue  # Ruffle's own crates, under Ruffle's license.
    folder = os.path.dirname(package["manifest_path"])
    # License files at the top of the crate, or in a LICENSES folder. A crate from a Git repository may have them at
    # the repository's top instead.
    if package["source"].startswith("git+"):
        for _ in range(2):
            if any(re.match(r"(?i)(licen[cs]e|copying)", f) for f in os.listdir(folder)):
                break
            folder = os.path.dirname(folder)
    files = []
    for f in sorted(os.listdir(folder)):
        if not re.match(r"(?i)(licen[cs]e|copying|notice|copyright)", f):
            continue
        if os.path.isdir(os.path.join(folder, f)):
            files += sorted(os.path.join(f, g) for g in os.listdir(os.path.join(folder, f)))
        else:
            files.append(f)
    license = package["license"] or "see the files below"
    if name in chosen:
        license += f", used under {chosen[name]}"
    out.append(f"==== {name} {version}: {license}\nSource: {package.get('repository') or package['source']}\n")
    if not files:
        # A few crates ship only the license's name in Cargo.toml. Their texts are the standard ones: Ruffle's own
        # notice carries MIT and Apache-2.0, and Engines/Flash/licenses/ the MPL-2.0 that symphonia uses.
        terms = set(re.findall(r"[\w.\-]+", license))
        if terms & {"MIT", "Apache-2.0"}:
            where = "the MIT and Apache-2.0 texts in Ruffle's notice"
        elif terms == {"MPL-2.0"} and name.startswith("symphonia"):
            where = "the notice for symphonia"
        else:
            sys.exit(f"error: {name} {version} ships no license file and is under {license}")
        authors = ", ".join(package.get("authors") or []) or "its authors"
        out.append(f"Copyright {authors}. Ships no license file. {license}: see {where}.\n")
    for f in files:
        out.append(f"---- {f}\n" + open(os.path.join(folder, f), encoding="utf-8", errors="replace").read().rstrip() + "\n")
open(target, "w").write("Rust libraries in Ruffle's wasm module\n\n" + "\n".join(out))
EOF
python3 -I - "$dist/ruffle.js.map" "$src/web/node_modules" "$out/licenses/Ruffle's JavaScript libraries.txt" <<'EOF'
import json, os, re, sys
sourcemap, modules, target = sys.argv[1:4]
names = sorted({m.group(1) for s in json.load(open(sourcemap))["sources"] if (m := re.search(r"node_modules/((?:@[^/]+/)?[^/]+)", s))})
out = []
for name in names:
    folder = os.path.join(modules, name)
    package = json.load(open(os.path.join(folder, "package.json")))
    files = sorted(f for f in os.listdir(folder) if re.match(r"(?i)(licen[cs]e|copying|notice)", f))
    license = package.get("license") or ""
    out.append(f"==== {name} {package['version']}: {license}\n")
    if not files:
        # Packages that only name their license get the standard text from Ruffle's notice, as the Rust crates do.
        if license not in ("MIT", "Apache-2.0"):
            sys.exit(f"error: {name} ships no license file and is under {license}")
        author = package.get("author")
        author = author.get("name") if isinstance(author, dict) else author
        repository = package.get("repository")
        repository = repository.get("url") if isinstance(repository, dict) else repository
        out.append(f"Copyright {author or 'its authors'}. Source: {repository}. Ships no license file. {license}: see the text in Ruffle's notice.\n")
    for f in files:
        out.append(f"---- {f}\n" + open(os.path.join(folder, f), encoding="utf-8").read().rstrip() + "\n")
open(target, "w").write("JavaScript libraries bundled into ruffle.js\n\n" + "\n".join(out))
EOF
cp "$flash"/licenses/*.txt "$out/licenses/"

if $check; then
  if diff -u "$flash/ruffle/SHA256SUMS" "$out/SHA256SUMS"; then
    echo "the build reproduces Engines/Flash/ruffle/SHA256SUMS"
  else
    echo "error: the build differs from Engines/Flash/ruffle/SHA256SUMS" >&2
    exit 1
  fi
  exit 0
fi
rm -rf "$flash/ruffle"
cp -R "$out" "$flash/ruffle"
cat "$flash/ruffle/SHA256SUMS"
