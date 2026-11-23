#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "png.h"
#include "sa.h"

static void h_text(void *c, const char *t, bool nl) { printf("  %s\n", t); }
static void h_speaker(void *c, const char *n, const char *v, const char *f) { printf("[%s %s %s]\n", n, v, f); }
static void h_page(void *c) { printf("  --\n"); }
static void h_log(void *c, const char *m) { printf("!! %s\n", m); }
static void h_command(void *c, const u8 *code, const Insn *in, VM *vm) {
    char *t = sa_insn_text(code, in);
    printf("cmd %s\n", t + 8);
    free(t);
}

static int usage(void) {
    fprintf(stderr, "satool <gameDir> list | extract <name> <out> | box <name.box> <outDir> | dis <name.box> <entry|*> | run <name.box> <entry> [choices] | agf <name> <out.png>\n       satool codec <save.dat ...>\n");
    return 2;
}

static int codec(int count, char **files) {
    int bad = 0;
    for (int i = 0; i < count; i++) {
        size_t n, rawLen, packedLen;
        u8 *d = sa_read_file(files[i], &n);
        u8 *raw = d ? vnsystemaoi_unpack(d, n, &rawLen) : NULL;
        u8 *packed = raw ? vnsystemaoi_pack(raw, rawLen, &packedLen) : NULL;
        bool same = packed && packedLen == n && !memcmp(packed, d, n);
        printf("%s: %s, unpacked %zu, repacked %zu of %zu, %s\n", files[i], raw ? "ok" : "unpack failed", raw ? rawLen : 0,
               packed ? packedLen : 0, n, same ? "identical" : "different");
        bad += !same;
        free(d);
        free(raw);
        free(packed);
    }
    return bad != 0;
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "codec")) return codec(argc - 2, argv + 2);
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
    } else if (!strcmp(cmd, "dis") && argc == 5) {
        size_t n;
        u8 *d = sa_vfs_read(&v, argv[3], &n);
        Box b;
        if (!d || !sa_box_parse(&b, d, n)) {
            rc = 1;
        } else {
            for (int i = 0; i < b.count; i++) {
                if (strcmp(argv[4], "*") && strcmp(argv[4], b.e[i].name)) continue;
                printf("# %s\n", b.e[i].name);
                Insn in;
                for (u32 at = 0; sa_insn_decode(b.e[i].data, b.e[i].size, at, &in); at = in.next) {
                    char *t = sa_insn_text(b.e[i].data, &in);
                    puts(t);
                    free(t);
                }
            }
            sa_box_free(&b);
        }
        free(d);
    } else if (!strcmp(cmd, "run") && argc >= 5) {
        static VM vm;
        VMHost h = {.text = h_text, .speaker = h_speaker, .page = h_page, .command = h_command, .log = h_log};
        const char *choices = argc > 5 ? argv[5] : "";
        int ci = 0;
        if (!sa_vm_start(&vm, &v, argv[3], atoi(argv[4]), &h)) {
            rc = 1;
        } else {
            for (int steps = 0; steps < 100000; steps++) {
                int st = sa_vm_run(&vm, &v, &h, 1000);
                if (st == VM_WAIT_CLICK) sa_vm_click(&vm);
                else if (st == VM_WAIT_CHOICE) {
                    int pick = choices[ci] ? choices[ci++] - '0' : 0;
                    for (int i = 0; i < vm.choiceCount; i++) printf("  %c%d %s\n", i == pick ? '>' : ' ', i, vm.choice[i].label);
                    sa_vm_choose(&vm, pick);
                } else if (st == VM_DONE) break;
            }
            sa_vm_free(&vm);
        }
    } else if (!strcmp(cmd, "pvm") && argc == 4) {
        size_t n;
        u8 *d = sa_vfs_read(&v, argv[3], &n);
        Pvm m;
        if (!d || !sa_pvm_parse(&m, d, n)) {
            rc = 1;
        } else {
            for (int i = 0; i < m.count; i++) {
                PvmLayer *l = &m.layer[i];
                int depth = 0;
                for (int p = l->parent; p >= 0; p = m.layer[p].parent) depth++;
                printf("%*s%s sw=%d tex=%s v=%d t=%d blend=%d%s\n", depth * 2, "", l->name, l->sw, l->texture, l->vcount, l->tcount, l->blend,
                       l->hasTranslate ? " translate" : "");
            }
            sa_pvm_free(&m);
        }
        free(d);
    } else if (!strcmp(cmd, "wav") && argc == 5) {
        Mixer *m = sa_mixer_new(44100, &v);
        sa_play(m, CH_VOICE, 0, argv[3], false);
        FILE *f = fopen(argv[4], "wb");
        static float buf[2048];
        long frames = 0;
        fseek(f, 44, SEEK_SET);
        while (sa_playing(m, CH_VOICE, 0) && frames < 44100L * 600) {
            sa_mixer_mix(m, buf, 1024, false);
            int16_t s16[2048];
            for (int i = 0; i < 2048; i++) s16[i] = (int16_t)(buf[i] * 32767.0f);
            fwrite(s16, sizeof s16, 1, f);
            frames += 1024;
        }
        uint32_t data = (uint32_t)(frames * 4), riff = data + 36;
        uint8_t hd[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0,
                          0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a'};
        memcpy(hd + 4, &riff, 4);
        memcpy(hd + 40, &data, 4);
        fseek(f, 0, SEEK_SET);
        fwrite(hd, 1, 44, f);
        fclose(f);
        sa_mixer_free(m);
        printf("%ld frames\n", frames);
    } else if (!strcmp(cmd, "table") && argc == 4) {
        Table t;
        if (!sa_table_load(&t, &v, argv[3])) {
            rc = 1;
        } else {
            for (int r = 0; r < t.rows; r++) {
                for (int c = 0; c < t.cols; c++) {
                    if (c) putchar('\t');
                    if (t.isText[c]) printf("\"%s\"", sa_table_text(&t, r, c));
                    else printf("%d", sa_table_int(&t, r, c));
                }
                putchar('\n');
            }
            sa_table_free(&t);
        }
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
