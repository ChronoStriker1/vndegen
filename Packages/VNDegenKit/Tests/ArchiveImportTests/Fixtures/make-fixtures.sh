#!/usr/bin/env bash
# Rebuilds the test archives in this folder. Needs 7-Zip (`brew install sevenzip`) and python3.
set -euo pipefail
cd "$(dirname "$0")"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
rm -f *.zip *.7z *.7z.0* disc.bin disc.cue

# Text whose exact bytes the tests rebuild: "line 1\n" through "line 20000\n".
lines() { python3 -c 'import sys; sys.stdout.write("".join(f"line {i}\n" for i in range(1, 20001)))'; }

# A zip written the way Japanese Windows writes one: code page 932 names and no UTF-8 flag.
# ① is in code page 932 but not in plain Shift-JIS.
python3 - <<'PY'
import zipfile
class CP932Info(zipfile.ZipInfo):
    def _encodeFilenameFlags(self):
        return self.filename.encode("cp932"), self.flag_bits & ~0x800
with zipfile.ZipFile("sjis.zip", "w") as z:
    for name, data in [("ゲーム/www/js/rpg_core.js", b"core"), ("ゲーム/セーブ①.txt", b"save")]:
        z.writestr(CP932Info(name), data)
PY

# A zip that names one folder in three cases, as Windows tools write when files are added at different times.
python3 - <<'PY'
import zipfile
with zipfile.ZipFile("mixed-case.zip", "w") as z:
    z.writestr("Mixed/Img/", b"")
    for name, data in [("Mixed/Img/a.png", b"a"), ("Mixed/img/b.png", b"b"), ("Mixed/IMG/Sub/c.png", b"c"), ("Mixed/img/A.PNG", b"A")]:
        z.writestr(name, data)
PY

mkdir -p "$work/Deflate Game"
lines > "$work/Deflate Game/lines.txt"
echo small > "$work/Deflate Game/small.txt"
(cd "$work" && 7zz a -tzip -mm=Deflate64 "$OLDPWD/deflate64.zip" "Deflate Game" >/dev/null)

mkdir -p "$work/Locked Game"
echo locked > "$work/Locked Game/locked.txt"
(cd "$work" && 7zz a -tzip -mem=AES256 -psecret "$OLDPWD/password.zip" "Locked Game" >/dev/null)

mkdir -p "$work/inner/Inner Game/www/js" "$work/outer/Outer"
echo core > "$work/inner/Inner Game/www/js/rpg_core.js"
(cd "$work/inner" && 7zz a -t7z "$work/outer/Outer/inner.7z" "Inner Game" >/dev/null)
echo "Extract inner.7z" > "$work/outer/Outer/readme.txt"
(cd "$work/outer" && 7zz a -tzip "$OLDPWD/nested.zip" Outer >/dev/null)

mkdir -p "$work/two"
echo a > "$work/two/a.txt"
mkdir -p "$work/two/folder" && echo b > "$work/two/folder/b.txt"
(cd "$work/two" && 7zz a -tzip "$OLDPWD/two-items.zip" a.txt folder >/dev/null)

mkdir -p "$work/Split Game"
lines > "$work/Split Game/lines.txt"
(cd "$work" && 7zz a -t7z -mx=0 -v64k "$OLDPWD/split.7z" "Split Game" >/dev/null)
# A CD image as BIN/CUE: one MODE1/2352 data track holding an ISO 9660 disc with an installer at its root, the game in
# its own folder, and an extras folder, as a game shipped on a CD-ROM. Each 2048-byte sector gets a 16-byte sync and
# header in front and 288 bytes of zeros where the EDC and ECC go.
mkdir -p "$work/disc/GAME" "$work/disc/EXTRA"
printf '[autorun]\r\nopen=SETUP.EXE\r\n' > "$work/disc/AUTORUN.INF"
echo setup > "$work/disc/SETUP.EXE"
echo game > "$work/disc/GAME/GAME.EXE"
lines > "$work/lines.txt" && head -c 20000 "$work/lines.txt" > "$work/disc/GAME/lines.txt"
echo movie > "$work/disc/EXTRA/MOVIE.MPG"
hdiutil makehybrid -quiet -iso -joliet -o "$work/disc.iso" "$work/disc"
python3 - "$work/disc.iso" <<'PY'
import sys
iso = open(sys.argv[1], "rb").read()
out = bytearray()
for n in range(len(iso) // 2048):
    lba = n + 150
    msf = bytes(int(str(v), 16) for v in (lba // 4500, lba // 75 % 60, lba % 75))
    out += b"\x00" + b"\xff" * 10 + b"\x00" + msf + b"\x01" + iso[n * 2048:(n + 1) * 2048] + bytes(288)
open("disc.bin", "wb").write(out)
open("disc.cue", "w").write('FILE "disc.bin" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n')
PY
ls -l
