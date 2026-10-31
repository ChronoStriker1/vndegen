// Runs a System 3 game headless for testing: replays scripted input and writes frames as PPM files.
//   s3run <gameDir> [--ms N] [--save DIR] [--log FILE] [--final FILE.ppm] [--time UNIX]
//         [--auto MS] [--prefer TEXT]... [--avoid TEXT]... [--repl FIFO] [--wav FILE] [events...]
// Events, by time in ms: T:key:VK, T:click:X,Y, T:rclick:X,Y, T:move:X,Y, T:shot:FILE.ppm, T:pick:TEXT
// T:pick:TEXT waits from T until a menu offers an item containing TEXT, then chooses it; picks go in order, and the
// autopilot answers the menus in between.
// From --auto MS on, the autopilot presses Return at message waits, and once a second at other waits, and answers menus: a pending pick first, then the
// first --prefer text an item contains, then a seeded random item, never one containing an --avoid text.
// With --repl, after --ms it reads commands from FIFO, one per line, and prints "ok" after each:
//   run MS | click X Y | tap X Y | rclick X Y | key VK | shot FILE | where | trace 0/1 | var FROM [TO] | arr VAR COUNT | quit
// The game clock starts at --time, 2026-01-01 by default, so a run replays the same way.
// It prints the game's problem, if one stopped it, and exits 1 then.
#include "vns3.h"
#include "vnfont.h"
#include "s3.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Ev { int t; char kind[16]; int a, b; char file[512]; } Ev;

