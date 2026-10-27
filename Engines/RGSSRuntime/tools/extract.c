// Development tool: lists an RGSS archive, or extracts it into a folder. Not part of the app.
#include "../src/archive.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void make_parents(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        mkdir(path, 0755);
        *p = '/';
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s archive [outdir]\n", argv[0]);
        return 2;
    }
    RGSSArchive *archive = rgss_archive_open(argv[1]);
    if (!archive) {
        fprintf(stderr, "not an RGSS archive: %s\n", argv[1]);
        return 1;
    }
    for (size_t i = 0; i < rgss_archive_count(archive); i++) {
        const char *name = rgss_archive_name(archive, (long)i);
        if (argc < 3) {
            printf("%10zu %s\n", rgss_archive_size(archive, (long)i), name);
            continue;
        }
        uint8_t *data;
        size_t size;
        if (!rgss_archive_read(archive, (long)i, &data, &size)) return 1;
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", argv[2], name);
        for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
        make_parents(path);
        FILE *out = fopen(path, "wb");
        if (!out || fwrite(data, 1, size, out) != size) {
            fprintf(stderr, "write %s: %s\n", path, strerror(errno));
            return 1;
        }
        fclose(out);
        free(data);
    }
    rgss_archive_close(archive);
    return 0;
}
