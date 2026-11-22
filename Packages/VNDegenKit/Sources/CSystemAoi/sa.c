#include "sa.h"

#include <stdarg.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>

#include "vnfont.h"

#define STD_SLOTS 9
#define MSG_LINES 4

enum { UI_CLOSE, UI_LEAF, UI_SKIP, UI_LOG, UI_REPEAT, UI_OPTION, UI_AUTO, UI_FEATHER, UI_CHOICE_TOP, UI_CHOICE_ROW, UI_CHOICE_BOTTOM, UI_COUNT };

static const char *const uiNames[UI_COUNT] = {"msg_b00.agf", "msg_b01.agf", "msg_b02.agf", "msg_b03.agf", "msg_b04.agf",
                                              "msg_b05.agf", "msg_b06.agf", "msg_a03.agf", "msg_c00.agf", "msg_c01.agf",
                                              "msg_c02.agf"};

enum { MODE_TITLE, MODE_STORY };
enum { TITLE_PROLOGUE, TITLE_START, TITLE_LOAD, TITLE_OPTION, TITLE_EXTRA, TITLE_END, TITLE_ITEMS };
enum { TITLE_X = 640, TITLE_Y = 369, TITLE_H = 50, TITLE_W = 640 };

typedef struct Button {
    int ui, x, y, w, h;
} Button;

enum { BTN_AUTO, BTN_SKIP, BTN_LOG, BTN_REPEAT, BTN_OPTION, BTN_CLOSE, BTN_COUNT };

static const Button buttons[BTN_COUNT] = {
    {UI_AUTO, 1086, 554, 78, 30}, {UI_SKIP, 1086, 584, 78, 30},   {UI_LOG, 1086, 614, 80, 30},
    {UI_REPEAT, 1086, 644, 80, 30}, {UI_OPTION, 1086, 674, 80, 30}, {UI_CLOSE, 1191, 554, 32, 30},
};

typedef struct BgmDef {
    int id;
    char file[40];
    double loopTo, loopFrom;
} BgmDef;

typedef struct Picture {
    char name[40];
    Image *img;
    int xslot, yslot;
} Picture;

struct SA {
    VNSystemAoiConfig cfg;
    char *gameDir, *saveDir;
    FILE *logFile;
    Vfs vfs;
    VM vm;
    VMHost host;
    Mixer *mix;
    u32 *fb;
    u32 serial;
    bool dirty, quit, paused;
    char title[128];
    char problem[256];
    Picture bg;
    Picture std[STD_SLOTS];
    Image *msgFrame;
    Image *ui[UI_COUNT];
    int hoverButton;
    bool autoMode, skipMode, windowHidden;
    u32 featherMs, autoMs;
    bool forceSkip;
    int mode, titleHover;
    VNSystemAoiMovies movies;
    bool haveMovies;
    pthread_mutex_t movieLock;
    void *movie;
    FILE *movieFile;
    double movieTime;
    int movieW, movieH;
    char *faceName[64], *facePrefix[64];
    int faceCount;
    char faceExpr[64][4];
    Picture face;
    char nextBox[32];
    int nextEntry;
    Image *titleBg, *titleItem[TITLE_ITEMS];
    char lastVoice[40];
    char name[96];
    char *lines[MSG_LINES];
    int lineCount;
    int shown, total;
    u32 textMs;
    char voice[32];
    int mouseX, mouseY;
    bool clicked;
    int hover;
    u32 waitMs;
    bool waitClick;
    BgmDef *bgm;
    int bgmCount;
    Image *msgBand;
    u32 *from;
    u32 transMs, transDur;
};

void sa_log(SA *s, const char *fmt, ...) {
    if (!s || !s->logFile) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(s->logFile, fmt, ap);
    va_end(ap);
    fputc('\n', s->logFile);
    fflush(s->logFile);
}

static void host_log(void *ctx, const char *msg) { sa_log(ctx, "%s", msg); }

static Image *load_image(SA *s, const char *name) {
    size_t n;
    u8 *d = sa_vfs_read(&s->vfs, name, &n);
    if (!d) {
        sa_log(s, "missing picture %s", name);
        return NULL;
    }
    Image *im = sa_agf_decode(d, n);
    free(d);
    if (!im) sa_log(s, "can't decode picture %s", name);
    return im;
}

static void picture_set(SA *s, Picture *p, const char *name, int xslot, int yslot) {
    if (strcmp(p->name, name)) {
        sa_image_free(p->img);
        p->img = load_image(s, name);
        snprintf(p->name, sizeof p->name, "%s", name);
    }
    p->xslot = xslot;
    p->yslot = yslot;
    s->dirty = true;
}

static void picture_clear(Picture *p) {
    sa_image_free(p->img);
    memset(p, 0, sizeof *p);
}

static void clear_text(SA *s) {
    for (int i = 0; i < s->lineCount; i++) free(s->lines[i]);
    s->lineCount = 0;
    s->shown = s->total = 0;
    s->textMs = 0;
}

static int utf8_len(const char *t) {
    int n = 0;
    for (u32 cp; *t; n++) t += sa_utf8_decode(t, &cp);
    return n;
}

