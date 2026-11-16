// Runs an N2System game headless with scripted input and writes frames as PNG files.
//
//   n2run <game dir> <save dir> <out dir> [commands...]
//
// Commands run in order: "wait:MS" runs the game for MS milliseconds, "click:X,Y" clicks the left button there,
// "rclick:X,Y" the right one, "key:VK" presses a Windows virtual key (hex with 0x), "shot:NAME" writes NAME.png,
// "move:X,Y" moves the pointer, "wheel:N" turns the wheel, "clicks:N,MS" clicks at the pointer N times MS apart.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "n2.h"
#include "vnfont.h"
#include "vnn2.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);

static FILE *wav;
static long wavFrames;
static float audio[2048];

static int step(VNN2 *n) {
    if (n->movie) usleep(16000);
    int alive = vnn2_frame(n, 16);
    vnn2_audio(n, audio, 705);
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

static int run(VNN2 *n, int ms) {
    for (int t = 0; t < ms; t += 16)
        if (!step(n)) return 0;
    return 1;
}

static void dump(FILE *f, Obj *o, int depth) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        fprintf(f, "%*s%s kind=%d prio=%d xy=%.0f,%.0f wh=%d,%d op=%.0f img=%s ms=%d%s\n", depth * 2, "", c->name, c->kind,
                c->prio, c->x, c->y, c->w, c->h, c->opacity, c->image ? "y" : "n", c->mouseState,
                c->disused ? " disused" : "");
        dump(f, c, depth + 1);
    }
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

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: n2run <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = {N2_FONTS_DIR};
    char table[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", N2_FONTS_DIR);
    vnfont_init(table, dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/n2run.log", argv[3]);
    VNN2Config cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100,
                      .startTime = 1700000000};
    char err[512];
    VNN2 *n = vnn2_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    char wavPath[1024];
    snprintf(wavPath, sizeof wavPath, "%s/audio.wav", argv[3]);
    wav = fopen(wavPath, "wb");
    if (wav) wav_header(wav, 0);
    int alive = 1;
    for (int i = 4; i < argc && alive; i++) {
        const char *c = argv[i];
        int x, y, k;
        if (sscanf(c, "wait:%d", &k) == 1) {
            for (int t = 0; t < k && alive; t += 16) alive = step(n);
        } else if (sscanf(c, "click:%d,%d", &x, &y) == 2) {
            vnn2_mouse_move(n, x, y);
            alive = run(n, 32);
            vnn2_mouse_button(n, VNN2_MOUSE_LEFT, 1);
            alive = alive && run(n, 48);
            vnn2_mouse_button(n, VNN2_MOUSE_LEFT, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "tap:%d,%d", &x, &y) == 2) {
            vnn2_mouse_move(n, x, y);
            vnn2_mouse_button(n, VNN2_MOUSE_LEFT, 1);
            vnn2_mouse_button(n, VNN2_MOUSE_LEFT, 0);
            alive = run(n, 64);
        } else if (sscanf(c, "rclick:%d,%d", &x, &y) == 2) {
            vnn2_mouse_move(n, x, y);
            vnn2_mouse_button(n, VNN2_MOUSE_RIGHT, 1);
            alive = run(n, 48);
            vnn2_mouse_button(n, VNN2_MOUSE_RIGHT, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vnn2_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "clicks:%d,%d", &k, &x) == 2) {
            for (int j = 0; j < k && alive; j++) {
                vnn2_mouse_button(n, VNN2_MOUSE_LEFT, 1);
                alive = run(n, 32);
                vnn2_mouse_button(n, VNN2_MOUSE_LEFT, 0);
                alive = alive && run(n, x);
            }
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnn2_key(n, k, 1);
            alive = run(n, 48);
            vnn2_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "wheel:%d", &k) == 1) {
            vnn2_wheel(n, k);
            alive = run(n, 32);
        } else if (strncmp(c, "set:", 4) == 0 && strchr(c, '=')) {
            const char *eq = strchr(c, '=');
            char name[256];
            snprintf(name, sizeof name, "%.*s", (int)(eq - c - 4), c + 4);
            Value v = v_num(atof(eq + 1));
            var_set(n, name, &v);
        } else if (strncmp(c, "print:", 6) == 0) {
            Value *v = var_get(n, c + 6);
            char tmp[256];
            printf("%s = %s\n", c + 6, v ? v_tostr(v, tmp, sizeof tmp) : "(unset)");
        } else if (strncmp(c, "tree:", 5) == 0) {
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.txt", argv[3], c + 5);
            FILE *f = fopen(path, "w");
            if (f) {
                dump(f, &n->root, 0);
                fprintf(f, "-- system\n");
                dump(f, &n->sysRoot, 0);
                fclose(f);
            }
        } else if (strncmp(c, "shot:", 5) == 0) {
            int w, h;
            const uint32_t *px = vnn2_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        }
    }
    if (!alive) printf("game ended: %s\n", vnn2_problem(n) ? vnn2_problem(n) : "normally");
    vnn2_close(n);
    if (wav) {
        wav_header(wav, wavFrames);
        fclose(wav);
    }
    return 0;
}

// A minimal PNG writer: stored (uncompressed) deflate blocks inside zlib, with CRC and Adler checksums.
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
static void be32(unsigned char *p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
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
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
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
    z[zn++] = 0x78; z[zn++] = 0x01;
    unsigned a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t len = raw - off < 65535 ? raw - off : 65535;
        z[zn++] = off + len >= raw;
        z[zn++] = len & 0xff; z[zn++] = len >> 8; z[zn++] = ~len & 0xff; z[zn++] = (~len >> 8) & 0xff;
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
