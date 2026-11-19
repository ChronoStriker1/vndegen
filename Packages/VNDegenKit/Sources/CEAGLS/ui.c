#include "eagls.h"

#include <time.h>

typedef struct Sheet {
    Image *base, *hover, *mask, *parts;
    char name[32];
} Sheet;

enum { MB_NONE, MB_TITLE, MB_EXIT };

struct UI {
    Sheet win;
    Sheet save, load, loadT, check, hist;
    Sheet option, bra, bgm;
    int track;
    int saveMode;
    bool fromTitle;
    int page;
    int hover;
    bool confirmSave, confirmLoad, loadDelete;
    int checkKind, checkSlot;
    int checkHover;
    bool menuOpen;
    int menuHover;
    int logTop;
    int logHover;
    bool dragBar;
    int mb;
    int mbHover;
    int cfgTab;
    int cfgDrag;
    int cfgHover;
    int cfgSaved[8];
    int newest;
    SlotInfo info[EG_SLOTS];
    bool infoLoaded;
    Image *thumbs[EG_SLOTS];
    u32 noticeUntil;
    char notice[64];
};

static void sheet_load(EG *e, Sheet *s, const char *base) {
    if (s->base) return;
    char n[64];
    s->base = eg_image_load(e, base);
    snprintf(n, sizeof n, "%sC", base);
    s->hover = eg_image_load(e, n);
    snprintf(n, sizeof n, "%sM", base);
    s->mask = eg_image_load(e, n);
    snprintf(n, sizeof n, "%sP", base);
    s->parts = eg_image_load(e, n);
    snprintf(s->name, sizeof s->name, "%s", base);
}

static void sheet_free(Sheet *s) {
    image_free(s->base);
    image_free(s->hover);
    image_free(s->mask);
    image_free(s->parts);
    memset(s, 0, sizeof *s);
}

static int mask_color(const Image *m, int x, int y) {
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    u32 p = m->px[(size_t)y * m->w + x];
    if (m->alpha && !(p >> 24)) return 0;
    return (int)(p & 0xff);
}

static void draw_masked(u32 *dst, int dw, int dh, const Image *src, const Image *mask, int color, int ox, int oy) {
    if (!src || !mask || !color) return;
    int w = src->w < mask->w ? src->w : mask->w, h = src->h < mask->h ? src->h : mask->h;
    for (int y = 0; y < h; y++) {
        int dy = oy + y;
        if (dy < 0 || dy >= dh) continue;
        const u32 *m = mask->px + (size_t)y * mask->w;
        const u32 *s = src->px + (size_t)y * src->w;
        u32 *d = dst + (size_t)dy * dw;
        for (int x = 0; x < w; x++) {
            if ((int)(m[x] & 0xff) != color || (mask->alpha && !(m[x] >> 24))) continue;
            int dx = ox + x;
            if (dx < 0 || dx >= dw) continue;
            u32 p = s[x];
            u32 a = src->alpha ? p >> 24 : 255;
            if (a >= 255) d[dx] = p | 0xff000000u;
            else if (a) {
                u32 ia = 255 - a, q = d[dx];
                d[dx] = (((p & 0xff) * a + (q & 0xff) * ia) / 255) | ((((p >> 8) & 0xff) * a + ((q >> 8) & 0xff) * ia) / 255) << 8 |
                        ((((p >> 16) & 0xff) * a + ((q >> 16) & 0xff) * ia) / 255) << 16 | 0xff000000u;
            }
        }
    }
}

static int ini_rect(EG *e, const char *key, int *r) { return ini_ints(&e->ini, key, r, 8); }

static i32 sysvar(EG *e, const char *name) {
    Var *v = var_find(&e->vars, name);
    return v ? var_at(v, 0)->i : 0;
}

static void set_sysvar(EG *e, const char *name, i32 value) { var_at(var_get(&e->vars, name, true), 0)->i = value; }

static void sound(EG *e, const char *name) { snd_play(e, name, 0, 2, 0); }

void ui_init(EG *e) {
    e->ui = eg_alloc(sizeof(UI));
    e->ui->confirmSave = e->ui->confirmLoad = true;
    e->ui->newest = -1;
}

void ui_free(EG *e) {
    UI *u = e->ui;
    if (!u) return;
    Sheet *sheets[] = {&u->win, &u->save, &u->load, &u->loadT, &u->check, &u->hist, &u->option, &u->bra};
    for (size_t i = 0; i < sizeof sheets / sizeof *sheets; i++) sheet_free(sheets[i]);
    for (int i = 0; i < EG_SLOTS; i++) image_free(u->thumbs[i]);
    free(u);
    e->ui = NULL;
}

static void load_infos(EG *e) {
    UI *u = e->ui;
    if (u->infoLoaded) return;
    u->infoLoaded = true;
    u->newest = -1;
    long long best = 0;
    for (int i = 0; i < EG_SLOTS; i++) {
        image_free(u->thumbs[i]);
        u->thumbs[i] = NULL;
        memset(&u->info[i], 0, sizeof u->info[i]);
        if (!save_info(e, i, &u->info[i], &u->thumbs[i])) continue;
        long long k = u->info[i].stamp;
        if (k > best) {
            best = k;
            u->newest = i;
        }
    }
}

void ui_slots_changed(EG *e) {
    if (e->ui) e->ui->infoLoaded = false;
}

void ui_notice(EG *e, const char *text) {
    snprintf(e->ui->notice, sizeof e->ui->notice, "%s", text);
    e->ui->noticeUntil = e->now + 1500;
}

