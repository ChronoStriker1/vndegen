# SystemAoi

VNDegen is getting a runtime of its own for Soft House Chara's SystemAoi engine, version 5 (`Aoi5.dll`, `Ags5.dll`),
`Packages/VNDegenKit/Sources/CSystemAoi`, written in C from the game's files, GARbro's MIT-licensed archive, script and
picture readers, the game's own manuals, and the original engine run under Wine. It is MIT licensed like the rest of
the app. `docs/systemaoi-cleanroom.md` lists where each part came from, rule by rule for the game's systems. Work in
progress on branch `feat/systemaoi`; nothing ships yet.

## Test game

| Game | Engine | Notes |
|---|---|---|
| BUNNY BLACK 3 (DVD, `Bunny3.exe` 2013-06) | SystemAoi 5, 1280x720 | Japanese only. A dungeon RPG with town building: story scripts, and battles, the 3D dungeon, unit building and the town in the exe |

## How the game is built

The game is the `Bunny3/` folder of the DVD image. The installer only copies it and writes the registry value
`HKLM\Software\SoftHouseChara\Bunny3\path`; saves go to `Bunny3.sav\` beside the game.

- Archives: `VF` version 0x200 (`.vfs`), a 16-byte header (count, entry size 23, index size, file size), 23-byte
  entries (name offset, six bytes VNDegen doesn't use, offset, size, unpacked size, a packed flag), then the UTF-16
  name table. Nothing is packed. Big archives are split into parts (`evA.vfs` to `evD.vfs`) with a `VL` index (`.vfl`)
  over all parts, whose last entry byte is the part letter; each part also has its own full index, so VNDegen mounts
  the parts and ignores the `VL` files. `sam.vfs` repeats the event pictures' names as small samples, so names resolve
  to the first archive in name order.
- Scripts: `box.vfs` holds 33 `AOIMY01` archives (a UTF-16 magic, a big-endian count, 40-byte entries with a UTF-16
  name and big-endian offset and size), each entry XORed with a key computed from its byte offset in the archive.
- Bytecode: one-byte opcodes, each followed by tagged operands: `0x82` a big-endian int32, `0x84` a byte, `0x88` a
  length byte and that many UTF-16 characters, `0x92` an absolute address in the script, and a variable as a bank byte
  and a big-endian 16-bit index. Banks: `02` C (integers), `04` F (flags), `22` CL and `24` FL (the running script's
  own integers and flags), `44` FG (system flags). Conditions are text after `1a` and `1c`, such as `F133 on`,
  `C055 == 0` or `CL000 >= 1`, naming the same banks; `1a` jumps when its condition is false.
- Scripts are the story and hooks. `bo0000.box` script 0 runs once a turn and calls the turn's events; the town, the
  dungeon and battles are the exe's, and the scripts call into them by name (a facility, a unit joining, an enemy
  party from `party.obj`).
- Tables: `obj.vfs`, 24 tables with every byte inverted: integer records, a UTF-16 string pool and a column signature.
- Pictures: AGF version 1 and 2, run-length and back-reference operations over BGRA pixels.
- Sound: BGM Ogg Vorbis, voices `AoiOgg`-wrapped Ogg named by the script (`!PC0001` is `PC0001.aog`), effects WAV.
- Movies: `data1.dat` and `data2.dat`, ASF with WMV1 video (1280x720, 30 fps) and WMA version 2 sound.
- 3D: `model.vfs` holds Pegasus3D text models (`.pvm`: a tree of layers with a material, a texture, a blend, and
  triangles over position, normal and uv), a text camera (`Cam.pvc`) and text lights (`Light.pvl`), with BMP
  textures. Each dungeon has a part set (`m000set.pvm` to `m100set.pvm`: floors, poles, roofs, walls, doors, stairs).
  `map.vfs` holds 117 floor maps (`mpDDFF.amd`), 15 by 15 cells of 104 bytes after a 48-byte header, each cell with an
  open-side mask (1 north, 2 east, 4 south, 8 west) and an event number.
