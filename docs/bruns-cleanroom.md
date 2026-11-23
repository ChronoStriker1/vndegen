# Bruns runtime: where the knowledge came from

The Bruns runtime was written for VNDegen. It derives from no GPL or LGPL code, and no part of the engine's code was
read. This file lists its sources.

## Sources

| Part | Sources |
|---|---|
| File encryption (`EENC`, `EENZ`) | GARbro `ArcFormats/Bruns/ImageEENC.cs` (MIT): the signature, the 32-bit key at offset 4 XORed with `0xDEADBEEF`, the body XORed with the key's four bytes in turn, and zlib after that for `EENZ`. Writing files for the original showed that the 32-bit value is the unpacked size |
| Audio (`.um3`) | GARbro `ArcFormats/Bruns/AudioUM3.cs` (MIT): Ogg Vorbis with its first 0x800 bytes XORed with `0xFF` |
| Scripts (`.bso`) | The game's own scripts, which decode to readable S-expressions (`a3:define-sub`, `a3:call-user-func` and so on) |
| Configuration (`ams.cfg`) | The game's own file, which decodes to commented S-expressions naming the resource paths, the parts table, the message windows and the menu |
| Builtins (`call-user-func`) | Their use in the game's scripts, and the original under Wine as a black box: small test scripts, written as text, encoded with the format above and put in place of the game's first system script, show what each argument does |
| Bitmap fonts (`font/ff_*.bmp`) | The files themselves: 4-bit grey sheets of 32 or 64 pixel cells, one per Shift JIS code from `0x40` to `0xFF` after each lead byte, and half-width ASCII from `0x20` |
| Licence and comment dialogs (7002, 7001) | The game's scripts (the arguments each passes and what they do with the result) and the original under Wine as a black box: the dialogs it shows, and the comment it stores for typed text. The game starts under Wine without its installer once `HKLM\SOFTWARE\MSRUSH\379MON` has a `Mode` value, which `ams.cfg`'s own notes say is all it checks |
