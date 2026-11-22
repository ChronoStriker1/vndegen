#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"
#include "sa.h"

static int usage(void) {
    fprintf(stderr, "satool <gameDir> list | extract <name> <out> | box <name.box> <outDir> | agf <name> <out.png>\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 3) return usage();
    Vfs v;
    if (!sa_vfs_open(&v, argv[1])) {
        fprintf(stderr, "no archives in %s\n", argv[1]);
        return 1;
    }
    const char *cmd = argv[2];
    int rc = 0;
    if (!strcmp(cmd, "list")) {
        for (int i = 0; i < v.count; i++) printf("%s\t%s\t%u\n", v.arcName[v.e[i].arc], v.e[i].name, v.e[i].size);
    } else if (!strcmp(cmd, "extract") && argc == 5) {
        size_t n;
        u8 *d = sa_vfs_read(&v, argv[3], &n);
        rc = !d || !sa_write_file(argv[4], d, n);
        free(d);
    } else if (!strcmp(cmd, "box") && argc == 5) {
        size_t n;
        u8 *d = sa_vfs_read(&v, argv[3], &n);
        Box b;
        if (!d || !sa_box_parse(&b, d, n)) {
            rc = 1;
        } else {
            for (int i = 0; i < b.count; i++) {
                char *p = sa_fmt("%s/%s", argv[4], b.e[i].name);
                sa_write_file(p, b.e[i].data, b.e[i].size);
                free(p);
            }
            printf("%d entries\n", b.count);
            sa_box_free(&b);
        }
        free(d);
    } else if (!strcmp(cmd, "agf") && argc == 5) {
        size_t n;
        u8 *d = sa_vfs_read(&v, argv[3], &n);
        Image *im = d ? sa_agf_decode(d, n) : NULL;
        if (im) {
            write_png(argv[4], im->px, im->w, im->h, 1, 1);
            printf("%dx%d\n", im->w, im->h);
        } else {
            rc = 1;
        }
        sa_image_free(im);
        free(d);
    } else {
        rc = usage();
    }
    sa_vfs_close(&v);
    return rc;
}
