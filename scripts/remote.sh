#!/usr/bin/env bash
# Syncs this checkout to a Mac and runs a command there from the synced copy.
#   scripts/remote.sh scripts/build.sh ios-sim
#   VNDEGEN_MAC=other-mac.local scripts/remote.sh scripts/build.sh mac
# The default Mac comes from ~/.config/vndegen/mac-host.
# Each checkout (worktree) syncs to its own folder, ~/vndegen-build/<checkout name>, so parallel agents never share one.
set -euo pipefail
host=${VNDEGEN_MAC:-$(cat ~/.config/vndegen/mac-host 2>/dev/null)}
[ -n "$host" ] || { echo "set VNDEGEN_MAC or write the Mac's hostname to ~/.config/vndegen/mac-host" >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
dest="vndegen-build/${VNDEGEN_REMOTE_NAME:-$(basename "$root")}"
ssh -o BatchMode=yes "chronostriker1@$host" "mkdir -p ~/$dest"
rsync -az --delete --exclude .git --exclude build --exclude '*.xcodeproj' --exclude .audit \
  --exclude .build --exclude .swiftpm --exclude /Engines/Artemis/art3m1s-core/target "$root/" "chronostriker1@$host:$dest/"
ssh -o BatchMode=yes "chronostriker1@$host" "export PATH=/opt/homebrew/bin:\$PATH; cd ~/$dest && $*"
