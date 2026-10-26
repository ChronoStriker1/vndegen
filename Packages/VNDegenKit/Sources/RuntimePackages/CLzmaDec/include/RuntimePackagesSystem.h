/* System libraries RuntimePackages calls. Every Apple SDK ships zlib and bzip2 with their headers. It ships libarchive
 * without headers, so the few calls used here are declared from archive.h and archive_entry.h of libarchive 3.7. */
#ifndef RUNTIME_PACKAGES_SYSTEM_H
#define RUNTIME_PACKAGES_SYSTEM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <zlib.h>
#include <bzlib.h>

struct archive;
struct archive_entry;

#define ARCHIVE_EOF 1
#define ARCHIVE_OK 0
#define ARCHIVE_WARN (-20)

static const mode_t AE_IFREG = 0100000;

struct archive *archive_read_new(void);
int archive_read_free(struct archive *);
int archive_read_support_format_zip_seekable(struct archive *);
int archive_read_open_filename(struct archive *, const char *filename, size_t block_size);
int archive_read_next_header(struct archive *, struct archive_entry **);
int archive_read_data_block(struct archive *, const void **buffer, size_t *size, int64_t *offset);
const char *archive_error_string(struct archive *);

const char *archive_entry_pathname(struct archive_entry *);
mode_t archive_entry_filetype(struct archive_entry *);

#endif