static void open_saveload(EG *e, int mode, bool fromTitle) {
    UI *u = e->ui;
    u->fromTitle = fromTitle;
    u->checkKind = 0;
    u->hover = 0;
    if (fromTitle) sheet_load(e, &u->loadT, "LoadT");
    sheet_load(e, &u->save, "Save");
    sheet_load(e, &u->load, "Load");
    sheet_load(e, &u->check, "CHECK");
    u->infoLoaded = false;
    load_infos(e);
    e->uiMode = mode;
}

void ui_open(EG *e, int mode) {
    UI *u = e->ui;
    u->hover = 0;
    switch (mode) {
    case UI_SAVE:
    case UI_LOAD: open_saveload(e, mode, false); break;
    case UI_LOG:
        sheet_load(e, &u->hist, "HISTMAIN");
        u->logTop = e->logCount - 1 - 4;
        if (u->logTop < 0) u->logTop = 0;
        e->uiMode = mode;
        break;
    case UI_CONFIG:
        u->cfgTab = 0;
        u->cfgDrag = -1;
        u->cfgSaved[0] = e->textSpeed;
        u->cfgSaved[1] = e->autoSpeed;
        u->cfgSaved[2] = e->volBgm;
        u->cfgSaved[3] = e->volVoice;
        u->cfgSaved[4] = e->volSe;
        u->cfgSaved[5] = e->volMovie;
        u->cfgSaved[6] = e->textWhite;
        u->cfgSaved[7] = e->mute;
        e->uiMode = mode;
        break;
    default: e->uiMode = mode; break;
    }
}

void ui_open_title_load(EG *e) { open_saveload(e, UI_LOAD, true); }

void ui_message_box(EG *e, int kind) {
    e->ui->mb = kind;
    e->ui->mbHover = -1;
    e->uiMode = UI_MSGBOX;
}

static void close_ui(EG *e) {
    e->uiMode = UI_NONE;
    e->ui->checkKind = 0;
    if (e->scriptUi) e->scriptUiDone = true;
}

static const int slotColors[8] = {35, 40, 45, 50, 55, 60, 65, 70};

static int slot_of_color(int c) {
    for (int i = 0; i < 8; i++)
        if (slotColors[i] == c) return i;
    return -1;
}

static Sheet *saveload_sheet(UI *u, int mode) {
    if (mode == UI_LOAD) return u->fromTitle && u->loadT.base ? &u->loadT : &u->load;
    return &u->save;
}

static void slot_text(EG *e, int slot, char *out, size_t n) {
    SlotInfo *in = &e->ui->info[slot];
    if (!in->used) snprintf(out, n, "No.%02d(e)--/--/-- --:--(e)NO DATA", slot + 1);
    else snprintf(out, n, "No.%02d(e)%04d/%02d/%02d %02d:%02d(e)%s", slot + 1, in->year, in->month, in->day, in->hour, in->minute, in->text);
}

static void draw_lines(EG *e, u32 *dst, const char *s, int x0, int y0, int x1, int y1, int fontIndex, u32 color) {
    const Font *f = &e->fonts[fontIndex];
    int size = f->size > 0 ? f->size : 13, spacing = f->size > 0 ? f->spacing : 2, line = size + (f->size > 0 ? f->lineGap : 3);
    int fx = x0 * 8, y = y0;
    for (const u8 *p = (const u8 *)s; *p;) {
        if (*p == '(' && (p[1] == 'e' || p[1] == 'E') && p[2] == ')') {
            fx = x0 * 8;
            y += line;
            p += 3;
            continue;
        }
        int len = eg_sjis_len(p);
        u32 cp;
        eg_sjis_decode(p, &cp);
        int adv = text_advance(cp, size, spacing);
        if (fx + adv > x1 * 8 && fx > x0 * 8) {
            fx = x0 * 8;
            y += line;
        }
        if (y + size > y1) break;
        char ch[3] = {0};
        memcpy(ch, p, (size_t)len);
        text_draw(e, dst, e->W, e->H, ch, fx / 8, y, size, spacing, color, false);
        fx += adv;
        p += len;
    }
}

