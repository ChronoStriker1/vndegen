#include "CArchive.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

enum { WINDOW_BUFFER = 1 << 20 };

struct window {
    FILE *file;
    int64_t start;
    int64_t length;
    int64_t position;
    unsigned char buffer[WINDOW_BUFFER];
};

static ssize_t window_read(struct archive *a, void *client, const void **out) {
    struct window *w = client;
    *out = w->buffer;
    int64_t left = w->length - w->position;
    if (left <= 0) return 0;
    size_t count = left < WINDOW_BUFFER ? (size_t)left : WINDOW_BUFFER;
    if (fseeko(w->file, (off_t)(w->start + w->position), SEEK_SET) != 0) {
        archive_set_error(a, EIO, "The file could not be read");
        return -1;
    }
    size_t got = fread(w->buffer, 1, count, w->file);
    if (got == 0) {
        archive_set_error(a, EIO, "The file could not be read");
        return -1;
    }
    w->position += (int64_t)got;
    return (ssize_t)got;
}

static int64_t window_seek(struct archive *a, void *client, int64_t offset, int whence) {
    struct window *w = client;
    int64_t target = whence == SEEK_SET ? offset : whence == SEEK_CUR ? w->position + offset : w->length + offset;
    if (target < 0) {
        archive_set_error(a, EINVAL, "Seek before the start of the archive");
        return -1;
    }
    w->position = target > w->length ? w->length : target;
    return w->position;
}

static int64_t window_skip(struct archive *a, void *client, int64_t request) {
    struct window *w = client;
    int64_t before = w->position;
    if (window_seek(a, client, request, SEEK_CUR) < 0) return 0;
    return w->position - before;
}

static int window_close(struct archive *a, void *client) {
    struct window *w = client;
    (void)a;
    fclose(w->file);
    free(w);
    return ARCHIVE_OK;
}

int carchive_open_window(struct archive *a, const char *path, int64_t start) {
    struct window *w = calloc(1, sizeof *w);
    if (!w) return ARCHIVE_FATAL;
    w->file = fopen(path, "rb");
    if (!w->file || fseeko(w->file, 0, SEEK_END) != 0) {
        if (w->file) fclose(w->file);
        free(w);
        archive_set_error(a, ENOENT, "The file could not be opened");
        return ARCHIVE_FATAL;
    }
    int64_t size = ftello(w->file);
    if (start < 0 || start > size) {
        fclose(w->file);
        free(w);
        archive_set_error(a, EINVAL, "The archive starts past the end of the file");
        return ARCHIVE_FATAL;
    }
    w->start = start;
    w->length = size - start;
    archive_read_set_read_callback(a, window_read);
    archive_read_set_skip_callback(a, window_skip);
    archive_read_set_seek_callback(a, window_seek);
    archive_read_set_close_callback(a, window_close);
    archive_read_set_callback_data(a, w);
    return archive_read_open1(a);
}