static void host_text(void *ctx, const char *utf8, bool newLine) {
    SA *s = ctx;
    if (s->lineCount == MSG_LINES) return;
    s->lines[s->lineCount++] = sa_strdup(utf8);
    s->total += utf8_len(utf8);
    if (s->voice[0]) {
        snprintf(s->lastVoice, sizeof s->lastVoice, "%s.aog", s->voice);
        sa_play(s->mix, CH_VOICE, 0, s->lastVoice, false);
        s->voice[0] = 0;
    }
    s->dirty = true;
}

static int face_index_by_prefix(SA *s, const char *prefix) {
    for (int i = 0; i < s->faceCount; i++)
        if (!strncmp(s->facePrefix[i], prefix, 5)) return i;
    return -1;
}

static void load_faces(SA *s) {
    size_t n;
    u8 *raw = sa_vfs_read(&s->vfs, "evntFace.obj", &n);
    if (!raw) return;
    for (size_t i = 0; i < n; i++) raw[i] = (u8)~raw[i];
    size_t start = 0;
    for (size_t k = 1; k * 12 + 24 < n && !start; k++) {
        size_t P = k * 12;
        bool ok = true;
        for (int r = 0; r < 3 && ok; r++)
            for (int c = 0; c < 2 && ok; c++) {
                u32 off = rd32(raw + r * 12 + c * 4);
                if (P + off + 2 > n || (!raw[P + off] && !raw[P + off + 1]) || (off && (raw[P + off - 2] || raw[P + off - 1]))) ok = false;
            }
        if (ok) start = P;
    }
    if (!start) {
        free(raw);
        return;
    }
    char *prev = NULL;
    for (size_t p = start; p + 1 < n && s->faceCount < 64;) {
        size_t q = p;
        while (q + 1 < n && (raw[q] || raw[q + 1])) q += 2;
        char *str = sa_utf16_to_utf8(raw + p, (int)((q - p) / 2));
        if (!str[0] || !strcmp(str, "    ")) {
            free(str);
            break;
        }
        if (!strncmp(str, "ta", 2) && strlen(str) == 5 && prev) {
            s->faceName[s->faceCount] = prev;
            s->facePrefix[s->faceCount] = str;
            snprintf(s->faceExpr[s->faceCount], 4, "000");
            s->faceCount++;
            prev = NULL;
        } else {
            free(prev);
            prev = str;
        }
        p = q + 2;
    }
    free(prev);
    free(raw);
    sa_log(s, "%d face sets", s->faceCount);
}

static void host_speaker(void *ctx, const char *name, const char *voice, const char *face) {
    SA *s = ctx;
    char want[40] = "";
    if (face[0]) snprintf(want, sizeof want, "%s", face);
    else
        for (int i = 0; i < s->faceCount; i++)
            if (!strcmp(s->faceName[i], name)) {
                snprintf(want, sizeof want, "ka%.3s%s.agf", s->facePrefix[i] + 2, s->faceExpr[i]);
                break;
            }
    if (want[0]) picture_set(s, &s->face, want, 0, 0);
    else picture_clear(&s->face);
    snprintf(s->name, sizeof s->name, "%s", name);
    snprintf(s->voice, sizeof s->voice, "%s", voice[0] == '!' ? voice + 1 : "");
    s->dirty = true;
}

static void host_page(void *ctx) {
    SA *s = ctx;
    s->dirty = true;
}

static double asd_time(const char *v) {
    double t = 0;
    const char *c = strchr(v, ':');
    if (c) {
        t = atof(v) * 60;
        v = c + 1;
    }
    return t + atof(v);
}

static void load_bgm_defs(SA *s) {
    size_t n;
    u8 *d = sa_vfs_read(&s->vfs, "bgm.asd", &n);
    if (!d) return;
    d[n] = 0;
    BgmDef *cur = NULL;
    for (char *line = (char *)d, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        while (*line == ' ' || *line == '\t') line++;
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        if (!strncmp(line, "[ASTD_", 6)) {
            s->bgm = sa_realloc(s->bgm, sizeof(BgmDef) * (size_t)(s->bgmCount + 1));
            cur = &s->bgm[s->bgmCount++];
            memset(cur, 0, sizeof *cur);
            cur->id = atoi(line + 6);
            continue;
        }
        char *eq = strchr(line, '=');
        if (!cur || !eq) continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        for (char *q = eq - 1; q >= k && (*q == ' ' || *q == '\t'); q--) *q = 0;
        while (*v == ' ' || *v == '\t') v++;
        if (sa_ieq(k, "File")) snprintf(cur->file, sizeof cur->file, "%s", v);
        else if (sa_ieq(k, "LoopTo")) cur->loopTo = asd_time(v);
        else if (sa_ieq(k, "LoopFrom")) cur->loopFrom = asd_time(v);
    }
    free(d);
    sa_log(s, "%d BGM entries", s->bgmCount);
}

static void play_bgm(SA *s, int id) {
    for (int i = 0; i < s->bgmCount; i++)
        if (s->bgm[i].id == id) {
            sa_play_loop(s->mix, CH_BGM, 0, s->bgm[i].file, s->bgm[i].loopTo, s->bgm[i].loopFrom);
            return;
        }
    sa_log(s, "no BGM %d", id);
}