static void draw_saveload(EG *e, u32 *dst) {
    UI *u = e->ui;
    Sheet *s = saveload_sheet(u, e->uiMode);
    if (!s->base) return;
    int W = e->W, H = e->H;
    blit(dst, W, H, s->base, 0, 0, 255);
    int r[8];
    bool tog1 = e->uiMode == UI_SAVE ? u->confirmSave : u->confirmLoad;
    if (ini_rect(e, "SaveChkTip", r) == 4 && tog1) blit_part(dst, W, H, s->hover, r[0], r[1], r[2] - r[0], r[3] - r[1], r[0], r[1], 255);
    if (ini_rect(e, "SaveDelTip", r) == 4 && u->loadDelete) blit_part(dst, W, H, s->hover, r[0], r[1], r[2] - r[0], r[3] - r[1], r[0], r[1], 255);
    draw_masked(dst, W, H, s->hover, s->mask, 80 + u->page * 5, 0, 0);
    if (u->hover && !u->checkKind) draw_masked(dst, W, H, s->hover, s->mask, u->hover, 0, 0);
    load_infos(e);
    for (int i = 0; i < 8; i++) {
        int slot = u->page * 8 + i;
        char key[32];
        int t[4], d[4], m[2];
        snprintf(key, sizeof key, "SaveThumbPut%d", i + 1);
        if (ini_ints(&e->ini, key, t, 4) == 4 && u->thumbs[slot]) blit(dst, W, H, u->thumbs[slot], t[0], t[1], 255);
        snprintf(key, sizeof key, "SaveDataPut%d", i + 1);
        if (ini_ints(&e->ini, key, d, 4) == 4) {
            char txt[512];
            slot_text(e, slot, txt, sizeof txt);
            draw_lines(e, dst, txt, d[0], d[1] + 3, d[2], d[3], 9, 0xffffffffu);
        }
        snprintf(key, sizeof key, "SaveNewDataMarkPut%d", i + 1);
        if (slot == u->newest && ini_ints(&e->ini, key, m, 2) == 2 && ini_rect(e, "SaveNewDataTip", r) == 4)
            blit_part(dst, W, H, s->parts, r[0], r[1], r[2] - r[0], r[3] - r[1], m[0], m[1], 255);
    }
    if (u->checkKind) {
        Sheet *c = &u->check;
        blit(dst, W, H, c->base, 0, 0, 255);
        if (u->checkHover) draw_masked(dst, W, H, c->hover, c->mask, u->checkHover, 0, 0);
        const char *key = u->checkKind == 1 ? "SaveCheckSaveState" : u->checkKind == 2 ? "SaveCheckLoadState" : "SaveCheckDelState";
        int q[6];
        if (ini_ints(&e->ini, key, q, 6) == 6) blit_part(dst, W, H, c->parts, q[4], q[5], q[2], q[3], q[0], q[1], 255);
        int tp[2], dp[4];
        if (ini_ints(&e->ini, "SaveCheckThumbPut", tp, 2) == 2 && u->thumbs[u->checkSlot]) blit(dst, W, H, u->thumbs[u->checkSlot], tp[0], tp[1], 255);
        else if (u->checkKind == 1 && ini_ints(&e->ini, "SaveCheckThumbPut", tp, 2) == 2 && e->thumb) blit(dst, W, H, e->thumb, tp[0], tp[1], 255);
        if (ini_ints(&e->ini, "SaveCheckDataPut", dp, 4) == 4) {
            char txt[512];
            if (u->checkKind == 1) {
                time_t now = time(NULL);
                struct tm tmv;
                localtime_r(&now, &tmv);
                snprintf(txt, sizeof txt, "No.%02d(e)%04d/%02d/%02d %02d:%02d(e)%s", u->checkSlot + 1, tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                         tmv.tm_hour, tmv.tm_min, e->text ? e->text : "");
            } else slot_text(e, u->checkSlot, txt, sizeof txt);
            draw_lines(e, dst, txt, dp[0], dp[1], dp[2], dp[3], 9, 0xffffffffu);
        }
    }
}

static void do_save(EG *e, int slot) {
    if (save_slot(e, slot)) {
        e->ui->newest = slot;
        e->ui->infoLoaded = false;
        load_infos(e);
        e->ui->newest = slot;
    }
}

static void do_load(EG *e, int slot) {
    if (!load_slot(e, slot)) return;
    e->uiMode = UI_NONE;
    e->ui->checkKind = 0;
    e->ui->menuOpen = false;
    if (e->scriptUi) e->scriptUiDone = true;
}

static void update_saveload(EG *e) {
    UI *u = e->ui;
    Sheet *s = saveload_sheet(u, e->uiMode);
    if (u->checkKind) {
        int c = mask_color(u->check.mask, e->mx, e->my);
        if (c != u->checkHover && c) sound(e, "SYS_OV");
        u->checkHover = c;
        if (e->rclick) {
            u->checkKind = 0;
            return;
        }
        if (!e->click) return;
        if (c == 50) {
            sound(e, "SYS_OK");
            int kind = u->checkKind, slot = u->checkSlot;
            u->checkKind = 0;
            if (kind == 1) do_save(e, slot);
            else if (kind == 2) do_load(e, slot);
            else {
                save_delete(e, slot);
                u->infoLoaded = false;
            }
        } else if (c == 100) {
            sound(e, "SYS_OK");
            u->checkKind = 0;
        }
        return;
    }
    int c = mask_color(s->mask, e->mx, e->my);
    if (c != u->hover && c) sound(e, "SYS_OV");
    u->hover = c;
    if (e->rclick) {
        close_ui(e);
        return;
    }
    if (e->wheel) {
        u->page = clampi(u->page - e->wheel, 0, 9);
        return;
    }
    if (!e->click || !c) return;
    sound(e, "SYS_OK");
    if (c == 30) close_ui(e);
    else if (c == 25 || c == 20) {
        if (!u->fromTitle) e->uiMode = e->uiMode == UI_SAVE ? UI_LOAD : UI_SAVE;
    } else if (c == 75) {
        if (e->uiMode == UI_SAVE) u->confirmSave = !u->confirmSave;
        else u->confirmLoad = !u->confirmLoad;
    } else if (c == 130) u->loadDelete = !u->loadDelete;
    else if (c >= 80 && c <= 125 && c % 5 == 0) u->page = (c - 80) / 5;
    else {
        int i = slot_of_color(c);
        if (i < 0) return;
        int slot = u->page * 8 + i;
        u->checkSlot = slot;
        u->checkHover = 0;
        if (u->loadDelete && u->info[slot].used) u->checkKind = 3;
        else if (e->uiMode == UI_SAVE) {
            if (u->confirmSave) u->checkKind = 1;
            else do_save(e, slot);
        } else if (u->info[slot].used) {
            if (u->confirmLoad) u->checkKind = 2;
            else do_load(e, slot);
        }
    }
}

