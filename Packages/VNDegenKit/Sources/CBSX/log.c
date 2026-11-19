#include "bsx.h"

typedef struct LogBtn {
    Image *img;
    int frames, x, y;
} LogBtn;

struct LogPhase {
    int top;
    int lines, left, width;
    u32 back, voiceColor;
    LogBtn bar, up, down;
    Image *icon;
    int hoverEntry;
    int rows[16][2];
};

static Image *own(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static void log_btn(BX *b, const char *key, LogBtn *bt) {
    memset(bt, 0, sizeof *bt);
    const char *v = bx_cfg_get(&b->cfg, "log screen", key);
    if (!v) return;
    char name[64], f[64];
    bx_cfg_field(v, 0, name, sizeof name);
    bt->img = own(b, name);
    bt->frames = bx_cfg_field(v, 1, f, sizeof f) > 0 ? bx_eval(f, NULL) : 1;
    if (bt->frames < 1) bt->frames = 1;
    bt->x = bx_cfg_field(v, 2, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
    bt->y = bx_cfg_field(v, 3, f, sizeof f) > 0 ? bx_eval(f, NULL) : 0;
}

static bool on(const LogBtn *bt, int x, int y) {
    if (!bt->img) return false;
    int h = bt->img->h / bt->frames;
    return x >= bt->x && y >= bt->y && x < bt->x + bt->img->w && y < bt->y + h;
}

void bx_log_free(BX *b) {
    LogPhase *l = b->logp;
    if (!l) return;
    bx_image_free(l->bar.img);
    bx_image_free(l->up.img);
    bx_image_free(l->down.img);
    bx_image_free(l->icon);
    free(l);
    b->logp = NULL;
}

static int max_top(BX *b, LogPhase *l) { return b->logCount > l->lines ? b->logCount - l->lines : 0; }

bool bx_log_open(BX *b) {
    if (!b->logCount) return false;
    bx_log_free(b);
    LogPhase *l = bx_alloc(sizeof *l);
    b->logp = l;
    l->lines = clampi(bx_cfg_int(&b->cfg, "log screen", "lines", 5), 1, 16);
    l->left = bx_cfg_int(&b->cfg, "log screen", "left pos", 150);
    l->width = bx_cfg_int(&b->cfg, "log screen", "width", 550);
    l->back = bx_cfg_color(&b->cfg, "log screen", "back color", 0x80000000u);
    l->voiceColor = bx_cfg_color(&b->cfg, "log screen", "voice color", 0x40ff00ffu);
    log_btn(b, "bar", &l->bar);
    log_btn(b, "up button", &l->up);
    log_btn(b, "down button", &l->down);
    l->icon = own(b, bx_cfg_get(&b->cfg, "log screen", "voice icon"));
    l->top = max_top(b, l);
    l->hoverEntry = -1;
    b->phaseBack = b->phase;
    b->phase = PH_LOG;
    return true;
}

static void close_log(BX *b) {
    bx_log_free(b);
    b->phase = b->phaseBack;
    b->phaseBack = PH_NONE;
}

void bx_log_update(BX *b) {
    LogPhase *l = b->logp;
    if (!l) return;
    int scroll = b->wheel;
    b->wheel = 0;
    if (b->keyPress[0x26] || b->keyPress[0x21]) scroll++;
    if (b->keyPress[0x28] || b->keyPress[0x22]) scroll--;
    if (scroll > 0) l->top = clampi(l->top - scroll, 0, max_top(b, l));
    else if (scroll < 0) {
        if (l->top >= max_top(b, l)) {
            close_log(b);
            return;
        }
        l->top = clampi(l->top - scroll, 0, max_top(b, l));
    }
    l->hoverEntry = -1;
    for (int i = 0; i < l->lines; i++)
        if (b->my >= l->rows[i][0] && b->my < l->rows[i][1] && b->mx >= l->left - 40 && b->mx < l->left + l->width) l->hoverEntry = l->top + i;
    if (b->rclick) {
        b->rclick = false;
        close_log(b);
        return;
    }
    if (!b->click) return;
    b->click = false;
    if (on(&l->up, b->mx, b->my)) l->top = clampi(l->top - 1, 0, max_top(b, l));
    else if (on(&l->down, b->mx, b->my)) l->top = clampi(l->top + 1, 0, max_top(b, l));
    else if (l->bar.img && b->mx >= l->bar.x && b->mx < l->bar.x + l->bar.img->w) {
        int span = (l->down.y) - (l->up.y + (l->up.img ? l->up.img->h / l->up.frames : 0));
        if (span > 0) l->top = clampi((b->my - l->up.y) * (max_top(b, l) + 1) / span, 0, max_top(b, l));
    } else if (l->hoverEntry >= 0 && l->hoverEntry < b->logCount && b->logs[l->hoverEntry].voice)
        bx_snd_play(b, CH_VOICE, RX_VOICE, b->logs[l->hoverEntry].voice, false, 0);
}

static int wrap_lines(BX *b, const char *text, int width, char out[][256], int max) {
    int n = 0;
    char line[256];
    size_t ln = 0;
    int lw = 0;
    const char *p = text;
    while (*p && n < max) {
        u32 cp;
        int len = bx_utf8_decode(p, &cp);
        if (cp == '\n') {
            line[ln] = 0;
            snprintf(out[n++], 256, "%s", line);
            ln = 0;
            lw = 0;
            p += len;
            continue;
        }
        const char *w = p;
        if (cp == ' ' || cp >= 0x2e80) w += len;
        else
            while (*w && *w != ' ' && *w != '\n') {
                u32 c2;
                int l2 = bx_utf8_decode(w, &c2);
                if (c2 >= 0x2e80) break;
                w += l2;
            }
        char word[256];
        size_t wl = (size_t)(w - p) < sizeof word - 1 ? (size_t)(w - p) : sizeof word - 1;
        memcpy(word, p, wl);
        word[wl] = 0;
        int ww = bx_text_width(word, b->fontSize, b->fontW);
        if (lw + ww > width && ln) {
            line[ln] = 0;
            snprintf(out[n++], 256, "%s", line);
            ln = 0;
            lw = 0;
            if (word[0] == ' ') {
                p += wl;
                continue;
            }
        }
        if (ln + wl < sizeof line - 1) {
            memcpy(line + ln, word, wl);
            ln += wl;
            lw += ww;
        }
        p += wl ? wl : 1;
    }
    if (ln && n < max) {
        line[ln] = 0;
        snprintf(out[n++], 256, "%s", line);
    }
    return n;
}

void bx_log_draw(BX *b, u32 *dst) {
    LogPhase *l = b->logp;
    if (!l) return;
    bx_fill(dst, b->W, b->H, 0, 0, b->W, b->H, l->back);
    int lineH = b->fontH + 4;
    int y = 24;
    for (int i = 0; i < l->lines; i++) {
        int e = l->top + i;
        l->rows[i][0] = l->rows[i][1] = -1;
        if (e >= b->logCount) continue;
        LogEntry *le = &b->logs[e];
        char lines[8][256];
        int n = wrap_lines(b, le->text, l->width, lines, 8);
        int h = (n + (le->name ? 1 : 0)) * lineH + 8;
        l->rows[i][0] = y;
        l->rows[i][1] = y + h;
        if (le->voice && e == l->hoverEntry) bx_fill(dst, b->W, b->H, l->left - 40, y - 2, l->width + 44, h, l->voiceColor);
        if (le->voice && l->icon) bx_blit(dst, b->W, b->H, l->icon, l->left - 36, y, 255);
        int ty = y;
        if (le->name) {
            bx_text_draw(b, dst, b->W, b->H, le->name, l->left, ty, b->fontSize, b->fontW, b->nameColor, b->edge, b->shadow);
            ty += lineH;
        }
        for (int k = 0; k < n; k++) bx_text_draw(b, dst, b->W, b->H, lines[k], l->left, ty + k * lineH, b->fontSize, b->fontW, b->textColor, b->edge, b->shadow);
        y += h;
    }
    if (l->bar.img) bx_blit(dst, b->W, b->H, l->bar.img, l->bar.x, l->bar.y, 255);
    if (l->up.img) bx_blit_part(dst, b->W, b->H, l->up.img, 0, on(&l->up, b->mx, b->my) ? l->up.img->h / l->up.frames : 0, l->up.img->w, l->up.img->h / l->up.frames, l->up.x, l->up.y, 255);
    if (l->down.img) bx_blit_part(dst, b->W, b->H, l->down.img, 0, on(&l->down, b->mx, b->my) ? l->down.img->h / l->down.frames : 0, l->down.img->w, l->down.img->h / l->down.frames, l->down.x, l->down.y, 255);
}
