#include "CArchive.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { USER_BYTES = 2048, BATCH = 32 };

struct raw_sectors {
    FILE *file;
    int sector_size;
    int data_offset;
    int64_t sectors;
    int64_t position;
    unsigned char buffer[USER_BYTES * BATCH];
    unsigned char *raw;
};

static ssize_t raw_read(struct archive *a, void *client, const void **out) {
    struct raw_sectors *r = client;
    int64_t sector = r->position / USER_BYTES;
    int64_t skip = r->position % USER_BYTES;
    if (sector >= r->sectors) {
        *out = r->buffer;
        return 0;
    }
    int64_t count = r->sectors - sector < BATCH ? r->sectors - sector : BATCH;
    if (fseeko(r->file, (off_t)(sector * r->sector_size), SEEK_SET) != 0 ||
        fread(r->raw, (size_t)r->sector_size, (size_t)count, r->file) != (size_t)count) {
        archive_set_error(a, EIO, "The disc image could not be read");
        return -1;
    }
    for (int64_t i = 0; i < count; i++)
        memcpy(r->buffer + i * USER_BYTES, r->raw + i * r->sector_size + r->data_offset, USER_BYTES);
    ssize_t size = (ssize_t)(count * USER_BYTES - skip);
    *out = r->buffer + skip;
    r->position += size;
    return size;
}

static int64_t raw_seek(struct archive *a, void *client, int64_t offset, int whence) {
    struct raw_sectors *r = client;
    int64_t end = r->sectors * USER_BYTES;
    int64_t target = whence == SEEK_SET ? offset : whence == SEEK_CUR ? r->position + offset : end + offset;
    if (target < 0) {
        archive_set_error(a, EINVAL, "Seek before the start of the disc image");
        return -1;
    }
    r->position = target > end ? end : target;
    return r->position;
}

static int64_t raw_skip(struct archive *a, void *client, int64_t request) {
    struct raw_sectors *r = client;
    int64_t before = r->position;
    if (raw_seek(a, client, request, SEEK_CUR) < 0) return 0;
    return r->position - before;
}

static int raw_close(struct archive *a, void *client) {
    struct raw_sectors *r = client;
    (void)a;
    fclose(r->file);
    free(r->raw);
    free(r);
    return ARCHIVE_OK;
}

int carchive_open_raw_sectors(struct archive *a, const char *path, int sector_size, int data_offset) {
    if (sector_size < USER_BYTES || data_offset < 0 || data_offset + USER_BYTES > sector_size) return ARCHIVE_FATAL;
    struct raw_sectors *r = calloc(1, sizeof *r);
    if (!r) return ARCHIVE_FATAL;
    r->raw = malloc((size_t)sector_size * BATCH);
    r->file = fopen(path, "rb");
    if (!r->raw || !r->file) {
        if (r->file) fclose(r->file);
        free(r->raw);
        free(r);
        archive_set_error(a, ENOENT, "The disc image could not be opened");
        return ARCHIVE_FATAL;
    }
    fseeko(r->file, 0, SEEK_END);
    r->sectors = ftello(r->file) / sector_size;
    r->sector_size = sector_size;
    r->data_offset = data_offset;
    archive_read_set_read_callback(a, raw_read);
    archive_read_set_skip_callback(a, raw_skip);
    archive_read_set_seek_callback(a, raw_seek);
    archive_read_set_close_callback(a, raw_close);
    archive_read_set_callback_data(a, r);
    return archive_read_open1(a);
}