static void log_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    Sheet *s = &u->hist;
    int W = e->W, H = e->H;
    if (s->base) blit(dst, W, H, s->base, 0, 0, 255);
    if (u->logHover) draw_masked(dst, W, H, s->hover, s->mask, u->logHover, 0, 0);
    int count = e->logCount - 1;
    for (int k = 0; k < 4; k++) {
        int idx = u->logTop + k;
        if (idx < 0 || idx >= count) continue;
        char key[32];
        int r[4];
        snprintf(key, sizeof key, "HistoryMesWnd%d", k + 1);
        if (ini_ints(&e->ini, key, r, 4) != 4) continue;
        const Font *f = msg_font(e, 0);
        int size = f->size, spacing = f->spacing, line = f->size + f->lineGap;
        u32 color = 0xffc0c0c0u;
        LogEntry *l = &e->logs[idx];
        int nr[4];
        snprintf(key, sizeof key, "HistoryMesNameChar%d", k + 1);
        if (l->name && ini_ints(&e->ini, key, nr, 4) == 4) {
            const Font *nf = msg_font(e, 1);
            int tw = text_width(l->name, nf->size, nf->spacing);
            text_draw(e, dst, W, H, l->name, (nr[0] + nr[2] - tw) / 2, (nr[1] + nr[3] - nf->size) / 2, nf->size, nf->spacing, 0xffffffffu, false);
        }
        int fx = r[0] * 8, y = r[1];
        for (const u8 *p = (const u8 *)l->text; *p;) {
            if (*p == '(' && (p[1] == 'e' || p[1] == 'E') && p[2] == ')') {
                fx = r[0] * 8;
                y += line;
                p += 3;
                continue;
            }
            int tag = msg_tag(e, p, &f, &color);
            if (tag) {
                size = f->size;
                spacing = f->spacing;
                p += tag;
                continue;
            }
            int len = eg_sjis_len(p);
            u32 cp;
            eg_sjis_decode(p, &cp);
            int adv = text_advance(cp, size, spacing);
            if (fx + adv > r[2] * 8 && fx > r[0] * 8) {
                fx = r[0] * 8;
                y += line;
            }
            char ch[3] = {0};
            memcpy(ch, p, (size_t)len);
            text_draw(e, dst, W, H, ch, fx / 8, y, size, spacing, color, true);
            fx += adv;
            p += len;
        }
        if (l->voice) {
            int b[4], src[4], over[4];
            snprintf(key, sizeof key, "HistorySndBtn%d", k + 1);
            if (ini_ints(&e->ini, key, b, 4) == 4 && ini_rect(e, "HistorySndBtnSrc", src) == 4 && ini_rect(e, "HistorySndBtnOverSrc", over) == 4) {
                int *sr = u->logHover == (k + 1) * 10 ? over : src;
                blit_part(dst, W, H, s->parts, sr[0], sr[1], sr[2] - sr[0], sr[3] - sr[1], b[0], b[1], 255);
            }
        }
    }
    int bar[4], tip[4];
    if (ini_rect(e, "HistoryScrollBarState", bar) == 4 && ini_rect(e, "HistoryScrollTip", tip) == 4 && count > 4) {
        int th = tip[3] - tip[1];
        int span = bar[3] - bar[1] - th;
        int pos = bar[1] + (int)((i64)span * u->logTop / (count - 4 > 0 ? count - 4 : 1));
        blit_part(dst, W, H, s->parts, tip[0], tip[1], tip[2] - tip[0], th, bar[0], pos, 255);
    }
}

static void log_update(EG *e) {
    UI *u = e->ui;
    int count = e->logCount - 1;
    int maxTop = count - 4 > 0 ? count - 4 : 0;
    int c = mask_color(u->hist.mask, e->mx, e->my);
    if (c != u->logHover && c) sound(e, "SYS_OV");
    u->logHover = c;
    if (e->wheel < 0 && u->logTop >= maxTop) {
        close_ui(e);
        return;
    }
    if (e->wheel) u->logTop = clampi(u->logTop - e->wheel, 0, maxTop);
    if (e->rclick) {
        close_ui(e);
        return;
    }
    if (u->dragBar && e->lDown) {
        int bar[4];
        if (ini_rect(e, "HistoryScrollBarState", bar) == 4 && bar[3] > bar[1]) u->logTop = clampi((int)((i64)(e->my - bar[1]) * maxTop / (bar[3] - bar[1])), 0, maxTop);
    }
    if (!e->lDown) u->dragBar = false;
    if (e->lPress && c == 90) u->dragBar = true;
    if (!e->click || !c) return;
    if (c == 100) {
        sound(e, "SYS_OK");
        close_ui(e);
    } else if (c == 50) u->logTop = 0;
    else if (c == 60) u->logTop = maxTop;
    else if (c == 70) u->logTop = clampi(u->logTop - 1, 0, maxTop);
    else if (c == 80) u->logTop = clampi(u->logTop + 1, 0, maxTop);
    else if (c >= 10 && c <= 40 && c % 10 == 0) {
        int idx = u->logTop + c / 10 - 1;
        if (idx >= 0 && idx < count && e->logs[idx].voice) snd_play(e, e->logs[idx].voice, 0, 6, 0);
    }
}

typedef struct Rect {
    int x, y, w, h;
} Rect;

