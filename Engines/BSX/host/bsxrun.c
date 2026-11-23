#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsx.h"
#include "vnbsx.h"
#include "vnfont.h"

static void write_png(const char *path, const uint32_t *px, int w, int h);

static FILE *wav;
static long wavFrames;
static float audio[2048];
static int prefer[64];
static int preferCount, preferUsed;

static int step(VNBSX *n) {
    int alive = vnbsx_frame(n, 16);
    vnbsx_audio(n, audio, 705);
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

static int run(VNBSX *n, int ms) {
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

static int press(VNBSX *n, int x, int y, int button) {
    vnbsx_mouse_move(n, x, y);
    int alive = run(n, 32);
    vnbsx_mouse_button(n, button, 1);
    alive = alive && run(n, 48);
    vnbsx_mouse_button(n, button, 0);
    return alive && run(n, 32);
}

static const char *label_at(BX *b, u32 pc) {
    const char *best = "-";
    u32 bestOff = 0;
    for (int i = 0; i < b->sc->labelCount; i++)
        if (b->sc->labels[i] <= pc && b->sc->labels[i] >= bestOff) {
            bestOff = b->sc->labels[i];
            best = b->sc->labelNames[i];
        }
    return best;
}

static void print_state(BX *b) {
    char objs[512] = "";
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &b->shown.obj[i];
        if (!o->used) continue;
        char one[96];
        snprintf(one, sizeof one, "%s%d:%s%s%s@%d,%d", objs[0] ? " " : "", o->kind, o->name, o->face[0] ? "+" : "", o->face, o->x, o->y);
        strncat(objs, one, sizeof objs - strlen(objs) - 1);
    }
    printf("label=%s pc=%x wait=%d phase=%d choices=%d bgm=%s objs=[%s] cutin=%d voice=%s name=%s text=%s\n", label_at(b, b->th.pc), b->th.pc,
           b->th.wait, b->phase, b->choiceCount, b->bgmName, objs, b->cutIn, b->voice ? b->voice : "", b->speaker ? b->speaker : "", b->text ? b->text : "");
}

static void print_map(BX *b) {
    int hit[16] = {0};
    static const int pts[][2] = {{408, 488}, {424, 456}, {440, 392}, {472, 320}, {256, 224}, {248, 176}, {240, 128}, {112, 32}, {688, 16}, {744, 216}};
    for (int i = 0; i < 10; i++) {
        vnbsx_mouse_move((VNBSX *)b, pts[i][0], pts[i][1]);
        bx_map_update(b);
        hit[i] = bx_map_hover(b);
    }
    printf("map hover ids at place points:");
    for (int i = 0; i < 10; i++) printf(" %d", hit[i]);
    printf("\n");
}

static int choice_xy(BX *b, int pick, int *x) {
    Choice *c = &b->choice[pick - 1 < 0 ? 0 : pick - 1 >= b->choiceCount ? b->choiceCount - 1 : pick - 1];
    *x = c->x + c->w / 2;
    return c->y + c->h / 2;
}

static int answer(VNBSX *n) {
    BX *b = (BX *)n;
    int pick = preferUsed < preferCount ? prefer[preferUsed++] : 1, x;
    int y = choice_xy(b, pick, &x);
    return press(n, x, y, VNBSX_MOUSE_LEFT);
}

static int cardPick;

static int card_rank(const char *name) {
    if (strncmp(name, "icon_ch", 7)) return 0;
    if (strstr(name, "_EV") || strstr(name, "_EX") || strstr(name, "_SP")) return 2;
    return 1;
}

static int card_xy(BX *b, int *x, int *y) {
    Obj *best[BX_OBJS];
    int n = 0, rank = -1;
    for (int i = 0; i < BX_OBJS; i++) {
        Obj *o = &b->shown.obj[i];
        if (!o->used || o->kind != OK_OBJ || !bx_image_load(b, o->name)) continue;
        int r = card_rank(o->name);
        if (r > rank) {
            rank = r;
            n = 0;
        }
        if (r == rank) best[n++] = o;
    }
    if (!n) return 0;
    Obj *o = best[cardPick++ % n];
    Image *im = bx_image_load(b, o->name);
    *x = im->x + o->x + o->kx + im->w / 2;
    *y = im->y + o->y + o->ky + im->h / 4;
    return 1;
}

static int autoplay(VNBSX *n, int steps, int pace) {
    BX *b = (BX *)n;
    int alive = 1;
    int nt[2] = {448, 528};
    bx_cfg_ints(&b->cfg, "map phase", "next time", nt, 2);
    const char *v = bx_cfg_get(&b->cfg, "map phase", "next time");
    char f[64];
    if (v && bx_cfg_field(v, 2, f, sizeof f) > 0) nt[0] = bx_eval(f, NULL);
    if (v && bx_cfg_field(v, 3, f, sizeof f) > 0) nt[1] = bx_eval(f, NULL);
    for (int j = 0; j < steps && alive; j++) {
        int x, y;
        if (b->phase == PH_MAP) alive = press(n, nt[0] + 52, nt[1] + 32, VNBSX_MOUSE_LEFT);
        else if (b->phase == PH_CUSTOM_MAP) alive = card_xy(b, &x, &y) ? press(n, x, y, VNBSX_MOUSE_LEFT) : run(n, 100);
        else if (b->phase == PH_TITLE) break;
        else if (b->th.wait == W_CHOICE) alive = answer(n);
        else if (b->th.wait == W_TEXT || b->th.wait == W_CLICK) alive = press(n, 400, 200, VNBSX_MOUSE_LEFT) && run(n, pace);
        else alive = run(n, 50);
    }
    return alive;
}

static int skipplay(VNBSX *n, int steps, int pickIcons) {
    BX *b = (BX *)n;
    int alive = 1;
    int nt[2] = {448, 528};
    const char *v = bx_cfg_get(&b->cfg, "map phase", "next time");
    char f[64];
    if (v && bx_cfg_field(v, 2, f, sizeof f) > 0) nt[0] = bx_eval(f, NULL);
    if (v && bx_cfg_field(v, 3, f, sizeof f) > 0) nt[1] = bx_eval(f, NULL);
    b->set.skipUnread = 1;
    const char *last = "";
    for (int j = 0; j < steps && alive; j++) {
        int x, y;
        b->skipMode = true;
        const char *l = label_at(b, b->th.pc);
        if (strcmp(l, last)) {
            printf("step %d label %s\n", j, l);
            last = l;
        }
        if (b->phase == PH_MAP) {
            if (pickIcons && bx_map_choose(b, j % 3)) alive = run(n, 16);
            else alive = press(n, nt[0] + 52, nt[1] + 32, VNBSX_MOUSE_LEFT);
        } else if (b->phase == PH_CUSTOM_MAP) alive = card_xy(b, &x, &y) ? press(n, x, y, VNBSX_MOUSE_LEFT) : run(n, 100);
        else if (b->phase == PH_TITLE) {
            printf("back at the title after %d steps\n", j);
            break;
        } else if (b->th.wait == W_CHOICE) alive = answer(n);
        else alive = run(n, 16);
    }
    return alive;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: bsxrun <game> <saves> <out> [commands]\n");
        return 2;
    }
    const char *dirs[] = {BX_FONTS_DIR};
    char table[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", BX_FONTS_DIR);
    vnfont_init(table, dirs, 1);
    char log[1024];
    snprintf(log, sizeof log, "%s/bsxrun.log", argv[3]);
    VNBSXConfig cfg = {.gameDir = argv[1], .saveDir = argv[2], .logPath = log, .audioRate = 44100, .startTime = 1700000000};
    char err[512];
    VNBSX *n = vnbsx_open(&cfg, err, sizeof err);
    if (!n) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    BX *b = (BX *)n;
    if (getenv("BSXRUN_WAV")) {
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
        else if (sscanf(c, "click:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNBSX_MOUSE_LEFT);
        else if (sscanf(c, "rclick:%d,%d", &x, &y) == 2) alive = press(n, x, y, VNBSX_MOUSE_RIGHT);
        else if (sscanf(c, "move:%d,%d", &x, &y) == 2) {
            vnbsx_mouse_move(n, x, y);
            alive = run(n, 32);
        } else if (sscanf(c, "key:%i", &k) == 1) {
            vnbsx_key(n, k, 1);
            alive = run(n, 48);
            vnbsx_key(n, k, 0);
            alive = alive && run(n, 32);
        } else if (sscanf(c, "wheel:%d", &k) == 1) {
            vnbsx_wheel(n, k);
            alive = run(n, 32);
        } else if (sscanf(c, "prefer:%d", &k) == 1) {
            if (preferCount < 64) prefer[preferCount++] = k;
        } else if (sscanf(c, "auto:%d,%d", &k, &x) == 2) {
            for (int j = 0; j < k && alive; j++) {
                if (b->th.wait == W_CHOICE) {
                    alive = answer(n);
                    continue;
                }
                alive = press(n, 400, 200, VNBSX_MOUSE_LEFT) && run(n, x);
            }
        } else if (sscanf(c, "save:%d", &k) == 1) {
            bx_save_capture_thumb(b);
            printf("save %d: %s\n", k, bx_save_slot(b, k) ? "ok" : "failed");
        } else if (sscanf(c, "load:%d", &k) == 1) {
            printf("load %d: %s\n", k, bx_load_slot(b, k) ? "ok" : "failed");
            alive = run(n, 100);
        } else if (sscanf(c, "skipplay:%d,%d", &k, &x) == 2) {
            alive = skipplay(n, k, x);
        } else if (sscanf(c, "skipplay:%d", &k) == 1) {
            alive = skipplay(n, k, 0);
        } else if (sscanf(c, "autoplay:%d,%d", &k, &x) == 2) {
            alive = autoplay(n, k, x);
        } else if (sscanf(c, "untilmsg:%d", &k) == 1) {
            for (int j = 0; j < 20000 && alive && !(b->th.wait == W_TEXT && b->logCount && b->text && b->sc->msgs[k] && strstr(b->logs[b->logCount - 1].text, b->sc->msgs[k]) == b->logs[b->logCount - 1].text); j++) {
                if (b->th.wait == W_CHOICE) alive = answer(n);
                else if (b->th.wait == W_TEXT || b->th.wait == W_CLICK) alive = press(n, 400, 200, VNBSX_MOUSE_LEFT);
                else alive = run(n, 16);
            }
        } else if (sscanf(c, "untilwait:%d", &k) == 1) {
            for (int j = 0; j < 2000 && alive && b->th.wait != k; j++) alive = run(n, 16);
        } else if (!strncmp(c, "label:", 6)) {
            int l = bx_script_label(b->sc, c + 6);
            if (l < 0) fprintf(stderr, "no label %s\n", c + 6);
            else {
                bx_phase_end(b);
                b->th.sp = 0;
                b->th.wait = W_NONE;
                bx_vm_goto(b, l);
            }
        } else if (!strncmp(c, "var:", 4)) {
            int v = bx_var_by_name(b, c + 4);
            printf("%s=%d\n", c + 4, v >= 0 ? b->globals[v].v : 0);
        } else if (!strncmp(c, "set:", 4)) {
            char name[128];
            int val;
            if (sscanf(c + 4, "%127[^=]=%d", name, &val) == 2) {
                int v = bx_var_by_name(b, name);
                if (v >= 0) b->globals[v] = (Val){0, val};
            }
        } else if (!strncmp(c, "pc:", 3)) {
            bx_phase_end(b);
            b->th.sp = 0;
            b->th.wait = W_NONE;
            b->th.pc = (u32)strtoul(c + 3, NULL, 16);
        } else if (!strncmp(c, "system:", 7)) {
            bx_system(b, c + 7);
            alive = run(n, 16);
        } else if (!strcmp(c, "museum")) {
            bx_phase_end(b);
            bx_museum_open(b);
            alive = run(n, 100);
        } else if (!strcmp(c, "state")) print_state(b);
        else if (!strcmp(c, "mapstate")) print_map(b);
        else if (!strcmp(c, "skip")) b->skipMode = true;
        else if (!strcmp(c, "noskip")) b->skipMode = false;
        else if (sscanf(c, "play:%d", &k) == 1) {
            for (int j = 0; j < k && alive; j++) {
                if (b->th.wait == W_CHOICE) alive = answer(n);
                else alive = run(n, 16);
            }
        } else if (!strncmp(c, "shot:", 5)) {
            int w, h;
            const uint32_t *px = vnbsx_pixels(n, &w, &h);
            char path[1024];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], c + 5);
            write_png(path, px, w, h);
            printf("wrote %s\n", path);
        } else fprintf(stderr, "unknown command %s\n", c);
    }
    if (!alive) printf("game ended: %s\n", vnbsx_problem(n) ? vnbsx_problem(n) : "normally");
    vnbsx_close(n);
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
