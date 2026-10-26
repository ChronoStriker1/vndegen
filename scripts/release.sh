#!/usr/bin/env bash
# Publishes a GitHub release from main: builds both platforms on a Mac, tags, and uploads the IPA and the macOS app.
#   scripts/release.sh 0.1.0
# Release notes come from docs/releases/<version>.md, which must be committed first.
set -euo pipefail
version=${1:?version, such as 0.1.0}
root=$(git rev-parse --show-toplevel)
cd "$root"
[ "$(git branch --show-current)" = main ] || { echo "check out main first" >&2; exit 2; }
git diff --quiet && git diff --cached --quiet || { echo "commit or stash changes first" >&2; exit 2; }
[ -f "docs/releases/$version.md" ] || { echo "write docs/releases/$version.md first" >&2; exit 2; }
git fetch -q origin main
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || { echo "push main first" >&2; exit 2; }
! git rev-parse -q --verify "refs/tags/v$version" >/dev/null || { echo "v$version already exists" >&2; exit 2; }

build_number=$(git rev-list --count HEAD)
host=${VNDEGEN_MAC:-build-mac.local}
remote_dir="vndegen-build/${VNDEGEN_REMOTE_NAME:-$(basename "$root")}"
scripts/remote.sh scripts/build.sh release "$version" "$build_number"

out="build/release/$version"
rm -rf "$out" && mkdir -p "$out"
scp -q "chronostriker1@$host:$remote_dir/build/release/*" "$out/"
(cd "$out" && sha256sum -- * > SHA256SUMS)

git tag -a "v$version" -m "VNDegen $version"
git push -q origin "v$version"
gh release create "v$version" "$out"/* --title "VNDegen $version" --notes-file "docs/releases/$version.md"