static bool in_rect(Rect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

#define DLG_X 182
#define DLG_Y 122
#define DLG_W 432
#define DLG_H 324

static const char *cfgTabs[] = {"Text", "Sound"};

static Rect tab_rect(int i) { return (Rect){DLG_X + 6 + i * 64, DLG_Y + 6, 62, 20}; }
static Rect ok_rect(void) { return (Rect){DLG_X + DLG_W - 190, DLG_Y + DLG_H - 30, 86, 22}; }
static Rect cancel_rect(void) { return (Rect){DLG_X + DLG_W - 96, DLG_Y + DLG_H - 30, 86, 22}; }

typedef struct Ctl {
    int kind;
    const char *label;
    int *value;
    int max;
} Ctl;

static int ctl_list(EG *e, int tab, Ctl *out) {
    int n = 0;
    if (tab == 0) {
        out[n++] = (Ctl){0, "Text Speed", &e->textSpeedSlider, 100};
        out[n++] = (Ctl){0, "Auto Speed", &e->autoSpeed, 100};
        out[n++] = (Ctl){1, "Text Color: White", &e->textWhite, 1};
        out[n++] = (Ctl){1, "Enable skipping for unread lines", &e->skipUnread, 1};
        out[n++] = (Ctl){1, "In auto-mode, wait for voice to finish", &e->autoWaitVoice, 1};
    } else {
        out[n++] = (Ctl){1, "Mute All", &e->mute, 1};
        out[n++] = (Ctl){0, "Music Volume", &e->volBgm, 100};
        out[n++] = (Ctl){0, "Voice Volume", &e->volVoice, 100};
        out[n++] = (Ctl){0, "Sound Effects Volume", &e->volSe, 100};
        out[n++] = (Ctl){0, "Movie Volume", &e->volMovie, 100};
    }
    return n;
}

static Rect ctl_rect(int i, int kind) {
    int y = DLG_Y + 44 + i * 46;
    return kind == 0 ? (Rect){DLG_X + 24, y + 18, DLG_W - 48, 16} : (Rect){DLG_X + 24, y + 10, 16, 16};
}

static void frame(u32 *dst, int W, int H, Rect r, u32 light, u32 dark) {
    fill_rect(dst, W, H, r.x, r.y, r.w, 1, light);
    fill_rect(dst, W, H, r.x, r.y, 1, r.h, light);
    fill_rect(dst, W, H, r.x, r.y + r.h - 1, r.w, 1, dark);
    fill_rect(dst, W, H, r.x + r.w - 1, r.y, 1, r.h, dark);
}

static void button(EG *e, u32 *dst, Rect r, const char *label, bool hot) {
    int W = e->W, H = e->H;
    fill_rect(dst, W, H, r.x, r.y, r.w, r.h, hot ? 0xfff6e8d9u : 0xfff0f0f0u);
    frame(dst, W, H, r, 0xffffffffu, 0xff707070u);
    int tw = text_width(label, 14, 0);
    text_draw(e, dst, W, H, label, r.x + (r.w - tw) / 2, r.y + (r.h - 14) / 2, 14, 0, 0xff000000u, false);
}

static void config_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    int W = e->W, H = e->H;
    fill_rect(dst, W, H, DLG_X, DLG_Y, DLG_W, DLG_H, 0xfff0f0f0u);
    frame(dst, W, H, (Rect){DLG_X, DLG_Y, DLG_W, DLG_H}, 0xffffffffu, 0xff404040u);
    for (int i = 0; i < 2; i++) {
        Rect t = tab_rect(i);
        fill_rect(dst, W, H, t.x, t.y, t.w, t.h, i == u->cfgTab ? 0xffffffffu : 0xffe0e0e0u);
        frame(dst, W, H, t, 0xffffffffu, 0xff909090u);
        text_draw(e, dst, W, H, cfgTabs[i], t.x + 8, t.y + 3, 14, 0, 0xff000000u, false);
    }
    frame(dst, W, H, (Rect){DLG_X + 6, DLG_Y + 26, DLG_W - 12, DLG_H - 62}, 0xff909090u, 0xffffffffu);
    Ctl c[8];
    int n = ctl_list(e, u->cfgTab, c);
    for (int i = 0; i < n; i++) {
        Rect r = ctl_rect(i, c[i].kind);
        if (c[i].kind == 0) {
            text_draw(e, dst, W, H, c[i].label, r.x, r.y - 18, 14, 0, 0xff000000u, false);
            fill_rect(dst, W, H, r.x, r.y + 6, r.w, 4, 0xffc8c8c8u);
            frame(dst, W, H, (Rect){r.x, r.y + 6, r.w, 4}, 0xff808080u, 0xffffffffu);
            int kx = r.x + (r.w - 12) * *c[i].value / c[i].max;
            fill_rect(dst, W, H, kx, r.y, 12, r.h, u->cfgDrag == i ? 0xffe5f1fbu : 0xfff0f0f0u);
            frame(dst, W, H, (Rect){kx, r.y, 12, r.h}, 0xffffffffu, 0xff606060u);
            char val[16];
            snprintf(val, sizeof val, "%d", *c[i].value);
            text_draw(e, dst, W, H, val, r.x + r.w - text_width(val, 14, 0), r.y - 18, 14, 0, 0xff000000u, false);
        } else {
            fill_rect(dst, W, H, r.x, r.y, r.w, r.h, 0xffffffffu);
            frame(dst, W, H, r, 0xff606060u, 0xffd0d0d0u);
            if (*c[i].value) text_draw(e, dst, W, H, "v", r.x + 4, r.y, 14, 0, 0xff000000u, false);
            text_draw(e, dst, W, H, c[i].label, r.x + 24, r.y + 1, 14, 0, 0xff000000u, false);
        }
    }
    button(e, dst, ok_rect(), "OK", u->cfgHover == 1);
    button(e, dst, cancel_rect(), "Cancel", u->cfgHover == 2);
}

