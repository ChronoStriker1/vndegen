#!/usr/bin/env bash
# Runs on a Mac. Mounts a case-sensitive APFS volume, as iPhones and iPads have, for the package tests and the case check:
#   scripts/case-volume.sh attach <image> <mount point>   makes <image>, a sparse disk image, if it is missing, and mounts it
#   scripts/case-volume.sh detach <mount point>
# The volume shows in no Finder window. A sparse image grows as files are written, up to 64 GB.
set -euo pipefail
case "${1:?attach or detach}" in
  attach)
    image=${2:?image, such as build/case-sensitive.sparseimage} mount=${3:?mount point}
    if [ ! -f "$image" ]; then
      mkdir -p "$(dirname "$image")"
      hdiutil create -quiet -type SPARSE -size 64g -fs "Case-sensitive APFS" -volname VNDegenCase "${image%.sparseimage}"
    fi
    mkdir -p "$mount"
    hdiutil attach -quiet -nobrowse -owners on -mountpoint "$mount" "$image" ;;
  detach)
    mount=${2:?mount point}
    hdiutil detach -quiet "$mount" 2>/dev/null || hdiutil detach -quiet -force "$mount" ;;
  *) echo "unknown command $1" >&2; exit 2 ;;
esac
