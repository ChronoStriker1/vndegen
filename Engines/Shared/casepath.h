// Finds a game's files whatever case the game names them in, as Windows does. iOS disks and case-sensitive Mac volumes
// tell `Img/Title.PNG` from `img/title.png`; Windows games often mix the two.
//
// The RGSS and KiriKiri engines call this from their file layers. Ren'Py and the web engines have their own copies of
// the same rules, in Python and Swift. Every engine plugin passes the case check in scripts/case-check.sh.
#ifndef VNDEGEN_CASEPATH_H
#define VNDEGEN_CASEPATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Writes `path` as it is on disk into `out`. When the path exists as written, `out` is the path unchanged, which costs
// one lstat. Otherwise each name below the deepest folder that exists as written is looked up in its folder's listing:
// the exact name first, then the first name in sorted order that equals it ignoring ASCII case. At the first name that
// matches nothing, the rest of the path is kept as written, so a new file is created in the right folder with the
// game's spelling.
//
// Returns 1 when `out` names an existing entry, 0 when it doesn't, and -1 when `out` is too small, in which case `out`
// holds nothing useful. `out` may not overlap `path`.
//
// Listings are cached per folder, checked against the folder's modification time, and shared between threads.
int casepath_resolve(const char *path, char *out, size_t size);

// Drops the cached listing of the folder that holds `path`, which must be written as it is on disk. Call it after
// creating, removing, or renaming `path`, for file systems whose folder times are too coarse to show the change.
void casepath_changed(const char *path);

#ifdef __cplusplus
}
#endif

#endif
