#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipac.h"
#include "vnfont.h"
#include "vnipac.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);

static FILE *wav;
static long wavFrames;
static float audio[2048];
static int prefer[64];
static int preferCount, preferUsed;

static int step(VNIPAC *n) {
    int alive = vnipac_frame(n, 16);
    vnipac_audio(n, audio, 705);
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

static int run(VNIPAC *n, int ms) {
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

static int press(VNIPAC *n, int x, int y, int button) {
    vnipac_mouse_move(n, x, y);
    int alive = run(n, 32);
    vnipac_mouse_button(n, button, 1);
    alive = alive && run(n, 48);
    vnipac_mouse_button(n, button, 0);
    return alive && run(n, 32);
}

static void print_state(IP *t) {
    char *bg = ip_sjis_to_utf8(t->bgName);
    char *text = ip_sjis_to_utf8("");
    if (t->msgText) {
        free(text);
        size_t cap = (size_t)t->msgLen * 4 + 1, k = 0;
        text = malloc(cap);
        for (int i = 0; i < t->msgLen && k + 4 < cap; i++) {
            uint32_t c = t->msgText[i];
            if (c < 0x80) text[k++] = (char)c;
            else if (c < 0x800) {
                text[k++] = (char)(0xc0 | c >> 6);
                text[k++] = (char)(0x80 | (c & 0x3f));
            } else {
                text[k++] = (char)(0xe0 | c >> 12);
                text[k++] = (char)(0x80 | ((c >> 6) & 0x3f));
                text[k++] = (char)(0x80 | (c & 0x3f));
            }
        }
        text[k] = 0;
    }
    printf("script=%s line=%d wait=%d ui=%d bg=%s choices=%d text=%s\n", t->sc ? t->sc->name : "-",
           t->sc && t->pc < t->sc->count ? t->sc->st[t->pc].line : -1, t->wait, t->uiMode, bg, t->choiceCount, text);
    free(bg);
    free(text);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: ipacrun <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = {IP_FONTS_DIR};
    char table[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", IP_FONTS_DIR);
    vnfont_init(table, dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/ipacrun.log", argv[3]);
    VNIPACConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100, .startTime = 1700000000};
    char err[512];
    VNIPAC *n = vnipac_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    if (getenv("IPACRUN_WAV")) {
        char wavPath[1024];
        snprintf(wavPath, sizeof wavPath, "%s/audio.wav", argv[3]);
        wav = fopen(wavPath, "wb");
        if (wav) wav_header(wav, 0);
    }
    int alive = 1;
    for (int i = 4; i < argc && alive; i++) {
        const char *c = argv[i];
        int x, y, k;
        if (sscanf(c, "wait:%d", &k) == 1) alive = run(n, k);
        else if (sscanf(c, "click:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNIPAC_MOUSE_LEFT);
        else if (sscanf(c, "rclick:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNIPAC_MOUSE_RIGHT);
        else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vnipac_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnipac_key(n, k, 1);
            alive = run(n, 48);
            vnipac_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "wheel:%d", &k) == 1) {
            vnipac_wheel(n, k);
            alive = run(n, 32);
        } else if (sscanf(c, "prefer:%d", &k) == 1) {
            if (preferCount < 64) prefer[preferCount++] = k;
        } else if (sscanf(c, "auto:%d,%d", &k, &x) == 2) {
            IP *t = (IP *)n;
            for (int j = 0; j < k && alive; j++) {
                if (t->wait == W_CHOICE) {
                    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1;
                    t->choiceHover = pick - 1;
                    t->click = true;
                    alive = run(n, 32);
                    continue;
                }
                alive = press(n, 400, 300, VNIPAC_MOUSE_LEFT) && run(n, x);
            }
        } else if (!strncmp(c, "goto:", 5)) {
            char script[128] = {0}, label[128] = {0};
            sscanf(c + 5, "%127[^,],%127s", script, label);
            char *s = ip_utf8_to_sjis(script), *l = ip_utf8_to_sjis(label);
            vm_start((IP *)n, s, l[0] ? l : NULL);
            ((IP *)n)->wait = W_NONE;
            free(s);
            free(l);
        } else if (sscanf(c, "var:F%d", &k) == 1) printf("F%d=%d\n", k, ((IP *)n)->vf[k]);
        else if (sscanf(c, "var:W%d", &k) == 1) printf("W%d=%d\n", k, ((IP *)n)->vw[k]);
        else if (!strcmp(c, "state")) print_state((IP *)n);
        else if (!strncmp(c, "shot:", 5)) {
            int w, h;
            const uint32_t *px = vnipac_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        } else fprintf(stderr, "unknown command %s\n", c);
    }
    if (!alive) printf("game ended: %s\n", vnipac_problem(n) ? vnipac_problem(n) : "normally");
    vnipac_close(n);
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
