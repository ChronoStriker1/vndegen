#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "age.h"
#include "vnfont.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);

static float audio[2048];

static int step(VNAge *n) {
    int skip = 0;
    if (((AG *)n)->wait == W_MOVIE) {
        if (getenv("AG_MOVIES")) usleep(16000);
        else { vnage_key(n, 0x0d, 1); skip = 1; }
    }
    int alive = vnage_frame(n, 16);
    if (skip) vnage_key(n, 0x0d, 0);
    vnage_audio(n, audio, 705);
    return alive;
}

static int run(VNAge *n, int ms) {
    for (int t = 0; t < ms; t += 16)
        if (!step(n)) return 0;
    return 1;
}

static int press(VNAge *n, int x, int y, int button) {
    vnage_mouse_move(n, x, y);
    int alive = run(n, 32);
    vnage_mouse_button(n, button, 1);
    alive = alive && run(n, 48);
    vnage_mouse_button(n, button, 0);
    return alive && run(n, 32);
}

static void print_state(AG *g) {
    printf("now=%u steps=%llu wait=%d top=%d\n", g->now, (unsigned long long)g->steps, g->wait, g->top);
    for (int i = 0; i <= g->top; i++)
        printf("  frame %d %s pc=%#x rsp=%d\n", i, g->fr[i].sc ? g->fr[i].sc->name : "-", g->fr[i].pc, g->fr[i].rsp);
    for (int i = 0; i < g->gfx.nobj; i++) {
        Obj *o = g->gfx.obj[i];
        Image *im = gfx_surface(g, o->slot);
        printf("  obj %#x on=%d%d%d%d%d anim=%u vis=%d slot=%d(%dx%d) src=%d,%d-%d,%d pos=%.0f,%.0f mode=%d color=%08x live=%d\n", o->handle, o->cColor.on, o->cScale.on, o->cRot.on, o->cTrans.on, o->cCell.on, g->gfx.animDur, o->visible, o->slot, im ? im->w : 0, im ? im->h : 0, o->sl, o->st, o->sr, o->sb, o->pos.x, o->pos.y, o->mode, o->color, o->live);
    }
    printf("  frameNow=%u animDur=%u animStart=%u started=%d svc=%#x busy=%d\n", g->gfx.frameNow, g->gfx.animDur, g->gfx.animStart, g->gfx.animStarted, g->gfx.serviceFlags, gfx_busy(g));
    for (int i = 0; i < AG_CMDS; i++) if (g->gfx.cmd[i].on) printf("  cmd %d type %d key %d delay %d dur %d started %d start %u\n", i, g->gfx.cmd[i].type, g->gfx.cmd[i].key, g->gfx.cmd[i].delay, g->gfx.cmd[i].dur, g->gfx.cmd[i].started, g->gfx.cmd[i].start);
    for (int i = 0; i < g->gfx.nobj; i++) { Obj *o = g->gfx.obj[i]; if (o->cColor.on) printf("  busy %#x start=%u started=%d delay=%d dur=%d\n", o->handle, o->start, o->started, o->cColor.delay, o->cColor.dur); }
    movie_debug(g, stdout);
    int missing = 0;
    for (int op = 0; op < 0x400; op++)
        if (g->opMissing[op]) { printf("  missing %#x x%u\n", op, g->opMissing[op]); missing++; }
    printf("  %d missing ops\n", missing);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: agrun <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = { AG_FONTS_DIR };
    vnfont_init(AG_FONTS_DIR "/fonts.tsv", dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/agrun.log", argv[3]);
    VNAgeConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .cacheDir = argv[2], .logPath = log, .audioRate = 44100, .startTime = 1700000000};
    char err[512];
    VNAge *n = vnage_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    int alive = 1;
    char line[1024];
    FILE *in = NULL;
    for (int i = 4; alive; i++) {
        const char *c;
        if (i < argc && strcmp(argv[i], "-")) c = argv[i];
        else if (i < argc || in) {
            if (!in) { in = stdin; i--; }
            else i--;
            if (!fgets(line, sizeof line, in)) break;
            line[strcspn(line, "\r\n")] = 0;
            if (!line[0]) continue;
            c = line;
        } else break;
        int x, y, k;
        if (sscanf(c, "wait:%d", &k) == 1) alive = run(n, k);
        else if (sscanf(c, "pages:%d", &k) == 1) {
            AG *g = (AG *)n;
            int done = 0;
            for (int guard = 0; guard < k * 400 && alive && done < k; guard++) {
                if (g->wait == W_INPUT) {
                    alive = press(n, 20, 300, VNAGE_MOUSE_LEFT);
                    done++;
                } else alive = run(n, 16);
            }
            printf("pages %d done %d\n", k, done);
        } else if (sscanf(c, "poke:%x,%d", &x, &y) == 2) {
            AG *g = (AG *)n;
            if ((unsigned)x < g->ngi) g->gi[x] = y;
        } else if (sscanf(c, "peek:%x", &x) == 1) {
            AG *g = (AG *)n;
            printf("G[%#x] = %d\n", x, (unsigned)x < g->ngi ? g->gi[x] : 0);
        } else if (sscanf(c, "click:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNAGE_MOUSE_LEFT);
        else if (sscanf(c, "rclick:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNAGE_MOUSE_RIGHT);
        else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vnage_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnage_key(n, k, 1);
            alive = run(n, 48);
            vnage_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (!strcmp(c, "state")) print_state((AG *)n);
        else if (!strncmp(c, "shot:", 5)) {
            int w, h;
            const uint32_t *px = vnage_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        } else if (!strncmp(c, "shots:", 6)) {
            int every, count;
            if (sscanf(c + 6, "%d,%d", &every, &count) == 2)
                for (int s = 0; s < count && alive; s++) {
                    alive = run(n, every);
                    int w, h;
                    const uint32_t *px = vnage_pixels(n, &w, &h);
                    char path[1024];
                    snprintf(path, sizeof path, "%s/s%03d.png", argv[3], s);
                    write_png(path, px, w, h);
                }
        } else fprintf(stderr, "unknown command %s\n", c);
        if (in) { printf("ok %s\n", c); fflush(stdout); }
    }
    if (!alive) printf("game ended: %s\n", vnage_problem(n) ? vnage_problem(n) : "normally");
    print_state((AG *)n);
    vnage_close(n);
    return 0;
}

static unsigned crc_table[256];
static unsigned crc(const unsigned char *b, size_t n, unsigned c) {
    if (!crc_table[1])
        for (unsigned i = 0; i < 256; i++) {
            unsigned v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ b[i]) & 0xff] ^ (c >> 8);
    return c;
}
static void be32(unsigned char *p, unsigned v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n) {
    unsigned char h[8];
    be32(h, (unsigned)n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    unsigned c = crc((const unsigned char *)type, 4, 0xffffffffu);
    c = crc(data, n, c) ^ 0xffffffffu;
    be32(h, c);
    fwrite(h, 1, 4, f);
}
static void write_png(const char *path, const uint32_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    unsigned char ihdr[13];
    be32(ihdr, w);
    be32(ihdr + 4, h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t row = (size_t)w * 3 + 1, raw = row * h;
    unsigned char *r = malloc(raw);
    for (int y = 0; y < h; y++) {
        r[y * row] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t p = px[y * w + x];
            r[y * row + 1 + x * 3] = p & 0xff;
            r[y * row + 2 + x * 3] = p >> 8 & 0xff;
            r[y * row + 3 + x * 3] = p >> 16 & 0xff;
        }
    }
    size_t blocks = (raw + 65534) / 65535;
    unsigned char *z = malloc(raw + blocks * 5 + 6);
    size_t zn = 0;
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    unsigned a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) {
        a = (a + r[i]) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t len = raw - off < 65535 ? raw - off : 65535;
        z[zn++] = off + len >= raw;
        z[zn++] = len & 0xff;
        z[zn++] = len >> 8;
        z[zn++] = ~len & 0xff;
        z[zn++] = (~len >> 8) & 0xff;
        memcpy(z + zn, r + off, len);
        zn += len;
    }
    be32(z + zn, b << 16 | a);
    zn += 4;
    chunk(f, "IDAT", z, zn);
    chunk(f, "IEND", NULL, 0);
    free(z);
    free(r);
    fclose(f);
}
