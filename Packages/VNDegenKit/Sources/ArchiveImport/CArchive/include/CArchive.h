/* The libarchive calls ArchiveImport uses. iOS and macOS ship libarchive (libarchive.tbd in both SDKs) but not its
 * headers, so these declarations follow archive.h and archive_entry.h from libarchive 3.7. */
#ifndef CARCHIVE_H
#define CARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <zlib.h>
#include "infback9.h"

struct archive;
struct archive_entry;

#define ARCHIVE_EOF 1
#define ARCHIVE_OK 0
#define ARCHIVE_RETRY (-10)
#define ARCHIVE_WARN (-20)
#define ARCHIVE_FAILED (-25)
#define ARCHIVE_FATAL (-30)

#define ARCHIVE_READ_FORMAT_CAPS_ENCRYPT_DATA (1 << 0)

static const mode_t AE_IFREG = 0100000;
static const mode_t AE_IFDIR = 0040000;

const char *archive_version_details(void);

struct archive *archive_read_new(void);
int archive_read_free(struct archive *);
int archive_read_support_format_zip_seekable(struct archive *);
int archive_read_support_format_rar(struct archive *);
int archive_read_support_format_rar5(struct archive *);
int archive_read_support_format_7zip(struct archive *);
int archive_read_add_passphrase(struct archive *, const char *);
int archive_read_open_filenames(struct archive *, const char **filenames, size_t block_size);
int archive_read_next_header(struct archive *, struct archive_entry **);
int archive_read_data_block(struct archive *, const void **buffer, size_t *size, int64_t *offset);
int archive_read_format_capabilities(struct archive *);
const char *archive_error_string(struct archive *);

const char *archive_entry_pathname(struct archive_entry *);
const char *archive_entry_pathname_utf8(struct archive_entry *);
mode_t archive_entry_filetype(struct archive_entry *);
int64_t archive_entry_size(struct archive_entry *);
int archive_entry_size_is_set(struct archive_entry *);
int archive_entry_is_encrypted(struct archive_entry *);

#endif