static void shot(VNS3 *g, const char *file) {
    int w, h;
    const uint32_t *px = vns3_pixels(g, &w, &h);
    FILE *f = fopen(file, "wb");
    if (!f || !px) { fprintf(stderr, "cannot write %s\n", file); if (f) fclose(f); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned char c[3] = { (unsigned char)(px[i] >> 16), (unsigned char)(px[i] >> 8), (unsigned char)px[i] };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
    printf("shot %s (%dx%d)\n", file, w, h);
}

static const char *prefer[32], *avoid[32];
static int preferCount, avoidCount;

static bool contains_any(const char *text, const char **list, int count) {
    for (int i = 0; i < count; i++) if (strstr(text, list[i])) return true;
    return false;
}

// Full-width ASCII and ideographic spaces as plain ASCII, in place, so menu text matches as typed.
static void narrow(char *u) {
    unsigned char *r = (unsigned char *)u, *w = r;
    while (*r) {
        if (r[0] == 0xef && r[1] == 0xbc && r[2] >= 0x81 && r[2] <= 0xbf) { *w++ = (unsigned char)(r[2] - 0x60); r += 3; }
        else if (r[0] == 0xef && r[1] == 0xbd && r[2] >= 0x80 && r[2] <= 0x9e) { *w++ = (unsigned char)(r[2] - 0x20); r += 3; }
        else if (r[0] == 0xe3 && r[1] == 0x80 && r[2] == 0x80) { *w++ = ' '; r += 3; }
        else *w++ = *r++;
    }
    *w = 0;
}

// Chooses an item of the open menu: the pick if one matches, else the autopilot's choice. Returns -1 for none.
static int choose(S3 *s, const char *pick, bool autopilot, int *turn, bool *picked) {
    char *items[S3_MAX_MENU];
    int count = s->menuCount, choice = -1;
    for (int i = 0; i < count; i++) { items[i] = s3_display_utf8(s, s->menu[i].text); narrow(items[i]); }
    if (pick) for (int i = 0; i < count && choice < 0; i++) if (strstr(items[i], pick)) choice = i;
    *picked = choice >= 0;
    if (choice < 0 && autopilot) {
        for (int p = 0; p < preferCount && choice < 0; p++)
            for (int i = 0; i < count && choice < 0; i++) if (strstr(items[i], prefer[p])) choice = i;
        // Otherwise a seeded random item, so the run replays the same way without cycling through two menus forever.
        *turn = *turn * 1103515245 + 12345;
        int start = (int)((unsigned)*turn >> 16) % count;
        for (int k = 0; k < count && choice < 0; k++) {
            int i = (start + k) % count;
            if (!contains_any(items[i], avoid, avoidCount)) choice = i;
        }
    }
    if (choice >= 0) {
        printf("%llu ms: menu", (unsigned long long)s->clockMs);
        for (int i = 0; i < count; i++) printf(" %s[%s]", i == choice ? "*" : "", items[i]);
        int cx, cy;
        s3_menu_item_center(s, choice, &cx, &cy);
        printf(" @%d,%d\n", cx, cy);
        for (int i = 0; i < count; i++) if (strstr(items[i], "\xef\xbf\xbd")) { printf("  raw %d zh %d:", i, s->menu[i].zh); for (const unsigned char *q = (const unsigned char *)s->menu[i].text; *q; q++) printf(" %02x", *q); printf("\n"); }
    }
    for (int i = 0; i < count; i++) free(items[i]);
    return choice;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: s3run gameDir [--ms N] [--save DIR] [--log FILE] [events]\n"); return 2; }
    const char *dirs[] = { S3_FONTS_DIR };
    vnfont_init(S3_FONTS_DIR "/fonts.tsv", dirs, 1);
    VNS3Config cfg = { .gameDir = argv[1], .audioRate = 44100, .soundFontPath = S3_SOUNDFONT, .startTime = 1767225600 };
    int total = 10000, autoFrom = -1;
    const char *final = NULL, *repl = NULL, *wavPath = NULL;
    static Ev ev[8192];
    int n = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--ms") && i + 1 < argc) total = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) cfg.saveDir = argv[++i];
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) cfg.logPath = argv[++i];
        else if (!strcmp(argv[i], "--final") && i + 1 < argc) final = argv[++i];
        else if (!strcmp(argv[i], "--time") && i + 1 < argc) cfg.startTime = atoll(argv[++i]);
        else if (!strcmp(argv[i], "--auto") && i + 1 < argc) autoFrom = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--repl") && i + 1 < argc) repl = argv[++i];
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wavPath = argv[++i];
        else if (!strcmp(argv[i], "--prefer") && i + 1 < argc && preferCount < 32) prefer[preferCount++] = argv[++i];
        else if (!strcmp(argv[i], "--avoid") && i + 1 < argc && avoidCount < 32) avoid[avoidCount++] = argv[++i];
        else if (n < 8192) {
            Ev *e = &ev[n];
            memset(e, 0, sizeof *e);
            char rest[600] = "";
            if (sscanf(argv[i], "%d:%15[^:]:%599[^\n]", &e->t, e->kind, rest) < 2) { fprintf(stderr, "bad event %s\n", argv[i]); return 2; }
            if (!strcmp(e->kind, "shot") || !strcmp(e->kind, "pick")) snprintf(e->file, sizeof e->file, "%s", rest);
            else sscanf(rest, "%i,%i", &e->a, &e->b);
            n++;
        }
    }
    char err[512] = "";
    VNS3 *g = vns3_open(&cfg, err, sizeof err);
    if (!g) { fprintf(stderr, "open failed: %s\n", err); return 1; }
    S3 *s = (S3 *)g;
    float audio[1470 * 2];
    // --wav FILE: every sample vns3_audio returns, as 16-bit stereo at 44100 Hz (header sizes patched at the end).
    FILE *wav = wavPath ? fopen(wavPath, "wb") : NULL;
    long wavFrames = 0;
    if (wav) { static const unsigned char h[44] = { 'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0x44,0xac,0,0,0x10,0xb1,2,0,4,0,16,0,'d','a','t','a',0,0,0,0 }; fwrite(h, 1, 44, wav); }
    int ok = 1, turn = 0, nextPick = 0, nextAuto = autoFrom;
    // Keys and buttons stay down for 96 ms, as a person's press does, so games that poll the state see them.
    int releaseAt = -1, heldKey = 0, heldButton = -1;
    for (int t = 0; t < total; t += 16) {
        if (releaseAt >= 0 && t >= releaseAt) {
            if (heldKey) vns3_key(g, heldKey, false);
            if (heldButton >= 0) vns3_mouse_button(g, heldButton, false);
            releaseAt = -1; heldKey = 0; heldButton = -1;
        }
        for (int i = 0; i < n; i++) {
            Ev *e = &ev[i];
            if (e->t < t || e->t >= t + 16) continue;
            if (!strcmp(e->kind, "key")) { vns3_key(g, e->a, true); heldKey = e->a; releaseAt = t + 96; }
            else if (!strcmp(e->kind, "move")) vns3_mouse_move(g, e->a, e->b);
            else if (!strcmp(e->kind, "click") || !strcmp(e->kind, "rclick")) {
                heldButton = e->kind[0] == 'r' ? VNS3_MOUSE_RIGHT : VNS3_MOUSE_LEFT;
                vns3_mouse_move(g, e->a, e->b);
                vns3_mouse_button(g, heldButton, true);
                releaseAt = t + 96;
            } else if (!strcmp(e->kind, "shot")) shot(g, e->file);
        }
        // The next pick whose time has come, if any.
        while (nextPick < n && strcmp(ev[nextPick].kind, "pick")) nextPick++;
        const char *pick = nextPick < n && ev[nextPick].t <= t ? ev[nextPick].file : NULL;
        bool autopilot = autoFrom >= 0 && t >= autoFrom;
        if (releaseAt < 0 && s->wait == S3_WAIT_MENU && (pick || autopilot) && t >= nextAuto) {
            bool picked = false;
            int c = choose(s, pick, autopilot, &turn, &picked);
            if (c >= 0) {
                s->menuCursorPos = c;
                vns3_key(g, 0x0d, true); heldKey = 0x0d; releaseAt = t + 96;
                if (picked) nextPick++;
                nextAuto = t + 300;
            }
        } else if (autopilot && releaseAt < 0 && t >= nextAuto && s->wait != S3_WAIT_MENU) {
            // Message waits take Return at once; anything else, such as a title polling for input, once a second.
            vns3_key(g, 0x0d, true); heldKey = 0x0d; releaseAt = t + 96;
            nextAuto = t + (s->wait == S3_WAIT_KEY ? 300 : 1000);
        }
        if (!vns3_frame(g, 16)) { ok = !vns3_problem(g); break; }
        vns3_audio(g, audio, 705); // keep the mixer moving as a device would
        if (wav) {
            for (int i = 0; i < 705 * 2; i++) { float v = audio[i] * 32767.0f; short x = (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); fwrite(&x, 2, 1, wav); }
            wavFrames += 705;
        }
    }
    if (wav) {
        unsigned int data = (unsigned int)(wavFrames * 4), riff = data + 36;
        fseek(wav, 4, SEEK_SET); fwrite(&riff, 4, 1, wav);
        fseek(wav, 40, SEEK_SET); fwrite(&data, 4, 1, wav);
        fclose(wav);
    }
    while (ok && repl) {
        FILE *in = fopen(repl, "r");
        if (!in) break;
        char line[600];
        bool quit = false;
        while (ok && !quit && fgets(line, sizeof line, in)) {
            char cmd[16] = "", arg[512] = "";
            int x = 0, y = 0;
            sscanf(line, "%15s %511[^\n]", cmd, arg);
            sscanf(arg, "%d %d", &x, &y);
            int ms = 0;
            if (!strcmp(cmd, "quit")) quit = true;
            else if (!strcmp(cmd, "shot")) shot(g, arg);
            else if (!strcmp(cmd, "where")) printf("page %d pc %x wait %d menus %d\n", s->page, s->pc, s->wait, s->menuCount);
            else if (!strcmp(cmd, "trace")) s->trace = x != 0;
            else if (!strcmp(cmd, "arr")) { // arr VAR COUNT: elements 0..COUNT-1 of an array variable
                S3LV lv = { .var = (u16)x, .offset = 0, .element = true };
                for (int k = 0; k < y; k++) printf("%u ", s3_lv_get(s, &lv, k));
                printf("\n");
            }
            else if (!strcmp(cmd, "var")) { for (int v = x; v <= (y > x ? y : x); v++) printf("v%d=%u ", v, s3_var_get(s, (u16)v)); printf("\n"); }
            else if (!strcmp(cmd, "run")) ms = x;
            else if (!strcmp(cmd, "key")) { vns3_key(g, x, true); ok = vns3_frame(g, 16) || !vns3_problem(g); for (int k = 0; k < 5; k++) vns3_frame(g, 16); vns3_key(g, x, false); }
            else if (!strcmp(cmd, "tap")) { // a touch: move, down and up between two frames, as iOS delivers it
                vns3_mouse_move(g, x, y);
                vns3_mouse_button(g, VNS3_MOUSE_LEFT, true);
                vns3_mouse_button(g, VNS3_MOUSE_LEFT, false);
            }
            else if (!strcmp(cmd, "click") || !strcmp(cmd, "rclick")) {
                int b = cmd[0] == 'r' ? VNS3_MOUSE_RIGHT : VNS3_MOUSE_LEFT;
                vns3_mouse_move(g, x, y);
                vns3_mouse_button(g, b, true);
                for (int k = 0; k < 6; k++) vns3_frame(g, 16);
                vns3_mouse_button(g, b, false);
            }
            for (int t = 0; t < ms && ok; t += 16) { if (!vns3_frame(g, 16)) ok = !vns3_problem(g) && false; vns3_audio(g, audio, 705); }
            printf("ok %s\n", cmd);
            fflush(stdout);
        }
        fclose(in);
        if (quit) break;
    }
    if (final) shot(g, final);
    if (vns3_problem(g)) printf("problem: %s\n", vns3_problem(g));
    printf("title: %s\n", vns3_title(g));
    vns3_close(g);
    return ok ? 0 : 1;
}
