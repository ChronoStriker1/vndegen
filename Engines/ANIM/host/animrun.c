#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anim.h"
#include "vnanim.h"
#include "vnfont.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);

static FILE *wav;
static long wavFrames;
static float audio[2048];
static int prefer[64];
static int preferCount, preferUsed;

static int step(VNANIM *n) {
    int alive = vnanim_frame(n, 16);
    vnanim_audio(n, audio, 705);
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

static int run(VNANIM *n, int ms) {
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

static int press(VNANIM *n, int x, int y) {
    vnanim_mouse_move(n, x, y);
    int alive = run(n, 32);
    vnanim_mouse_button(n, VNANIM_MOUSE_LEFT, 1);
    alive = alive && run(n, 48);
    vnanim_mouse_button(n, VNANIM_MOUSE_LEFT, 0);
    return alive && run(n, 32);
}

static void print_state(AN *a) {
    char layers[512] = "";
    for (int i = 0; i < AN_LAYERS; i++) {
        Layer *l = &a->scene.layer[i];
        if (l->kind == LK_NONE) continue;
        char one[128];
        snprintf(one, sizeof one, "%s%d:%s%s%s@%d,%d", layers[0] ? " " : "", i, l->name, l->motion[0] ? "+" : "", l->motion, l->toX, l->toY);
        strncat(layers, one, sizeof layers - strlen(layers) - 1);
    }
    printf("phase=%d label=%s pc=%x wait=%d bg=%s window=%d layers=[%s] name=%s text=%s\n", a->phase,
           an_script_label_at(a->sc, a->th.pc), a->th.pc, a->th.wait, a->scene.bg, a->scene.window, layers,
           a->name ? a->name : "", a->text ? a->text : "");
}

static int answer(VNANIM *n) {
    AN *a = (AN *)n;
    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1;
    if (pick < 1) pick = 1;
    if (pick > a->choiceCount) pick = a->choiceCount;
    Choice *c = &a->choice[pick - 1];
    printf("choice %d of %d: %s\n", pick, a->choiceCount, c->text);
    return press(n, c->x + c->w / 2, c->y + c->h / 2);
}

static int autoplay(VNANIM *n, int steps, int pace, int verbose) {
    AN *a = (AN *)n;
    int alive = 1, idle = 0;
    for (int j = 0; j < steps && alive; j++) {
        if (getenv("ANIMRUN_TRACE")) printf("step %d label=%s pc=%x wait=%d\n", j, an_script_label_at(a->sc, a->th.pc), a->th.pc, a->th.wait);
        if (a->phase == PH_TITLE) {
            printf("back at the title after %d steps\n", j);
            break;
        }
        if (a->th.wait == W_CHOICE) alive = answer(n);
        else if (a->th.wait == W_TEXT) {
            if (verbose) printf("%s%s%s\n", a->name ? a->name : "", a->name ? ": " : "", a->text);
            alive = press(n, 640, 300) && run(n, pace);
        } else {
            const char *l = an_script_label_at(a->sc, a->th.pc);
            if (!strcmp(l, "ClickWait") || (idle > 40 && a->th.wait != W_TIME && a->th.wait != W_TRANS)) {
                alive = press(n, 640, 300);
                idle = 0;
            } else {
                alive = run(n, 50);
                idle++;
            }
            continue;
        }
        idle = 0;
    }
    return alive;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: animrun <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = {AN_FONTS_DIR};
    char table[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", AN_FONTS_DIR);
    vnfont_init(table, dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/animrun.log", argv[3]);
    VNANIMConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100, .startTime = 1700000000};
    char err[512];
    VNANIM *n = vnanim_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    AN *a = (AN *)n;
    if (getenv("ANIMRUN_WAV")) {
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
        else if (sscanf(c, "click:%d,%d", &x, &y) == 2) alive = press(n, x, y);
        else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vnanim_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnanim_key(n, k, 1);
            alive = run(n, 48);
            vnanim_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "prefer:%d", &k) == 1) {
            if (preferCount < 64) prefer[preferCount++] = k;
        } else if (sscanf(c, "auto:%d,%d", &k, &x) == 2) alive = autoplay(n, k, x, 0);
        else if (sscanf(c, "say:%d,%d", &k, &x) == 2) alive = autoplay(n, k, x, 1);
        else if (!strncmp(c, "label:", 6)) {
            int l = an_script_label(a->sc, c + 6);
            if (l < 0) fprintf(stderr, "no label %s\n", c + 6);
            else an_vm_start(a, (u32)l);
        } else if (!strncmp(c, "pc:", 3)) an_vm_start(a, (u32)strtoul(c + 3, NULL, 16));
        else if (sscanf(c, "load:%d", &k) == 1) printf("load %d: %s\n", k, an_load_slot(a, k) ? "ok" : "failed");
        else if (sscanf(c, "save:%d", &k) == 1) {
            an_capture_shot(a);
            printf("save %d: %s\n", k, an_save_slot(a, k) ? "ok" : "failed");
        } else if (!strncmp(c, "var:", 4)) {
            char name[64] = "";
            float v;
            if (sscanf(c + 4, "%d,%63[^,],%f", &k, name, &v) == 3 && k >= 0 && k < AN_LAYERS) {
                an_layer_set_var(&a->scene.layer[k], name, v);
                a->scene.layer[k].varSerial++;
                a->scene.layer[k].fresh = true;
            }
        } else if (!strncmp(c, "emote:", 6)) {
            char model[64] = "", preset[64] = "";
            if (sscanf(c + 6, "%d,%63[^,],%63s", &k, model, preset) == 3 && k >= 0 && k < AN_LAYERS) {
                Layer *l = &a->scene.layer[k];
                memset(l, 0, sizeof *l);
                l->kind = LK_EMOTE;
                snprintf(l->name, sizeof l->name, "%s", model);
                l->alpha = l->fromAlpha = l->toAlpha = 255;
                l->scale = 1000;
                an_layer_apply_preset(a, l, "AllReset", 0);
                an_layer_apply_preset(a, l, preset, 0);
                l->fresh = true;
            }
        } else if (!strcmp(c, "state")) print_state(a);
        else if (!strcmp(c, "skip")) a->skipMode = true;
        else if (!strcmp(c, "noskip")) a->skipMode = false;
        else if (!strncmp(c, "shot:", 5)) {
            int w, h;
            const uint32_t *px = vnanim_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        } else fprintf(stderr, "unknown command %s\n", c);
    }
    if (!alive) printf("game ended: %s\n", vnanim_problem(n) ? vnanim_problem(n) : "normally");
    vnanim_close(n);
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
