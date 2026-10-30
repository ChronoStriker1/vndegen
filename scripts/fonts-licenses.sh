#!/usr/bin/env bash
# Writes build/fonts/licenses/, the notices of the fonts the app bundles from Engines/Shared/fonts, one text file per
# font family. The Chinese and Korean fonts aren't bundled. Their package installs Adobe's LICENSE.txt beside them.
#   scripts/fonts-licenses.sh
set -euo pipefail
cd "$(dirname "$0")/.."
src=Engines/Shared/fonts/licenses
out=build/fonts/licenses
rm -rf "$out.part" && mkdir -p "$out.part"
cp "$src/Liberation-2.1.5-LICENSE" "$out.part/Liberation fonts 2.1.5 (Arial, Times New Roman, Courier New).txt"
cp "$src/DejaVu-2.37-LICENSE" "$out.part/DejaVu fonts 2.37 (Tahoma).txt"
# The Ume fonts' license is a web page. Its text, without the markup, is the notice.
sed -e 's/<[^>]*>//g' -e '/^[[:space:]]*$/d' "$src/umefont-670-license.html" >"$out.part/Ume fonts 670 (MS Gothic, MS Mincho).txt"
rm -rf "$out" && mv "$out.part" "$out"