static void compose(SA *s);

static u32 transition_ms(int kind) { return kind == 10 ? 500 : kind == 11 || kind == 12 ? 300 : 0; }

static void begin_transition(SA *s, VM *vm, int kind) {
    u32 ms = transition_ms(kind);
    if (!ms) return;
    if (s->dirty) compose(s);
    memcpy(s->from, s->fb, (size_t)SA_W * SA_H * 4);
    s->transDur = ms;
    s->transMs = 0;
    vm->state = VM_WAIT_TIME;
    s->waitMs = ms;
}

static int64_t movie_read(void *user, uint64_t offset, void *buffer, size_t length) {
    FILE *f = user;
    if (fseeko(f, (off_t)offset, SEEK_SET)) return -1;
    return (int64_t)fread(buffer, 1, length, f);
}

static void movie_stop(SA *s) {
    pthread_mutex_lock(&s->movieLock);
    void *m = s->movie;
    s->movie = NULL;
    pthread_mutex_unlock(&s->movieLock);
    if (m) s->movies.close(m);
    if (s->movieFile) fclose(s->movieFile);
    s->movieFile = NULL;
    if (s->vm.state == VM_WAIT_TIME) s->vm.state = VM_RUN;
    s->dirty = true;
}

static void movie_start(SA *s, VM *vm, const char *name) {
    if (!s->haveMovies) {
        sa_log(s, "movie %s left out: no movie decoder", name);
        return;
    }
    char *p = sa_fmt("%s/%s", s->gameDir, name);
    char real[2048];
    FILE *f = sa_path_resolve(p, real, sizeof real) == 1 ? fopen(real, "rb") : NULL;
    free(p);
    if (!f) {
        sa_log(s, "missing movie %s", name);
        return;
    }
    fseeko(f, 0, SEEK_END);
    uint64_t size = (uint64_t)ftello(f);
    VNSystemAoiMovieInfo info = {0};
    char err[256] = "";
    void *m = s->movies.open(movie_read, f, size, 0, &info, err, sizeof err);
    if (!m) {
        sa_log(s, "can't play movie %s: %s", name, err);
        fclose(f);
        return;
    }
    sa_log(s, "movie %s %dx%d %.2f fps %.1f s, sound %d Hz", name, info.width, info.height, info.fps, info.duration, info.audioRate);
    sa_stop(s->mix, CH_BGM, 0, 0);
    s->movieFile = f;
    s->movieTime = 0;
    s->movieW = info.width;
    s->movieH = info.height;
    pthread_mutex_lock(&s->movieLock);
    s->movie = m;
    pthread_mutex_unlock(&s->movieLock);
    vm->state = VM_WAIT_TIME;
    s->waitMs = 0;
    s->waitClick = false;
}

static void movie_tick(SA *s, u32 ms) {
    s->movieTime += ms / 1000.0;
    int changed = 0, ended = 0;
    const uint8_t *px = s->movies.frame(s->movie, s->movieTime, &changed, &ended);
    if (px && changed) {
        u32 *fb = s->fb;
        for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
        int ox = (SA_W - s->movieW) / 2, oy = (SA_H - s->movieH) / 2;
        for (int y = 0; y < s->movieH; y++) {
            int dy = oy + y;
            if (dy < 0 || dy >= SA_H) continue;
            for (int x = 0; x < s->movieW; x++) {
                int dx = ox + x;
                if (dx < 0 || dx >= SA_W) continue;
                const uint8_t *q = px + ((size_t)y * s->movieW + x) * 4;
                fb[(size_t)dy * SA_W + dx] = 0xff000000u | (u32)q[2] << 16 | (u32)q[1] << 8 | q[0];
            }
        }
        s->serial++;
    }
    if (ended) movie_stop(s);
}

static char *opd_str(const u8 *code, const Insn *in, int i) {
    if (i >= in->count || in->o[i].kind != OPD_STR) return sa_strdup("");
    return sa_utf16_to_utf8(code + in->o[i].strOffset, in->o[i].strLen);
}