static void config_apply(EG *e) {
    e->textSpeed = 40 - e->textSpeedSlider * 40 / 100;
    snd_volumes(e);
}

static void config_update(EG *e) {
    UI *u = e->ui;
    Ctl c[8];
    int n = ctl_list(e, u->cfgTab, c);
    u->cfgHover = in_rect(ok_rect(), e->mx, e->my) ? 1 : in_rect(cancel_rect(), e->mx, e->my) ? 2 : 0;
    if (u->cfgDrag >= 0 && u->cfgDrag < n) {
        if (!e->lDown) u->cfgDrag = -1;
        else {
            Rect r = ctl_rect(u->cfgDrag, 0);
            *c[u->cfgDrag].value = clampi((e->mx - r.x - 6) * c[u->cfgDrag].max / (r.w - 12), 0, c[u->cfgDrag].max);
            config_apply(e);
        }
    }
    if (e->lPress)
        for (int i = 0; i < n; i++)
            if (c[i].kind == 0) {
                Rect r = ctl_rect(i, 0);
                if (in_rect((Rect){r.x - 4, r.y - 4, r.w + 8, r.h + 8}, e->mx, e->my)) u->cfgDrag = i;
            }
    bool cancel = e->rclick;
    if (e->click) {
        for (int i = 0; i < 2; i++)
            if (in_rect(tab_rect(i), e->mx, e->my)) u->cfgTab = i;
        for (int i = 0; i < n; i++)
            if (c[i].kind == 1 && in_rect((Rect){ctl_rect(i, 1).x, ctl_rect(i, 1).y, 300, 16}, e->mx, e->my)) {
                *c[i].value = !*c[i].value;
                config_apply(e);
            }
        if (u->cfgHover == 1) {
            config_apply(e);
            config_save(e);
            close_ui(e);
            return;
        }
        if (u->cfgHover == 2) cancel = true;
    }
    if (cancel) {
        e->textSpeed = u->cfgSaved[0];
        e->autoSpeed = u->cfgSaved[1];
        e->volBgm = u->cfgSaved[2];
        e->volVoice = u->cfgSaved[3];
        e->volSe = u->cfgSaved[4];
        e->volMovie = u->cfgSaved[5];
        e->textWhite = u->cfgSaved[6];
        e->mute = u->cfgSaved[7];
        e->textSpeedSlider = (40 - e->textSpeed) * 100 / 40;
        snd_volumes(e);
        close_ui(e);
    }
}

static Rect mb_rect(void) { return (Rect){278, 242, 244, 86}; }
static Rect mb_button(int i) { return (Rect){278 + 22 + i * 108, 242 + 56, 86, 22}; }

static void mb_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    int W = e->W, H = e->H;
    Rect r = mb_rect();
    fill_rect(dst, W, H, r.x, r.y, r.w, r.h, 0xfff0f0f0u);
    fill_rect(dst, W, H, r.x, r.y, r.w, 46, 0xffffffffu);
    frame(dst, W, H, r, 0xffd0d0d0u, 0xff404040u);
    const char *q = u->mb == MB_TITLE ? "Return to title?" : "Exit game?";
    int tw = text_width(q, 14, 0);
    text_draw(e, dst, W, H, q, r.x + (r.w - tw) / 2, r.y + 16, 14, 0, 0xff000000u, false);
    button(e, dst, mb_button(0), "OK", u->mbHover == 0);
    button(e, dst, mb_button(1), "Cancel", u->mbHover == 1);
}

static void mb_update(EG *e) {
    UI *u = e->ui;
    u->mbHover = in_rect(mb_button(0), e->mx, e->my) ? 0 : in_rect(mb_button(1), e->mx, e->my) ? 1 : -1;
    if (e->rclick) {
        close_ui(e);
        return;
    }
    if (!e->click || u->mbHover < 0) return;
    int kind = u->mb;
    close_ui(e);
    if (u->mbHover == 1) return;
    if (kind == MB_TITLE) game_to_title(e);
    else {
        config_save(e);
        e->quit = true;
    }
}

static void option_draw(EG *e, u32 *dst, Sheet *s, int hover) {
    blit(dst, e->W, e->H, s->base, 0, 0, 255);
    if (hover) draw_masked(dst, e->W, e->H, s->hover, s->mask, hover, 0, 0);
}

static const Rect soundTracks[10] = {{24, 72, 373, 63},  {24, 172, 373, 64},  {24, 271, 373, 63},  {24, 370, 373, 63},  {24, 470, 373, 64},
                                     {405, 52, 373, 63}, {405, 152, 373, 63}, {405, 254, 373, 63}, {405, 353, 373, 63}, {405, 452, 373, 63}};
static const Rect soundButtons[7] = {{487, 555, 39, 43}, {545, 555, 39, 43}, {604, 555, 39, 43}, {663, 555, 39, 43}, {732, 554, 60, 43}, {11, 566, 23, 23}, {418, 566, 23, 22}};
static const Rect soundBar = {43, 570, 365, 15};

static void sound_play_track(EG *e, int i) {
    UI *u = e->ui;
    u->track = i;
    char name[32];
    snprintf(name, sizeof name, "WAVE\\%02d", i + 1);
    snd_play(e, name, 1, 0, 0);
}

static int sound_hit(EG *e) {
    for (int i = 0; i < 10; i++)
        if (in_rect(soundTracks[i], e->mx, e->my)) return i + 1;
    for (int i = 0; i < 7; i++)
        if (in_rect(soundButtons[i], e->mx, e->my)) return 20 + i;
    if (in_rect(soundBar, e->mx, e->my)) return 30;
    return 0;
}

