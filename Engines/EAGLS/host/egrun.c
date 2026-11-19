#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "eagls.h"
#include "vnfont.h"
#include "vneagls.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);

static FILE *wav;
static long wavFrames;
static float audio[2048];
static int prefer[64];
static int preferCount, preferUsed;

static int step(VNEAGLS *n) {
    if (movie_any((EG *)n)) usleep(16000);
    int alive = vneagls_frame(n, 16);
    vneagls_audio(n, audio, 705);
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

static int run(VNEAGLS *n, int ms) {
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

static int press(VNEAGLS *n, int x, int y, int button) {
    vneagls_mouse_move(n, x, y);
    int alive = run(n, 32);
    vneagls_mouse_button(n, button, 1);
    alive = alive && run(n, 48);
    vneagls_mouse_button(n, button, 0);
    return alive && run(n, 32);
}

static void print_state(EG *t) {
    Thread *th = &t->th[0];
    char *text = eg_sjis_to_utf8(t->text ? t->text : "");
    char *name = eg_sjis_to_utf8(t->speaker ? t->speaker : "");
    printf("script=%s line=%d wait=%d ui=%d bg=%s choices=%d name=%s text=%s\n", th->sc ? th->sc->name : "-",
           th->sc && th->pc > 0 && th->pc <= th->sc->count ? th->sc->ins[th->pc - 1].line : -1, th->wait, t->uiMode,
           t->planeName[2], t->choiceCount, name, text);
    free(text);
    free(name);
}

static int choice_y(EG *t, int pick) {
    ChoiceBox *c = &t->choice[pick - 1 < 0 ? 0 : pick - 1];
    return c->y + c->h / 2;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: egrun <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = {EG_FONTS_DIR};
    char table[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", EG_FONTS_DIR);
    vnfont_init(table, dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/egrun.log", argv[3]);
    VNEAGLSConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100, .startTime = 1700000000};
    char err[512];
    VNEAGLS *n = vneagls_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    if (getenv("EGRUN_WAV")) {
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
        else if (sscanf(c, "click:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNEAGLS_MOUSE_LEFT);
        else if (sscanf(c, "rclick:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNEAGLS_MOUSE_RIGHT);
        else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vneagls_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vneagls_key(n, k, 1);
            alive = run(n, 48);
            vneagls_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "wheel:%d", &k) == 1) {
            vneagls_wheel(n, k);
            alive = run(n, 32);
        } else if (sscanf(c, "prefer:%d", &k) == 1) {
            if (preferCount < 64) prefer[preferCount++] = k;
        } else if (sscanf(c, "auto:%d,%d", &k, &x) == 2) {
            EG *t = (EG *)n;
            for (int j = 0; j < k && alive; j++) {
                if (t->th[0].wait == W_CHOICE) {
                    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1;
                    alive = press(n, 400, choice_y(t, pick), VNEAGLS_MOUSE_LEFT);
                    continue;
                }
                alive = press(n, 400, 300, VNEAGLS_MOUSE_LEFT) && run(n, x);
            }
        } else if (!strncmp(c, "goto:", 5)) {
            char script[128] = {0}, label[128] = {0};
            sscanf(c + 5, "%127[^,],%127s", script, label);
            char *s = eg_utf8_to_sjis(script), *l = eg_utf8_to_sjis(label);
            EG *t = (EG *)n;
            t->th[0].sp = 0;
            vm_goto(t, &t->th[0], s, l);
            t->th[0].wait = W_NONE;
            free(s);
            free(l);
        } else if (!strncmp(c, "var:", 4)) {
            EG *t = (EG *)n;
            Var *v = var_find(&t->vars, c + 4);
            Val *x = v ? var_at(v, 0) : NULL;
            printf("%s=%d %s\n", c + 4, x ? x->i : 0, x && x->s ? x->s : "");
        } else if (!strncmp(c, "set:", 4)) {
            EG *t = (EG *)n;
            char name[128];
            int val;
            if (sscanf(c + 4, "%127[^=]=%d", name, &val) == 2) var_at(var_get(&t->vars, name, true), 0)->i = val;
        }
        else if (sscanf(c, "untiltext:%d", &k) == 1) {
            EG *t = (EG *)n;
            for (int j = 0; j < 5000 && alive && !(t->th[0].wait == W_TEXT && t->textId == k); j++) {
                if (t->th[0].wait == W_CHOICE) {
                    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1;
                    alive = press(n, 400, choice_y(t, pick), VNEAGLS_MOUSE_LEFT);
                } else if (t->th[0].wait == W_TEXT) alive = press(n, 400, 300, VNEAGLS_MOUSE_LEFT);
                else alive = run(n, 16);
            }
        }
        else if (sscanf(c, "skipall:%d", &k) == 1) {
            EG *t = (EG *)n;
            const char *last = NULL;
            int stop = -1;
            sscanf(c, "skipall:%*d,%d", &stop);
            for (int j = 0; j < k && alive; j++) {
                if (stop >= 0 && t->th[0].wait == W_TEXT && t->textId == stop) {
                    var_at(var_get(&t->vars, ":SkipFlg", true), 0)->i = 0;
                    break;
                }
                var_at(var_get(&t->vars, ":SkipFlg", true), 0)->i = 1;
                if (t->th[0].sc && t->th[0].sc->name != last) {
                    last = t->th[0].sc->name;
                    printf("frame %d in %s\n", j, last);
                    fflush(stdout);
                }
                if (t->th[0].wait == W_CHOICE) {
                    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1;
                    alive = press(n, 400, choice_y(t, pick), VNEAGLS_MOUSE_LEFT);
                } else if (t->editing) {
                    vneagls_key(n, 0x0d, 1);
                    vneagls_key(n, 0x0d, 0);
                    alive = run(n, 16);
                } else if (j % 30 == 29) {
                    const char *sn = t->th[0].sc ? t->th[0].sc->name : "";
                    if (!strcmp(sn, "00Name.dat")) alive = press(n, 726, 529, VNEAGLS_MOUSE_LEFT);
                    else if (!strcmp(sn, "00NameOK.dat")) alive = press(n, 292, 298, VNEAGLS_MOUSE_LEFT);
                    else if (!strncmp(sn, "00", 2)) {
                        static const int pts[][2] = {{220, 110}, {350, 110}, {90, 250}, {220, 250}, {350, 250}, {480, 250},
                                                     {70, 390}, {200, 390}, {330, 390}, {460, 390}, {292, 298}, {542, 298}};
                        static int pi;
                        int *p = (int *)pts[pi++ % 12];
                        alive = press(n, p[0], p[1], VNEAGLS_MOUSE_LEFT);
                    } else alive = press(n, 400, 300, VNEAGLS_MOUSE_LEFT);
                } else alive = run(n, 16);
            }
        }
        else if (!strncmp(c, "movie:", 6)) {
            EG *t = (EG *)n;
            movie_open(t, 0, c + 6, 20);
            movie_play(t, 0, false);
            int j = 0;
            for (; j < 20000 && alive && movie_state(t, 0) != 2; j++) {
                alive = run(n, 16);
                if (j % 300 == 0) printf("movie frame %d state %d\n", j, movie_state(t, 0));
            }
            printf("movie done after %d frames\n", j);
            movie_stop(t, 0);
        }
        else if (!strcmp(c, "state")) print_state((EG *)n);
        else if (!strcmp(c, "skip")) var_at(var_get(&((EG *)n)->vars, ":SkipFlg", true), 0)->i = 1;
        else if (sscanf(c, "play:%d", &k) == 1) {
            EG *t = (EG *)n;
            for (int j = 0; j < k && alive; j++) {
                if (t->th[0].wait == W_CHOICE) {
                    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1;
                    alive = press(n, 400, choice_y(t, pick), VNEAGLS_MOUSE_LEFT);
                    continue;
                }
                alive = run(n, 16);
            }
        }
        else if (!strncmp(c, "shot:", 5)) {
            int w, h;
            const uint32_t *px = vneagls_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        } else fprintf(stderr, "unknown command %s\n", c);
    }
    if (!alive) printf("game ended: %s\n", vneagls_problem(n) ? vneagls_problem(n) : "normally");
    vneagls_close(n);
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
