// RGSS's encrypted archives: Game.rgssad (XP), Game.rgss2a (VX) and Game.rgss3a (VX Ace). docs/rgss-cleanroom.md
// lists the sources for the format.
#ifndef RGSS_ARCHIVE_H
#define RGSS_ARCHIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RGSSArchive RGSSArchive;

// Opens an archive and reads its index. NULL when the file is missing or is not an RGSS archive.
RGSSArchive *rgss_archive_open(const char *path);
void rgss_archive_close(RGSSArchive *archive);

// The entry for a path, matched without regard to ASCII case or the kind of slash. -1 when there is none.
long rgss_archive_find(const RGSSArchive *archive, const char *path);
// Decrypts an entry into a buffer the caller frees. False on a read error.
bool rgss_archive_read(RGSSArchive *archive, long entry, uint8_t **data, size_t *size);

size_t rgss_archive_count(const RGSSArchive *archive);
// An entry's path as stored, with backslashes.
const char *rgss_archive_name(const RGSSArchive *archive, long entry);
size_t rgss_archive_size(const RGSSArchive *archive, long entry);

// Folds a path for lookups: ASCII letters to lower case and backslashes to slashes, into `out`, which holds `len`.
void rgss_fold_path(const char *path, char *out, size_t len);

#endif
