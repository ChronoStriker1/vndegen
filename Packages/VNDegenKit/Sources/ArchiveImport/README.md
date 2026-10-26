# ArchiveImport

Turns a zip, RAR, or 7z archive into a game folder in Games. `ArchiveImportView` is the whole flow. `ArchiveImporter` is its state machine, and `ArchiveDropTarget` is a drop area the hub can place.

## Extraction library

Extraction uses the libarchive that ships with iOS and macOS. Both SDKs include `libarchive.tbd` but no headers, so `CArchive/include/CArchive.h` declares the calls this module makes, following libarchive 3.7's `archive.h`. libarchive is BSD-2-Clause and adds nothing to the app's size.

It was chosen by testing on archives from the user's library (2026-10-25). The system libarchive on macOS (3.7.4, built with zlib, liblzma, and bzip2) extracted all of these. The iOS 27 simulator extracted all but the zip64 and split 7z ones, which were tested on macOS only:

| Archive | Format | Result |
|---|---|---|
| Test Game 01 (516 MB) | zip, Deflate, 2,668 code page 932 names without the UTF-8 flag | 2,635 files, names decoded |
| Test Game 02 (155 MB) | RAR5 | 609 files |
| Test Game 03 (188 MB) | RAR4 | 641 files |
| Test Game 04 (145 MB) | 7z, LZMA2 + LZMA + BCJ2, solid | 1,596 files |
| Test Game 05 (1.4 GB) | RAR4, 29 old-style volumes (`.rar`, `.r00` to `.r27`) | 1 file, CRC matches |
| Two scene RARs | RAR4 holding another RAR | nested import |
| Synthetic | zip64 (4.6 GB entry), split `.7z.001`, AES-256 and ZipCrypto zips | extracted |

Two gaps showed up, and none of the 516 archives in the user's library hits either. A header scan of all 149 zips found only Store and Deflate, and no RAR or 7z there has encrypted headers or an encrypted first file.

- **Deflate64.** libarchive reports "Unsupported ZIP compression method (9)". Windows writes Deflate64 when it zips large files, so `Deflate64.swift` reads those entries straight from the zip with zlib's `contrib/infback9` (zlib license, vendored unmodified in `CArchive/`, see `LICENSE-zlib.txt`).
- **RAR and 7z encryption.** libarchive decrypts zip only. Encrypted RAR or 7z archives get a message saying so. Supporting them would mean adding UnRAR (freeware license that forbids writing RAR) and 7-Zip's AES code.

No other candidate covers everything either. SWCompression decodes in memory, which rules out multi-GB archives, and has no RAR. ZIPFoundation is zip only and has no Deflate64. UnrarKit is RAR only.

## Behavior

- **Names.** A name that is valid UTF-8 stays UTF-8. Anything else is decoded as code page 932, the Windows superset of Shift-JIS, then Windows-1252. Path parts `.` and `..` are dropped, so an entry can't escape the game folder.
- **Layout.** When the archive holds one top-level folder, that folder's contents become the game folder. `__MACOSX` and `.DS_Store` don't count.
- **Volumes.** Picking any part of `name.partN.rar`, `name.rar` with `name.r00`, or `name.7z.001` pulls in the other parts when their folder can be listed. Outside the app's container on iOS, the picker grants access only to the picked files, so the user selects every part (inferred from the sandbox rules, not tested). Missing parts are reported by number. No `.partN.rar` set exists in the user's library, so that naming is covered by unit tests only.
- **Staging.** Files are extracted into `Games/.importing/<UUID>` and moved into Games only when everything succeeded. A failure or cancel deletes the staging folder. The library scan skips dot folders, and a new `ArchiveImporter` deletes staging folders left by a crash.
- **Nested archives.** When the result has no subfolders and holds one archive (or one volume set) next to loose files such as a readme, the user can extract that too. Accepting keeps only the inner archive's contents.
- **Space and progress.** A listing pass reads headers only and seeks past file data. On the Mac mini it took 0.3 s or less per test archive once cached, and 3.2 s for one uncached 145 MB zip. It gives the exact byte and file totals for the free-space check and the progress bar. Each file and folder also counts 4 KB, so an archive of many empty files can't fill the device. The import starts only when that total plus a 100 MB reserve is free.
- **Limits.** Header sizes are untrusted. A negative size, or a total over 1 PiB, rejects the archive before anything is written. While extracting, the import stops when the files use more than the listing promised, which is what a decompression bomb or an archive changed after listing does, and when free space falls below the 100 MB reserve. Free space is checked every 64 MB written or every quarter second. Deflate64 entries stop at their listed size while they decompress.
- **Deleting the archive.** "Delete archive after import" is offered only for an archive inside VNDegen's own folders: the app's container on iOS, or the Games folder on the Mac. Anywhere else the file may be synced by iCloud Drive, Dropbox, OneDrive, or a server, and deleting it would delete it on the user's other devices too.
