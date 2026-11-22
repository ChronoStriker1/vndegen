#include "sa.h"

#include <stdarg.h>

#include "vnfont.h"

#define STD_SLOTS 9
#define MSG_LINES 4

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
        char *f = sa_fmt("%s.aog", s->voice);
        sa_play(s->mix, CH_VOICE, 0, f, false);
        free(f);
        s->voice[0] = 0;
    }
    s->dirty = true;
}

static void host_speaker(void *ctx, const char *name, const char *voice, const char *face) {
    SA *s = ctx;
    snprintf(s->name, sizeof s->name, "%s", name);
    snprintf(s->voice, sizeof s->voice, "%s", voice[0] == '!' ? voice + 1 : "");
    s->dirty = true;
}

static void host_page(void *ctx) {
    SA *s = ctx;
    s->dirty = true;
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
        picture_set(s, &s->bg, n, 0, 0);
        for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
        free(n);
        break;
    }
    case 0x52: {
        char *n = opd_str(code, in, 0);
        int x = in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 4;
        int y = in->count > 2 ? sa_vm_get(vm, code, &in->o[2]) : 4;
        if (x >= 0 && x < STD_SLOTS) picture_set(s, &s->std[x], n, x, y);
        free(n);
        break;
    }
    case 0x56: {
        int x = in->count > 0 ? sa_vm_get(vm, code, &in->o[0]) : -1;
        for (int i = 0; i < STD_SLOTS; i++)
            if (x < 0 || x == i) picture_clear(&s->std[i]);
        s->dirty = true;
        break;
    }
    case 0x41:
        break;
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

enum { MSG_X = 26, MSG_Y = 540, TEXT_X = 150, TEXT_Y = 590, TEXT_SIZE = 24, TEXT_PITCH = 32, NAME_X = 150, NAME_Y = 552 };
enum { CHOICE_W = 504, CHOICE_H = 50, CHOICE_GAP = 12 };

static int choice_y(SA *s, int i) {
    int n = s->vm.choiceCount;
    int top = (SA_H - 160 - n * CHOICE_H - (n - 1) * CHOICE_GAP) / 2;
    return top + i * (CHOICE_H + CHOICE_GAP);
}

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
    if (s->lineCount || s->name[0]) {
        fill(fb, MSG_X, MSG_Y, 1228, 171, 0x90000000u);
        blit(fb, s->msgFrame, MSG_X, MSG_Y);
        if (s->name[0]) sa_text_draw(fb, SA_W, SA_H, s->name, -1, NAME_X, NAME_Y, TEXT_SIZE, 0xffffff, 0x000000);
        int left = s->shown;
        for (int i = 0; i < s->lineCount && left > 0; i++) {
            int n = utf8_len(s->lines[i]);
            sa_text_draw(fb, SA_W, SA_H, s->lines[i], n < left ? n : left, TEXT_X, TEXT_Y + i * TEXT_PITCH, TEXT_SIZE, 0xffffff, 0x000000);
            left -= n;
        }
    }
    if (s->vm.state == VM_WAIT_CHOICE) {
        for (int i = 0; i < s->vm.choiceCount; i++) {
            int y = choice_y(s, i);
            int x = (SA_W - CHOICE_W) / 2;
            fill(fb, x, y, CHOICE_W, CHOICE_H, i == s->hover ? 0xd0f0f8ffu : 0xc0c0d8e8u);
            const char *t = s->vm.choice[i].label;
            int w = sa_text_width(t, -1, TEXT_SIZE);
            sa_text_draw(fb, SA_W, SA_H, t, -1, x + (CHOICE_W - w) / 2, y + (CHOICE_H - TEXT_SIZE) / 2 - 2, TEXT_SIZE, 0x202020, 0xffffffffu);
        }
    }
    s->serial++;
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
    s->mix = sa_mixer_new(config->audioRate, &s->vfs);
    sa_mixer_set_log(s->mix, host_log, s);
    s->fb = sa_alloc((size_t)SA_W * SA_H * 4);
    s->msgFrame = load_image(s, "msg_a00.agf");
    snprintf(s->title, sizeof s->title, "BUNNY BLACK 3");
    s->host = (VMHost){.ctx = s, .text = host_text, .speaker = host_speaker, .page = host_page, .command = host_command, .log = host_log};
    s->hover = -1;
    if (!sa_vm_start(&s->vm, &s->vfs, "pro.box", 0, &s->host)) {
        if (err) snprintf(err, errSize, "can't start the prologue script");
        vnsystemaoi_close((VNSystemAoi *)s);
        return NULL;
    }
    s->dirty = true;
    return (VNSystemAoi *)s;
}

void vnsystemaoi_close(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    if (!s) return;
    sa_mixer_free(s->mix);
    sa_vm_free(&s->vm);
    sa_vfs_close(&s->vfs);
    picture_clear(&s->bg);
    for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
    sa_image_free(s->msgFrame);
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
    if (s->total && s->shown < s->total) {
        s->textMs += elapsedMs;
        int want = (int)(s->textMs / 30);
        if (want > s->total) want = s->total;
        if (want != s->shown) {
            s->shown = want;
            s->dirty = true;
        }
    }
    if (s->clicked) {
        s->clicked = false;
        if (s->vm.state == VM_WAIT_CHOICE) {
            int c = choice_at(s, s->mouseX, s->mouseY);
            if (c >= 0) {
                clear_text(s);
                s->name[0] = 0;
                sa_vm_choose(&s->vm, c);
            }
        } else if (s->shown < s->total) {
            s->shown = s->total;
        } else if (s->vm.state == VM_WAIT_CLICK) {
            clear_text(s);
            s->name[0] = 0;
            sa_stop(s->mix, CH_VOICE, 0, 0);
            sa_vm_click(&s->vm);
        }
        s->dirty = true;
    }
    if (s->vm.state == VM_RUN) sa_vm_run(&s->vm, &s->vfs, &s->host, 20000);
    if (s->vm.state == VM_DONE) {
        s->quit = true;
        return false;
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
}

void vnsystemaoi_set_paused(VNSystemAoi *sa, bool paused) { ((SA *)sa)->paused = paused; }
void vnsystemaoi_request_quit(VNSystemAoi *sa) { ((SA *)sa)->quit = true; }
const char *vnsystemaoi_title(VNSystemAoi *sa) { return ((SA *)sa)->title; }
const char *vnsystemaoi_problem(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    return s->problem[0] ? s->problem : NULL;
}