static void host_command(void *ctx, const u8 *code, const Insn *in, VM *vm) {
    SA *s = ctx;
    switch (in->op) {
    case 0x4c: {
        char *n = opd_str(code, in, 0);
        begin_transition(s, vm, in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 0);
        picture_set(s, &s->bg, n, 0, 0);
        for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
        free(n);
        break;
    }
    case 0x52: {
        char *n = opd_str(code, in, 0);
        int x = in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 4;
        int y = in->count > 2 ? sa_vm_get(vm, code, &in->o[2]) : 4;
        begin_transition(s, vm, in->count > 3 ? sa_vm_get(vm, code, &in->o[3]) : 0);
        if (x >= 0 && x < STD_SLOTS) picture_set(s, &s->std[x], n, x, y);
        int fi = face_index_by_prefix(s, n);
        if (fi >= 0 && strlen(n) >= 8) snprintf(s->faceExpr[fi], 4, "%.3s", n + 5);
        free(n);
        break;
    }
    case 0x56: {
        int x = in->count > 0 ? sa_vm_get(vm, code, &in->o[0]) : -1;
        begin_transition(s, vm, in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 0);
        for (int i = 0; i < STD_SLOTS; i++)
            if (x < 0 || x == i) picture_clear(&s->std[i]);
        s->dirty = true;
        break;
    }
    case 0x41:
        if (in->count > 0) play_bgm(s, sa_vm_get(vm, code, &in->o[0]));
        break;
    case 0x49: {
        char *n = opd_str(code, in, 0);
        movie_start(s, vm, n);
        free(n);
        break;
    }
    case 0x42:
        break;
    case 0x2a: {
        int n = in->count > 0 ? sa_vm_get(vm, code, &in->o[0]) : 0;
        if (n < 0) {
            s->waitClick = true;
            vm->state = VM_WAIT_TIME;
            s->waitMs = 0;
        } else if (n > 0) {
            vm->state = VM_WAIT_TIME;
            s->waitMs = (u32)n * 1000 / 60;
        }
        break;
    }
    default: {
        static u32 seen[8];
        u32 bit = 1u << (in->op & 31);
        if (!(seen[in->op >> 5] & bit)) {
            seen[in->op >> 5] |= bit;
            char *t = sa_insn_text(code, in);
            sa_log(s, "not handled: %s", t + 8);
            free(t);
        }
        break;
    }
    }
}