static void sound_update(EG *e) {
    UI *u = e->ui;
    int h = sound_hit(e);
    if (h != u->hover && h) sound(e, "SYS_OV");
    u->hover = h;
    if (e->lDown && h == 30) {
        e->volBgm = clampi((e->mx - soundBar.x) * 100 / soundBar.w, 0, 100);
        snd_volumes(e);
    }
    if (e->rclick) {
        snd_play(e, "0", 3, 0, 0);
        close_ui(e);
        return;
    }
    if (!e->click || !h) return;
    if (h >= 1 && h <= 10) sound_play_track(e, h - 1);
    else if (h == 20) sound_play_track(e, u->track);
    else if (h == 21) snd_play(e, "0", 3, 0, 0);
    else if (h == 22) sound_play_track(e, (u->track + 9) % 10);
    else if (h == 23) sound_play_track(e, (u->track + 1) % 10);
    else if (h == 24) {
        sound(e, "SYS_OK");
        snd_play(e, "0", 3, 0, 0);
        config_save(e);
        close_ui(e);
    } else if (h == 25 || h == 26) {
        e->volBgm = clampi(e->volBgm + (h == 25 ? -10 : 10), 0, 100);
        snd_volumes(e);
    }
}

static void sound_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    blit(dst, e->W, e->H, u->bgm.base, 0, 0, 255);
    Image *c = u->bgm.hover;
    Rect cur = soundTracks[u->track];
    blit_part(dst, e->W, e->H, c, cur.x, cur.y, cur.w, cur.h, cur.x, cur.y, 255);
    if (u->hover >= 1 && u->hover <= 10) {
        Rect r = soundTracks[u->hover - 1];
        blit_part(dst, e->W, e->H, c, r.x, r.y, r.w, r.h, r.x, r.y, 255);
    } else if (u->hover >= 20 && u->hover <= 26) {
        Rect r = soundButtons[u->hover - 20];
        blit_part(dst, e->W, e->H, c, r.x, r.y, r.w, r.h, r.x, r.y, 255);
    }
    int w = soundBar.w * e->volBgm / 100;
    blit_part(dst, e->W, e->H, c, soundBar.x, soundBar.y, w, soundBar.h, soundBar.x, soundBar.y, 255);
}

void ui_open_sound(EG *e) {
    UI *u = e->ui;
    sheet_load(e, &u->bgm, "BGMMode");
    if (u->bgm.base) u->bgm.base->alpha = false;
    if (u->bgm.hover) u->bgm.hover->alpha = false;
    u->hover = 0;
    u->track = 0;
    e->uiMode = UI_SOUND;
}

void ui_update(EG *e) {
    UI *u = e->ui;
    switch (e->uiMode) {
    case UI_SAVE:
    case UI_LOAD: update_saveload(e); break;
    case UI_LOG: log_update(e); break;
    case UI_CONFIG: config_update(e); break;
    case UI_MSGBOX: mb_update(e); break;
    case UI_ANIM: {
        int c = mask_color(u->option.mask, e->mx, e->my);
        if (c != u->hover && c) sound(e, "SYS_OV");
        u->hover = c;
        if (e->rclick || (e->click && c == 190)) {
            close_ui(e);
            break;
        }
        if (e->click && c >= 10 && c <= 40) {
            sound(e, "SYS_OK");
            if (c == 10 || c == 20) set_sysvar(e, ":OptionNum1", c == 10);
            else set_sysvar(e, ":OptionNum2", c == 30);
        }
        break;
    }
    case UI_SOUND: sound_update(e); break;
    case UI_CG:
    case UI_SCENE: gallery_update(e); break;
    case UI_BRA: {
        int c = mask_color(u->bra.mask, e->mx, e->my);
        if (c != u->hover && c) sound(e, "SYS_OV");
        u->hover = c;
        if (e->rclick || (e->click && c == 60)) close_ui(e);
        break;
    }
    default: break;
    }
}

static void anim_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    option_draw(e, dst, &u->option, 0);
    int on1 = sysvar(e, ":OptionNum1") ? 10 : 20, on2 = sysvar(e, ":OptionNum2") ? 30 : 40;
    draw_masked(dst, e->W, e->H, u->option.hover, u->option.mask, on1, 0, 0);
    draw_masked(dst, e->W, e->H, u->option.hover, u->option.mask, on2, 0, 0);
    if (u->hover == 190) draw_masked(dst, e->W, e->H, u->option.hover, u->option.mask, 190, 0, 0);
}

static void bra_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    blit(dst, e->W, e->H, u->bra.base, 0, 0, 255);
    static const int colors[5] = {10, 20, 30, 40, 50};
    (void)colors;
    if (u->hover == 60) draw_masked(dst, e->W, e->H, u->bra.hover, u->bra.mask, 60, 0, 0);
}

static void edit_draw(EG *e, u32 *dst) {
    int *r = e->editRect;
    int h = r[3] - r[1];
    int size = h > 44 ? 40 : h - 4;
    if (size < 12) size = 12;
    fill_rect(dst, e->W, e->H, r[0], r[1], r[2] - r[0], h, 0xff000000u);
    int x = text_draw(e, dst, e->W, e->H, e->editBuf, r[0] + 8, r[1] + (h - size) / 2, size, 0, 0xffffffffu, false);
    if ((e->now / 500) % 2 == 0) fill_rect(dst, e->W, e->H, x + 1, r[1] + (h - size) / 2, 2, size, 0xffffffffu);
}

