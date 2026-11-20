#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pj.h"
#include "vnfont.h"

static void write_png(const char *path, const uint32_t *px, int w, int h, int scale);

static FILE *wav;
static long wavFrames;
static float audio[2048];

static int step(VNPJADV *n) {
    int alive = vnpjadv_frame(n, 16);
    vnpjadv_audio(n, audio, 705);
    if (wav) {
        int16_t s[1410];
        for (int i = 0; i < 1410; i++) {
            float v = audio[i] * 32767.0f;
            s[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        fwrite(s, sizeof s, 1, wav);
        wavFrames += 705;
    }
    return alive;
}

static int run(VNPJADV *n, int ms) {
    for (int t = 0; t < ms; t += 16)
        if (!step(n)) return 0;
    return 1;
}

static void wav_header(FILE *f, long frames) {
    uint32_t data = (uint32_t)(frames * 4);
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0,
                     0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a'};
    uint32_t riff = data + 36;
    memcpy(h + 4, &riff, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 40, f);
    fwrite(&data, 4, 1, f);
}

static int press(VNPJADV *n, int x, int y, int button) {
    vnpjadv_mouse_move(n, x, y);
    int alive = run(n, 48);
    vnpjadv_mouse_button(n, button, 1);
    alive = alive && run(n, 64);
    vnpjadv_mouse_button(n, button, 0);
    return alive && run(n, 32);
}

static int solve(VNPJADV *n, int maxMs) {
    PJ *p = (PJ *)n;
    int t = 0, alive = 1;
    while (alive && t < 10000 && !p->game) {
        alive = run(n, 100);
        t += 100;
    }
    while (alive && t < maxMs && p->game && pj_game_phase(p->game) < 2) {
        alive = run(n, 100);
        t += 100;
    }
    while (alive && t < maxMs && p->game && pj_game_phase(p->game) == 2) {
        int xs[256], ys[256], tools[256];
        int c = pj_game_icons(p->game, xs, ys, tools, 256);
        if (!c) {
            alive = run(n, 100);
            t += 100;
            continue;
        }
        int tool = pj_game_tool(p->game), k;
        for (k = 0; k < c && tools[k] != tool; k++) {}
        if (k == c) {
            alive = press(n, 180, 910, VNPJADV_MOUSE_LEFT);
            t += 150;
            continue;
        }
        vnpjadv_mouse_move(n, xs[k] - 60, ys[k]);
        alive = run(n, 16);
        vnpjadv_mouse_button(n, VNPJADV_MOUSE_LEFT, 1);
        for (int s = 0; s < 6 && alive; s++) {
            vnpjadv_mouse_move(n, xs[k] + (s % 2 ? -60 : 60), ys[k]);
            alive = run(n, 48);
        }
        vnpjadv_mouse_button(n, VNPJADV_MOUSE_LEFT, 0);
        alive = alive && run(n, 16);
        t += 340;
    }
    printf("solve: %d ms, phase %d\n", t, p->game ? pj_game_phase(p->game) : -1);
    return alive;
}

static void print_state(PJ *p) {
    printf("pc=%x wait=%d sp=%d game=%d text=%s\n", p->pc, p->wait, p->sp, p->game != NULL, p->msgText ? p->msgText : "");
    for (int i = 0; i < PJ_LAYERS + PJ_SUBS; i++) {
        Layer *l = &p->layer[i];
        if (l->used)
            printf("  layer %x %s z=%u shown=%d at %d,%d+%d,%d a=%d src=%d,%d %dx%d\n", l->id, l->name, l->z, l->shown, l->x, l->y, l->ox, l->oy,
                   l->alpha, l->sx, l->sy, l->sw, l->sh);
    }
    for (int i = 0; i < PJ_BUTTONS; i++) {
        Button *b = &p->btn[i];
        if (b->used) printf("  button g%d id%d %s %d,%d-%d,%d state=%d img=%dx%d src=%d,%d %dx%d at %d,%d\n", b->group, b->id, b->spr.name, b->x1, b->y1, b->x2, b->y2, b->spr.btnState, b->spr.img ? b->spr.img->w : 0, b->spr.img ? b->spr.img->h : 0, b->spr.sx, b->spr.sy, b->spr.sw, b->spr.sh, b->spr.x, b->spr.y);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pjadvrun gameDir [-s saveDir] [-l lang] [-w wav] [-o log] [commands]\n"
                        "commands: w:MS  S[:MS]  c:X,Y  r:X,Y  m:X,Y  d:X,Y  u:X,Y  k:VK  p:FILE.png  P:FILE.png  t  q\n");
        return 2;
    }
    const char *dirs[] = {PJ_FONTS_DIR};
    vnfont_init(PJ_FONTS_DIR "/fonts.tsv", dirs, 1);
    VNPJADVConfig cfg = {.gameDir = argv[1], .audioRate = 44100, .logPath = "/dev/stderr", .language = 1, .seed = 12345};
    int i = 2;
    for (; i + 1 < argc && argv[i][0] == '-'; i += 2) {
        if (!strcmp(argv[i], "-s")) cfg.saveDir = argv[i + 1];
        else if (!strcmp(argv[i], "-l")) cfg.language = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "-o")) cfg.logPath = argv[i + 1];
        else if (!strcmp(argv[i], "-w")) {
            wav = fopen(argv[i + 1], "wb");
            if (wav) fseek(wav, 44, SEEK_SET);
        }
    }
    char err[512];
    VNPJADV *n = vnpjadv_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    int alive = 1;
    for (; i < argc && alive; i++) {
        const char *c = argv[i];
        int x = 0, y = 0;
        if (c[1] == ':') sscanf(c + 2, "%d,%d", &x, &y);
        switch (c[0]) {
        case 'w': alive = run(n, x); break;
        case 'c': alive = press(n, x, y, VNPJADV_MOUSE_LEFT); break;
        case 'r': alive = press(n, x, y, VNPJADV_MOUSE_RIGHT); break;
        case 'm':
            vnpjadv_mouse_move(n, x, y);
            alive = run(n, 16);
            break;
        case 'd':
            vnpjadv_mouse_move(n, x, y);
            vnpjadv_mouse_button(n, VNPJADV_MOUSE_LEFT, 1);
            alive = run(n, 16);
            break;
        case 'u':
            vnpjadv_mouse_move(n, x, y);
            vnpjadv_mouse_button(n, VNPJADV_MOUSE_LEFT, 0);
            alive = run(n, 16);
            break;
        case 'k':
            vnpjadv_key(n, x, 1);
            alive = run(n, 32);
            vnpjadv_key(n, x, 0);
            break;
        case 'p':
        case 'P': {
            int w, h;
            const uint32_t *px = vnpjadv_pixels(n, &w, &h);
            write_png(c + 2, px, w, h, c[0] == 'p' ? 2 : 1);
            break;
        }
        case 't': print_state((PJ *)n); break;
        case 'S': alive = solve(n, x > 0 ? x : 120000); break;
        case 'q': alive = 0; break;
        }
    }
    print_state((PJ *)n);
    if (vnpjadv_problem(n)) fprintf(stderr, "problem: %s\n", vnpjadv_problem(n));
    vnpjadv_request_quit(n);
    vnpjadv_close(n);
    if (wav) {
        wav_header(wav, wavFrames);
        fclose(wav);
    }
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

static void write_png(const char *path, const uint32_t *src, int sw, int sh, int scale) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int w = sw / scale, h = sh / scale;
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
            uint32_t p = src[(size_t)y * scale * sw + (size_t)x * scale];
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
