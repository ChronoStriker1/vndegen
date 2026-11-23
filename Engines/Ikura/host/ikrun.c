#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ikura.h"
#include "vnfont.h"
#include "vnikura.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);
static int press(VNIkura *n, int x, int y, int button);
static int run(VNIkura *n, int ms);

static FILE *wav;
static long wavFrames;
static float audio[2048];

static int step(VNIkura *n) {
    if (((IK *)n)->movie) usleep(16000);
    int alive = vnikura_frame(n, 16);
    vnikura_audio(n, audio, 705);
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

static int run(VNIkura *n, int ms) {
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

static int press(VNIkura *n, int x, int y, int button) {
    vnikura_mouse_move(n, x, y);
    int alive = run(n, 32);
    vnikura_mouse_button(n, button, 1);
    alive = alive && run(n, 48);
    vnikura_mouse_button(n, button, 0);
    return alive && run(n, 32);
}

static int prefer[256];
static int preferCount, preferUsed;

static void print_state(IK *t) {
    printf("script=%s pc=%x wait=%d sp=%d hover=%d tags=%d cursor=%d shown=%d/%d text=", t->sc ? t->sc->name : "-", t->pc, t->wait, t->sp, t->hover,
           t->msg.tags != NULL, t->msg.cursor, t->msg.revealed, t->msg.count);
    for (int i = 0; i < t->msg.revealed; i++) putchar(t->msg.chars[i].cp < 0x80 ? (int)t->msg.chars[i].cp : '?');
    putchar('\n');
}

static int choice_button(IK *t, int pick) {
    int seen = 0;
    for (int k = 0; k < IK_BUTTONS; k++) {
        Button *b = &t->buttons[k];
        if (!b->made || !b->shown || !b->enabled || !b->bound || b->layer < 0 || b->layer >= t->layerCount) continue;
        if (!layer_drawn(t, b->layer) || b->layer == t->msg.layer) continue;
        if (++seen == pick) return k;
    }
    return -1;
}

static struct {
    const void *sc;
    u32 pc;
    int visits;
} seen[512];
static int seenCount;

static int rotate(IK *t) {
    int total = 0;
    while (choice_button(t, total + 1) >= 0) total++;
    if (!total) return 1;
    for (int i = 0; i < seenCount; i++)
        if (seen[i].sc == t->sc && seen[i].pc == t->pc) return 1 + seen[i].visits++ % total;
    if (seenCount < 512) seen[seenCount++] = (typeof(seen[0])){t->sc, t->pc, 1};
    return 1;
}

static int autoplay(VNIkura *n, int steps, int ms, int every, const char *out) {
    IK *t = (IK *)n;
    int alive = 1;
    for (int j = 0; j < steps && alive; j++) {
        if (t->wait == W_INPUT) {
            int pick = preferUsed < preferCount ? prefer[preferUsed++] : rotate(t);
            int k = choice_button(t, pick);
            if (k < 0) k = choice_button(t, 1);
            if (k >= 0) {
                Button *b = &t->buttons[k];
                Layer *l = &t->layers[b->layer];
                printf("choice %d -> button %d\n", pick, k);
                alive = press(n, l->x + b->x + b->w / 2, l->y + b->y + b->h / 2, VNIKURA_MOUSE_LEFT) && run(n, ms);
            } else alive = run(n, ms);
        } else alive = press(n, 400, 200, VNIKURA_MOUSE_LEFT) && run(n, ms);
        if (every > 0 && j % every == 0) {
            int w, h;
            const uint32_t *px = vnikura_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/auto%05d.png", out, j);
            write_png(path, px, w, h);
            print_state(t);
        }
    }
    return alive;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: ikrun <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = {IK_FONTS_DIR};
    char table[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", IK_FONTS_DIR);
    vnfont_init(table, dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/ikrun.log", argv[3]);
    VNIkuraConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100, .startTime = 1700000000};
    char err[512];
    VNIkura *n = vnikura_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    if (getenv("IKRUN_WAV")) {
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
        else if (sscanf(c, "click:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNIKURA_MOUSE_LEFT);
        else if (sscanf(c, "rclick:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNIKURA_MOUSE_RIGHT);
        else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vnikura_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnikura_key(n, k, 1);
            alive = run(n, 48);
            vnikura_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "wheel:%d", &k) == 1) {
            vnikura_wheel(n, k);
            alive = run(n, 32);
        } else if (sscanf(c, "prefer:%d", &k) == 1) {
            if (preferCount < 256) prefer[preferCount++] = k;
        } else if (sscanf(c, "auto:%d,%d,%d", &k, &x, &y) == 3) alive = autoplay(n, k, x, y, argv[3]);
        else if (sscanf(c, "load:%d", &k) == 1) {
            uint8_t slot[4] = {(uint8_t)k, (uint8_t)(k >> 8), 0, 0};
            save_exec((IK *)n, 0x0a07, slot, 4);
            alive = run(n, 32);
        } else if (sscanf(c, "var:%d", &k) == 1) printf("var%d=%d\n", k, k < ((IK *)n)->varCount ? ((IK *)n)->vars[k] : 0);
        else if (!strcmp(c, "state")) print_state((IK *)n);
        else if (!strcmp(c, "layers")) {
            IK *t = (IK *)n;
            for (int i = 0; i < t->layerCount; i++) {
                Layer *l = &t->layers[i];
                if (l->made && l->shown) printf("layer %d at %d,%d %dx%d img %d argb %d ready %d\n", i, l->x, l->y, l->w, l->h, l->img, l->argb[0], l->ready);
            }
            for (int k = 0; k < IK_BUTTONS; k++) {
                Button *b = &t->buttons[k];
                if (b->made) printf("button %d layer %d at %d,%d %dx%d mode %d type %d img %d\n", k, b->layer, b->x, b->y, b->w, b->h, b->mode, b->type, b->state[0].img);
            }
        }
        else if (!strncmp(c, "shot:", 5)) {
            int w, h;
            const uint32_t *px = vnikura_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        } else fprintf(stderr, "unknown command %s\n", c);
    }
    if (!alive) printf("game ended: %s\n", vnikura_problem(n) ? vnikura_problem(n) : "normally");
    vnikura_close(n);
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