void ui_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    if (e->editing) edit_draw(e, dst);
    switch (e->uiMode) {
    case UI_SAVE:
    case UI_LOAD: draw_saveload(e, dst); break;
    case UI_LOG: log_draw(e, dst); break;
    case UI_CONFIG: config_draw(e, dst); break;
    case UI_MSGBOX: mb_draw(e, dst); break;
    case UI_ANIM: anim_draw(e, dst); break;
    case UI_BRA: bra_draw(e, dst); break;
    case UI_SOUND: sound_draw(e, dst); break;
    case UI_CG:
    case UI_SCENE: gallery_draw(e, dst); break;
    default: break;
    }
    if (u->noticeUntil && (i32)(e->now - u->noticeUntil) < 0) {
        int tw = text_width(u->notice, 16, 0);
        fill_rect(dst, e->W, e->H, e->W - tw - 24, 8, tw + 16, 26, 0xc0000000u);
        text_draw(e, dst, e->W, e->H, u->notice, e->W - tw - 16, 13, 16, 0, 0xffffffffu, false);
    }
}

static bool window_active(EG *e) {
    return e->mesMode && !e->winHidden && e->uiMode == UI_NONE && e->th[0].wait == W_TEXT && e->mesImg[0] && e->mesImg[2];
}

static Image *window_mask(EG *e) {
    UI *u = e->ui;
    if (u->menuOpen && e->mesImg[5]) return e->mesImg[5];
    return e->mesImg[2];
}

static void quick_save(EG *e) {
    if (save_slot(e, EG_SLOTS)) ui_notice(e, "Quick saved");
}

static void quick_load(EG *e) {
    if (save_exists(e, EG_SLOTS)) load_slot(e, EG_SLOTS);
}

static void window_button(EG *e, int c) {
    UI *u = e->ui;
    if (c != 150) u->menuOpen = false;
    switch (c) {
    case 150: u->menuOpen = !u->menuOpen; break;
    case 220: ui_open(e, UI_CONFIG); break;
    case 180: ui_open(e, UI_LOG); break;
    case 80: e->winHidden = true; break;
    case 130: set_sysvar(e, ":SkipFlg", !sysvar(e, ":SkipFlg")); break;
    case 120:
        set_sysvar(e, ":AutoFlg", !sysvar(e, ":AutoFlg"));
        e->autoAt = 0;
        break;
    case 170: quick_save(e); break;
    case 160: quick_load(e); break;
    case 105:
        save_capture_thumb(e);
        ui_open(e, UI_SAVE);
        break;
    case 115: ui_open(e, UI_LOAD); break;
    case 235: ui_message_box(e, MB_TITLE); break;
    case 215: ui_message_box(e, MB_EXIT); break;
    case 245:
        sheet_load(e, &u->option, "Option");
        e->uiMode = UI_ANIM;
        break;
    case 45:
        sheet_load(e, &u->bra, "Option2");
        e->uiMode = UI_BRA;
        break;
    default: eg_log(e, "window button %d does nothing", c); break;
    }
}

void win_update(EG *e) {
    UI *u = e->ui;
    e->winHover = 0;
    if (!window_active(e)) {
        u->menuOpen = false;
        return;
    }
    int c = mask_color(window_mask(e), e->mx - e->winRect[0], e->my - e->winRect[1]);
    if (c != u->menuHover && c) sound(e, "SYS_OV");
    u->menuHover = c;
    e->winHover = c;
    if (e->click && c) {
        e->click = false;
        sound(e, "SYS_OK");
        window_button(e, c);
        return;
    }
    if (e->click && u->menuOpen) {
        e->click = false;
        u->menuOpen = false;
    }
    if (e->wheel > 0 && e->logCount > 1) {
        e->wheel = 0;
        ui_open(e, UI_LOG);
    }
}

void win_draw(EG *e, u32 *dst) {
    UI *u = e->ui;
    if (!window_active(e)) return;
    int ox = e->winRect[0], oy = e->winRect[1];
    bool popupItem = u->menuOpen && (u->menuHover == 45 || u->menuHover == 95 || u->menuHover == 105 || u->menuHover == 115 ||
                                      u->menuHover == 215 || u->menuHover == 235 || u->menuHover == 245);
    if (u->menuHover && !popupItem) draw_masked(dst, e->W, e->H, e->mesImg[1], window_mask(e), u->menuHover, ox, oy);
    if (u->menuOpen && e->mesImg[3]) {
        int m[6], mc[6];
        if (ini_ints(&e->ini, "MenuSystem", m, 6) == 6) blit_part(dst, e->W, e->H, e->mesImg[3], m[4], m[5], m[2], m[3], m[0], m[1], 255);
        if (u->menuHover && ini_ints(&e->ini, "MenuSystemC", mc, 6) == 6) {
            Image *mask = window_mask(e);
            for (int y = 0; y < mc[3]; y++)
                for (int x = 0; x < mc[2]; x++) {
                    int sx = mc[0] + x - ox, sy = mc[1] + y - oy;
                    if (mask_color(mask, sx, sy) != u->menuHover) continue;
                    int px = mc[4] + x, py = mc[5] + y;
                    Image *p = e->mesImg[3];
                    if (px >= p->w || py >= p->h) continue;
                    u32 s = p->px[(size_t)py * p->w + px];
                    if (!(s >> 24)) continue;
                    int dx = mc[0] + x, dy = mc[1] + y;
                    if (dx < e->W && dy < e->H) dst[(size_t)dy * e->W + dx] = s | 0xff000000u;
                }
        }
    }
}