static void blit(u32 *dst, const Image *im, int x, int y) {
    if (!im) return;
    for (int j = 0; j < im->h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= SA_H) continue;
        const u32 *src = im->px + (size_t)j * im->w;
        u32 *d = dst + (size_t)dy * SA_W;
        for (int i = 0; i < im->w; i++) {
            int dx = x + i;
            if (dx < 0 || dx >= SA_W) continue;
            u32 p = src[i], a = p >> 24;
            if (!a) continue;
            if (a == 255) {
                d[dx] = p;
                continue;
            }
            u32 q = d[dx], ia = 255 - a;
            u32 r = ((p & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((p >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((p >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            d[dx] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

static void blit_alpha(u32 *dst, const Image *im, int x, int y, u32 alpha) {
    if (!im) return;
    for (int j = 0; j < im->h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= SA_H) continue;
        for (int i = 0; i < im->w; i++) {
            int dx = x + i;
            if (dx < 0 || dx >= SA_W) continue;
            u32 p = im->px[(size_t)j * im->w + i], a = (p >> 24) * alpha / 255;
            if (!a) continue;
            u32 q = dst[(size_t)dy * SA_W + dx], ia = 255 - a;
            u32 r = ((p & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((p >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((p >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            dst[(size_t)dy * SA_W + dx] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

static void blit_rect(u32 *dst, const Image *im, int sx, int sy, int w, int h, int x, int y) {
    if (!im) return;
    for (int j = 0; j < h; j++) {
        int dy = y + j, syy = sy + j;
        if (dy < 0 || dy >= SA_H || syy < 0 || syy >= im->h) continue;
        for (int i = 0; i < w; i++) {
            int dx = x + i, sxx = sx + i;
            if (dx < 0 || dx >= SA_W || sxx < 0 || sxx >= im->w) continue;
            u32 p = im->px[(size_t)syy * im->w + sxx], a = p >> 24;
            if (!a) continue;
            u32 *d = &dst[(size_t)dy * SA_W + dx];
            if (a == 255) {
                *d = p;
                continue;
            }
            u32 q = *d, ia = 255 - a;
            u32 r = ((p & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((p >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((p >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            *d = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

static void fill(u32 *dst, int x, int y, int w, int h, u32 rgba) {
    u32 a = rgba >> 24;
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= SA_H) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= SA_W) continue;
            u32 q = dst[(size_t)j * SA_W + i], ia = 255 - a;
            u32 r = ((rgba & 0xff) * a + (q & 0xff) * ia) / 255;
            u32 g = ((rgba >> 8 & 0xff) * a + (q >> 8 & 0xff) * ia) / 255;
            u32 b = ((rgba >> 16 & 0xff) * a + (q >> 16 & 0xff) * ia) / 255;
            dst[(size_t)j * SA_W + i] = 0xff000000u | b << 16 | g << 8 | r;
        }
    }
}

enum { MSG_X = 0, MSG_Y = 546, TEXT_X = 324, TEXT_Y = 589, TEXT_SIZE = 28, TEXT_PITCH = 31, NAME_X = 305, NAME_Y = 553 };
enum { CHOICE_W = 504, CHOICE_H = 50, CHOICE_EDGE = 22, FEATHER_X = 1046, FEATHER_Y = 679 };

static int choice_y(SA *s, int i) {
    int n = s->vm.choiceCount;
    int top = (MSG_Y - n * CHOICE_H - 2 * CHOICE_EDGE) / 2 + CHOICE_EDGE;
    return top + i * CHOICE_H;
}

static int button_at(int x, int y) {
    for (int i = 0; i < BTN_COUNT; i++)
        if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w && y >= buttons[i].y && y < buttons[i].y + buttons[i].h) return i;
    return -1;
}

static bool window_shown(SA *s) { return (s->lineCount || s->name[0]) && !s->windowHidden; }

static void compose(SA *s) {
    u32 *fb = s->fb;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    blit(fb, s->bg.img, 0, 0);
    for (int i = 0; i < STD_SLOTS; i++) {
        Picture *p = &s->std[i];
        if (!p->img) continue;
        int cx = p->xslot * SA_W / 8;
        blit(fb, p->img, cx - p->img->w / 2, SA_H - p->img->h);
    }
    if (window_shown(s)) {
        blit_alpha(fb, s->msgBand, MSG_X, MSG_Y, 160);
        blit(fb, s->msgFrame, MSG_X, MSG_Y);
        blit_rect(fb, s->ui[UI_LEAF], 0, 0, 44, 41, 1044, 536);
        for (int i = 0; i < BTN_COUNT; i++) {
            const Button *b = &buttons[i];
            int col = (i == BTN_AUTO && s->autoMode) || (i == BTN_SKIP && s->skipMode) ? 1 : 0;
            blit_rect(fb, s->ui[b->ui], col * b->w, i == s->hoverButton ? b->h : 0, b->w, b->h, b->x, b->y);
        }
        if (s->vm.state == VM_WAIT_CLICK && s->shown >= s->total) {
            int f = (int)(s->featherMs / 33) % 31;
            blit_rect(fb, s->ui[UI_FEATHER], f * 32, 0, 32, 32, FEATHER_X, FEATHER_Y);
        }
        blit(fb, s->face.img, -1, 294);
        if (s->name[0]) sa_text_draw(fb, SA_W, SA_H, s->name, -1, NAME_X, NAME_Y, TEXT_SIZE, 0xffffff, 0x000000);
        int left = s->shown;
        for (int i = 0; i < s->lineCount && left > 0; i++) {
            int n = utf8_len(s->lines[i]);
            sa_text_draw(fb, SA_W, SA_H, s->lines[i], n < left ? n : left, TEXT_X, TEXT_Y + i * TEXT_PITCH, TEXT_SIZE, 0xffffff, 0x000000);
            left -= n;
        }
    }
    if (s->vm.state == VM_WAIT_CHOICE) {
        int x = (SA_W - CHOICE_W) / 2;
        blit(fb, s->ui[UI_CHOICE_TOP], x, choice_y(s, 0) - CHOICE_EDGE);
        for (int i = 0; i < s->vm.choiceCount; i++) {
            int y = choice_y(s, i);
            blit_rect(fb, s->ui[UI_CHOICE_ROW], 0, i == s->hover ? CHOICE_H : 0, CHOICE_W, CHOICE_H, x, y);
            const char *t = s->vm.choice[i].label;
            int w = sa_text_width(t, -1, TEXT_SIZE);
            sa_text_draw(fb, SA_W, SA_H, t, -1, x + (CHOICE_W - w) / 2, y + (CHOICE_H - TEXT_SIZE) / 2, TEXT_SIZE, 0x302010, 0xffffffffu);
        }
        blit(fb, s->ui[UI_CHOICE_BOTTOM], x, choice_y(s, s->vm.choiceCount));
    }
    if (s->transDur && s->transMs < s->transDur) {
        u32 a = s->transMs * 256 / s->transDur;
        for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) {
            u32 p = fb[i], q = s->from[i];
            u32 rb = (((p & 0xff00ff) * a + (q & 0xff00ff) * (256 - a)) >> 8) & 0xff00ff;
            u32 g = (((p & 0xff00) * a + (q & 0xff00) * (256 - a)) >> 8) & 0xff00;
            fb[i] = 0xff000000u | rb | g;
        }
    }
    s->serial++;
}

static int title_at(int x, int y) {
    if (x < TITLE_X || x >= TITLE_X + TITLE_W || y < TITLE_Y) return -1;
    int i = (y - TITLE_Y) / TITLE_H;
    return i < TITLE_ITEMS ? i : -1;
}

static void compose_title(SA *s) {
    u32 *fb = s->fb;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    blit(fb, s->titleBg, 0, 0);
    for (int i = 0; i < TITLE_ITEMS; i++)
        blit_rect(fb, s->titleItem[i], 0, i == s->titleHover ? TITLE_H : 0, TITLE_W, TITLE_H, TITLE_X, TITLE_Y + i * TITLE_H);
    s->serial++;
}

static void reset_scene(SA *s) {
    picture_clear(&s->bg);
    picture_clear(&s->face);
    for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
    clear_text(s);
    s->name[0] = 0;
    picture_clear(&s->face);
    s->windowHidden = s->autoMode = s->skipMode = false;
    s->transDur = 0;
    s->waitClick = false;
}

static bool start_script(SA *s, const char *box, int entry) {
    s->vm.depth = 0;
    s->vm.choiceCount = 0;
    s->mode = MODE_STORY;
    if (sa_vm_start(&s->vm, &s->vfs, box, entry, &s->host)) return true;
    sa_log(s, "can't start %s #%d", box, entry);
    return false;
}

static void go_title(SA *s) {
    reset_scene(s);
    sa_stop(s->mix, CH_VOICE, 0, 0);
    sa_stop(s->mix, CH_BGM, 0, 500);
    s->mode = MODE_TITLE;
    s->titleHover = -1;
    s->dirty = true;
}

static void new_game(SA *s) {
    reset_scene(s);
    memset(s->vm.c, 0, sizeof s->vm.c);
    memset(s->vm.f, 0, sizeof s->vm.f);
    snprintf(s->nextBox, sizeof s->nextBox, "bo0000.box");
    s->nextEntry = 0;
    start_script(s, "init.box", 0);
}

static void title_pick(SA *s, int item) {
    switch (item) {
    case TITLE_PROLOGUE:
        reset_scene(s);
        s->nextBox[0] = 0;
        start_script(s, "pro.box", 0);
        break;
    case TITLE_START:
        new_game(s);
        break;
    case TITLE_END:
        s->quit = true;
        break;
    default:
        sa_log(s, "title item %d not built yet", item);
        break;
    }
    s->dirty = true;
}

static int choice_at(SA *s, int x, int y) {
    if (s->vm.state != VM_WAIT_CHOICE) return -1;
    int cx = (SA_W - CHOICE_W) / 2;
    for (int i = 0; i < s->vm.choiceCount; i++) {
        int cy = choice_y(s, i);
        if (x >= cx && x < cx + CHOICE_W && y >= cy && y < cy + CHOICE_H) return i;
    }
    return -1;
}

const VNSystemAoiMovies *vnsystemaoi_find_movies(void) {
    static VNSystemAoiMovies movies;
    static bool found = false;
    if (found) return &movies;
    const char *names[5] = {"vnffmovie_open", "vnffmovie_frame", "vnffmovie_audio", "vnffmovie_seek", "vnffmovie_close"};
    void *ptrs[5];
    for (int i = 0; i < 5; i++)
        if (!(ptrs[i] = dlsym(RTLD_DEFAULT, names[i]))) return NULL;
    movies.open = (void *(*)(VNSystemAoiMovieRead, void *, uint64_t, int, VNSystemAoiMovieInfo *, char *, size_t))ptrs[0];
    movies.frame = (const uint8_t *(*)(void *, double, int *, int *))ptrs[1];
    movies.audio = (size_t(*)(void *, float *, size_t))ptrs[2];
    movies.seek = (void (*)(void *, double))ptrs[3];
    movies.close = (void (*)(void *))ptrs[4];
    found = true;
    return &movies;
}

bool vnsystemaoi_probe(const char *gameDir, char *title, size_t titleSize) {
    char *p = sa_fmt("%s/box.vfs", gameDir);
    char real[2048];
    bool ok = sa_path_resolve(p, real, sizeof real) == 1;
    free(p);
    if (!ok) return false;
    Vfs v;
    if (!sa_vfs_open(&v, gameDir)) return false;
    ok = sa_vfs_find(&v, "init.box") && sa_vfs_find(&v, "pro.box");
    sa_vfs_close(&v);
    if (ok && title) snprintf(title, titleSize, "BUNNY BLACK 3");
    return ok;
}

VNSystemAoi *vnsystemaoi_open(const VNSystemAoiConfig *config, char *err, size_t errSize) {
    SA *s = sa_alloc(sizeof *s);
    s->cfg = *config;
    s->gameDir = sa_strdup(config->gameDir);
    s->saveDir = sa_strdup(config->saveDir ? config->saveDir : config->gameDir);
    if (config->logPath) s->logFile = fopen(config->logPath, "w");
    if (!sa_vfs_open(&s->vfs, s->gameDir)) {
        if (err) snprintf(err, errSize, "no SystemAoi archives in the game folder");
        vnsystemaoi_close((VNSystemAoi *)s);
        return NULL;
    }
    sa_log(s, "%d archive entries", s->vfs.count);
    static const char *const fonts[] = {"MS Gothic", "MS PGothic", NULL};
    char used[1024] = "";
    if (sa_font_init(fonts, used, sizeof used)) sa_log(s, "font %s", used);
    else sa_log(s, "no font");
    sa_font_set_spacing(1);
    s->mix = sa_mixer_new(config->audioRate, &s->vfs);
    sa_mixer_set_log(s->mix, host_log, s);
    s->fb = sa_alloc((size_t)SA_W * SA_H * 4);
    s->msgFrame = load_image(s, "msg_a00.agf");
    s->msgBand = load_image(s, "msg_a00a.agf");
    for (int i = 0; i < UI_COUNT; i++) s->ui[i] = load_image(s, uiNames[i]);
    s->hoverButton = -1;
    pthread_mutex_init(&s->movieLock, NULL);
    if (config->movies) {
        s->movies = *config->movies;
        s->haveMovies = true;
    }
    s->from = sa_alloc((size_t)SA_W * SA_H * 4);
    load_bgm_defs(s);
    load_faces(s);
    snprintf(s->title, sizeof s->title, "BUNNY BLACK 3");
    s->host = (VMHost){.ctx = s, .text = host_text, .speaker = host_speaker, .page = host_page, .command = host_command, .log = host_log};
    s->hover = -1;
    s->titleBg = load_image(s, "tit_a00.agf");
    for (int i = 0; i < TITLE_ITEMS; i++) {
        char n[32];
        snprintf(n, sizeof n, "tit_a1%d.agf", i);
        s->titleItem[i] = load_image(s, n);
    }
    char *sav = sa_fmt("%s/Bunny3.sav", s->saveDir);
    char real[2048];
    bool first = sa_path_resolve(sav, real, sizeof real) != 1;
    free(sav);
    go_title(s);
    if (first) {
        title_pick(s, TITLE_PROLOGUE);
        sa_log(s, "first start: prologue");
    }
    s->dirty = true;
    return (VNSystemAoi *)s;
}

void vnsystemaoi_close(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    if (!s) return;
    if (s->movie) movie_stop(s);
    sa_mixer_free(s->mix);
    sa_vm_free(&s->vm);
    sa_vfs_close(&s->vfs);
    picture_clear(&s->bg);
    for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
    sa_image_free(s->msgFrame);
    sa_image_free(s->msgBand);
    sa_image_free(s->titleBg);
    for (int i = 0; i < TITLE_ITEMS; i++) sa_image_free(s->titleItem[i]);
    for (int i = 0; i < UI_COUNT; i++) sa_image_free(s->ui[i]);
    free(s->from);
    free(s->bgm);
    picture_clear(&s->face);
    for (int i = 0; i < s->faceCount; i++) {
        free(s->faceName[i]);
        free(s->facePrefix[i]);
    }
    clear_text(s);
    sa_font_free();
    if (s->logFile) fclose(s->logFile);
    free(s->fb);
    free(s->gameDir);
    free(s->saveDir);
    free(s);
}

bool vnsystemaoi_frame(VNSystemAoi *sa, uint32_t elapsedMs) {
    SA *s = (SA *)sa;
    if (s->quit) return false;
    if (s->paused) return true;
    if (s->movie) {
        if (s->clicked) {
            s->clicked = false;
            movie_stop(s);
        } else {
            movie_tick(s, elapsedMs);
            return true;
        }
    }
    if (s->mode == MODE_TITLE) {
        int h = title_at(s->mouseX, s->mouseY);
        if (h != s->titleHover) {
            s->titleHover = h;
            s->dirty = true;
        }
        if (s->clicked) {
            s->clicked = false;
            if (h >= 0) title_pick(s, h);
        }
        if (s->mode == MODE_TITLE) {
            if (s->dirty) {
                compose_title(s);
                s->dirty = false;
            }
            return !s->quit;
        }
    }
    if (s->total && s->shown < s->total) {
        s->textMs += elapsedMs;
        int want = (int)(s->textMs / 30);
        if (want > s->total) want = s->total;
        if (want != s->shown) {
            s->shown = want;
            s->dirty = true;
        }
    }
    if (s->forceSkip && s->mode == MODE_STORY) {
        if (s->transDur) s->transMs = s->transDur;
        if (s->vm.state == VM_WAIT_TIME && !s->movie) s->waitMs = 0;
        if (s->vm.state == VM_WAIT_TIME && s->waitClick) {
            s->waitClick = false;
            s->vm.state = VM_RUN;
        }
        if (s->vm.state == VM_WAIT_CLICK) {
            s->shown = s->total;
            s->clicked = true;
            s->mouseX = s->mouseY = -1;
        }
    }
    if (s->transDur) {
        s->transMs += elapsedMs;
        if (s->transMs >= s->transDur) s->transDur = 0;
        s->dirty = true;
    }
    if (s->vm.state == VM_WAIT_TIME && !s->waitClick) {
        if (s->waitMs > elapsedMs) s->waitMs -= elapsedMs;
        else {
            s->waitMs = 0;
            s->vm.state = VM_RUN;
        }
    }
    if (s->vm.state == VM_WAIT_CLICK && s->shown >= s->total) {
        s->featherMs += elapsedMs;
        s->dirty = true;
        bool voice = sa_playing(s->mix, CH_VOICE, 0);
        if (s->skipMode || (s->autoMode && !voice && (s->autoMs += elapsedMs) >= 1500)) {
            s->autoMs = 0;
            s->clicked = true;
            s->mouseX = s->mouseY = -1;
        }
    } else {
        s->featherMs = 0;
        s->autoMs = 0;
    }
    if (s->skipMode && s->shown < s->total) s->shown = s->total;
    int btn = window_shown(s) ? button_at(s->mouseX, s->mouseY) : -1;
    if (btn != s->hoverButton) {
        s->hoverButton = btn;
        s->dirty = true;
    }
    if (s->clicked && s->windowHidden) {
        s->clicked = false;
        s->windowHidden = false;
        s->dirty = true;
    }
    if (s->clicked && btn >= 0) {
        s->clicked = false;
        s->dirty = true;
        switch (btn) {
        case BTN_AUTO:
            s->autoMode = !s->autoMode;
            s->skipMode = false;
            break;
        case BTN_SKIP:
            s->skipMode = !s->skipMode;
            s->autoMode = false;
            break;
        case BTN_REPEAT:
            if (s->lastVoice[0]) sa_play(s->mix, CH_VOICE, 0, s->lastVoice, false);
            break;
        case BTN_CLOSE:
            s->windowHidden = true;
            break;
        default:
            sa_log(s, "button %d not built yet", btn);
            break;
        }
    }
    if (s->clicked) {
        s->clicked = false;
        if (s->skipMode && s->mouseX >= 0) s->skipMode = false;
        if (s->vm.state == VM_WAIT_TIME && s->waitClick) {
            s->waitClick = false;
            s->vm.state = VM_RUN;
        } else if (s->vm.state == VM_WAIT_CHOICE) {
            int c = choice_at(s, s->mouseX, s->mouseY);
            if (c >= 0) {
                clear_text(s);
                s->name[0] = 0;
                picture_clear(&s->face);
                sa_vm_choose(&s->vm, c);
            }
        } else if (s->shown < s->total) {
            s->shown = s->total;
        } else if (s->vm.state == VM_WAIT_CLICK) {
            clear_text(s);
            s->name[0] = 0;
            picture_clear(&s->face);
            sa_stop(s->mix, CH_VOICE, 0, 0);
            sa_vm_click(&s->vm);
        }
        s->dirty = true;
    }
    if (s->vm.state == VM_RUN) sa_vm_run(&s->vm, &s->vfs, &s->host, 20000);
    if (s->vm.state == VM_DONE) {
        if (s->nextBox[0]) {
            char box[32];
            snprintf(box, sizeof box, "%s", s->nextBox);
            s->nextBox[0] = 0;
            start_script(s, box, s->nextEntry);
        } else {
            if (s->vm.f[0x78]) sa_log(s, "turn scripts done; the town is not built yet");
            go_title(s);
            char *sav = sa_fmt("%s/Bunny3.sav", s->saveDir);
            sa_mkdirs(sav);
            free(sav);
            return true;
        }
    }
    int h = choice_at(s, s->mouseX, s->mouseY);
    if (h != s->hover) {
        s->hover = h;
        s->dirty = true;
    }
    if (s->dirty) {
        compose(s);
        s->dirty = false;
    }
    return true;
}

const uint32_t *vnsystemaoi_pixels(VNSystemAoi *sa, int *width, int *height) {
    SA *s = (SA *)sa;
    *width = SA_W;
    *height = SA_H;
    return s->fb;
}

uint32_t vnsystemaoi_frame_serial(VNSystemAoi *sa) { return ((SA *)sa)->serial; }

void vnsystemaoi_mouse_move(VNSystemAoi *sa, int x, int y) {
    SA *s = (SA *)sa;
    s->mouseX = x;
    s->mouseY = y;
}

void vnsystemaoi_mouse_button(VNSystemAoi *sa, int button, bool down) {
    SA *s = (SA *)sa;
    if (button == VNSYSTEMAOI_MOUSE_LEFT && !down) s->clicked = true;
}

void vnsystemaoi_key(VNSystemAoi *sa, int virtualKey, bool down) {
    SA *s = (SA *)sa;
    if (!down) return;
    if (virtualKey == 0x0d || virtualKey == 0x20) {
        if (s->vm.state == VM_WAIT_CHOICE && s->hover < 0) s->hover = 0;
        if (s->vm.state == VM_WAIT_CHOICE) {
            int c = s->hover;
            clear_text(s);
            s->name[0] = 0;
            picture_clear(&s->face);
            sa_vm_choose(&s->vm, c);
            s->dirty = true;
        } else {
            s->clicked = true;
            s->mouseX = s->mouseY = -1;
        }
    } else if (virtualKey == 0x26 || virtualKey == 0x28) {
        if (s->vm.state == VM_WAIT_CHOICE && s->vm.choiceCount) {
            int n = s->vm.choiceCount;
            s->hover = ((s->hover < 0 ? 0 : s->hover) + (virtualKey == 0x28 ? 1 : n - 1)) % n;
            s->dirty = true;
        }
    }
}

void vnsystemaoi_wheel(VNSystemAoi *sa, int notches) {}

void vnsystemaoi_audio(VNSystemAoi *sa, float *out, int frames) {
    SA *s = (SA *)sa;
    sa_mixer_mix(s->mix, out, frames, s->paused);
    pthread_mutex_lock(&s->movieLock);
    if (s->movie && !s->paused) {
        static float buf[8192];
        int done = 0;
        while (done < frames) {
            int want = frames - done < 4096 ? frames - done : 4096;
            size_t got = s->movies.audio(s->movie, buf, (size_t)want);
            for (size_t i = 0; i < got * 2; i++) out[done * 2 + i] += buf[i];
            if (got < (size_t)want) break;
            done += (int)got;
        }
    }
    pthread_mutex_unlock(&s->movieLock);
}

void vnsystemaoi_set_paused(VNSystemAoi *sa, bool paused) { ((SA *)sa)->paused = paused; }
void vnsystemaoi_set_skip(VNSystemAoi *sa, bool on) {
    SA *s = (SA *)sa;
    s->forceSkip = on;
    s->dirty = true;
}
void vnsystemaoi_request_quit(VNSystemAoi *sa) { ((SA *)sa)->quit = true; }
const char *vnsystemaoi_title(VNSystemAoi *sa) { return ((SA *)sa)->title; }
const char *vnsystemaoi_problem(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    return s->problem[0] ? s->problem : NULL;
}
