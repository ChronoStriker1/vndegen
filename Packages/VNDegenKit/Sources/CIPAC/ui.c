#include "ipac.h"

#define SLOTS_PER_PAGE 20
#define PAGES 5
#define QUICK_SLOT 99

typedef struct Rect {
    int x, y, w, h;
} Rect;

struct UI {
    Image *sheet, *map;
    int hover;
    int page;
    int confirm;
    int confirmHover;
    int slot;
    int cfgPage;
    int drag;
    int logTop;
    int prevMode;
};

static u32 rgb_at(const Image *m, int x, int y) {
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    u32 p = m->px[(size_t)y * m->w + x];
    return (p & 0xff) << 16 | (p & 0xff00) | (p >> 16 & 0xff);
}

static bool find_rect(const Image *a, u32 rgb, int y0, int y1, Rect *r) {
    if (!a) return false;
    u32 want = (rgb >> 16 & 0xff) | (rgb & 0xff00) | (rgb & 0xff) << 16;
    int minx = a->w, miny = a->h, maxx = -1, maxy = -1;
    if (y0 < 0) y0 = 0;
    if (y1 > a->h) y1 = a->h;
    for (int j = y0; j < y1; j++)
        for (int i = 0; i < a->w; i++)
            if ((a->px[(size_t)j * a->w + i] & 0xffffff) == want) {
                if (i < minx) minx = i;
                if (i > maxx) maxx = i;
                if (j < miny) miny = j;
                if (j > maxy) maxy = j;
            }
    if (maxx < 0) return false;
    *r = (Rect){minx, miny, maxx - minx + 1, maxy - miny + 1};
    return true;
}

static bool in_rect(Rect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

static void ui_close(IP *t) {
    if (t->ui) {
        image_free(t->ui->sheet);
        image_free(t->ui->map);
        free(t->ui);
        t->ui = NULL;
    }
    t->uiMode = UI_NONE;
    t->dirty = true;
}

void ui_init(IP *t) { (void)t; }

void ui_free(IP *t) { ui_close(t); }

void ui_open(IP *t, int mode) {
    ui_close(t);
    UI *u = ip_alloc(sizeof *u);
    u->hover = -1;
    u->confirm = 0;
    u->confirmHover = 0;
    const char *sheet = NULL, *map = NULL;
    switch (mode) {
    case UI_SAVE:
    case UI_LOAD: sheet = "Data", map = "DataA"; break;
    case UI_CONFIG: sheet = "config", map = "configA"; break;
    case UI_LOG: sheet = "rireki"; break;
    case UI_EXIT: sheet = "SysEnd", map = "SysEndA"; break;
    default: break;
    }
    if (sheet) u->sheet = image_load(t, PK_PARTS, sheet);
    if (map) u->map = image_load(t, PK_PARTS, map);
    if (!u->sheet && mode != UI_TITLE_CONFIRM) {
        free(u);
        if (mode == UI_EXIT) t->quit = true;
        return;
    }
    if (mode == UI_LOG) u->logTop = t->logCount > 20 ? t->logCount - 20 : 0;
    t->ui = u;
    t->uiMode = mode;
    t->dirty = true;
}

static Rect slot_rect(IP *t, int i) {
    Rect c = {142, 100, 253, 40};
    find_rect(t->ui->map, 0x00c800, 0, 600, &c);
    return (Rect){c.x + (i % 2) * 254, c.y + (i / 2) * 40, c.w - 5, c.h - 3};
}

static void draw_glyphs(IP *t, const Image *sheet, const char *s, int x, int y) {
    for (; *s; s++) {
        int sx, w;
        if (*s >= '0' && *s <= '9') sx = 8 + (*s - '0') * 10, w = 10;
        else if (*s == ':') sx = 109, w = 10;
        else if (*s == 'd') sx = 119, w = 11;
        else if (*s == 'm') sx = 130, w = 12;
        else {
            x += 6;
            continue;
        }
        blit_part(t->screen, t->W, t->H, sheet, sx, 737, w, 16, x, y, 255);
        x += w;
    }
}

static void draw_slots(IP *t) {
    UI *u = t->ui;
    blit_part(t->screen, t->W, t->H, u->sheet, 0, 0, 800, 600, 0, 0, 255);
    for (int i = 0; i < SLOTS_PER_PAGE; i++) {
        int slot = u->page * SLOTS_PER_PAGE + (i % 2) * 10 + i / 2;
        Rect r = slot_rect(t, i);
        if (i == u->hover && !u->confirm) fill_rect(t->screen, t->W, t->H, r.x, r.y + 1, r.w, r.h, 0xff7e41d7u);
        if (slot == QUICK_SLOT) blit_part(t->screen, t->W, t->H, u->sheet, 0, 600, 251, 38, r.x, r.y + 1, 255);
        SlotInfo info;
        if (!save_info(t, slot, &info)) continue;
        fill_rect(t->screen, t->W, t->H, r.x, r.y + 1, 193, 15, 0xa0301818u);
        int n;
        u32 *cps = text_decode(info.scene, &n);
        text_line(t->screen, t->W, t->H, cps, n, r.x + 2, r.y + 2, 12, 13, 0xffffffffu, 0);
        free(cps);
        char date[32];
        snprintf(date, sizeof date, "%dm%dd %d:%02d", info.month, info.day, info.hour, info.minute);
        draw_glyphs(t, u->sheet, date, r.x + 116, r.y + 21);
    }
    int d = u->page + 1;
    blit_part(t->screen, t->W, t->H, u->sheet, (d - 1) * 16, 763, 16, 26, 373, 522, 255);
    if (u->confirm) {
        int src = t->uiMode == UI_SAVE ? 623 : 835;
        int dx = 418, dy = 378;
        blit_part(t->screen, t->W, t->H, u->sheet, 405, src, 364, 164, dx, dy, 255);
        if (u->confirmHover == 1) blit_part(t->screen, t->W, t->H, u->sheet, 4, 670, 79, 38, dx + 51, dy + 96, 255);
        if (u->confirmHover == 2) blit_part(t->screen, t->W, t->H, u->sheet, 94, 670, 79, 38, dx + 141, dy + 96, 255);
    }
}

static void slots_update(IP *t) {
    UI *u = t->ui;
    if (u->confirm) {
        int dx = 418, dy = 378, h = 0;
        if (in_rect((Rect){dx + 51, dy + 96, 79, 38}, t->mx, t->my)) h = 1;
        else if (in_rect((Rect){dx + 141, dy + 96, 79, 38}, t->mx, t->my)) h = 2;
        if (h != u->confirmHover) {
            u->confirmHover = h;
            t->dirty = true;
        }
        if (t->rclick) {
            t->rclick = false;
            u->confirm = 0;
            t->dirty = true;
        }
        if (!t->click) return;
        t->click = false;
        if (h == 2) {
            u->confirm = 0;
            t->dirty = true;
        } else if (h == 1) {
            int slot = u->slot, mode = t->uiMode;
            if (mode == UI_SAVE) {
                save_slot(t, slot);
                u->confirm = 0;
                t->dirty = true;
            } else {
                ui_close(t);
                load_slot(t, slot);
            }
        }
        return;
    }
    int hover = -1;
    for (int i = 0; i < SLOTS_PER_PAGE; i++)
        if (in_rect(slot_rect(t, i), t->mx, t->my)) hover = i;
    if (hover != u->hover) {
        u->hover = hover;
        t->dirty = true;
    }
    if (t->rclick) {
        t->rclick = false;
        ui_close(t);
        return;
    }
    if (t->wheel) {
        u->page = (u->page + (t->wheel < 0 ? 1 : PAGES - 1)) % PAGES;
        t->wheel = 0;
        t->dirty = true;
    }
    if (!t->click) return;
    t->click = false;
    Rect left = {244, 521, 60, 29}, right = {483, 521, 60, 29};
    find_rect(u->map, 0x00fa00, 0, 600, &left);
    find_rect(u->map, 0x00fb00, 0, 600, &right);
    if (in_rect(left, t->mx, t->my)) {
        u->page = (u->page + PAGES - 1) % PAGES;
        t->dirty = true;
        return;
    }
    if (in_rect(right, t->mx, t->my)) {
        u->page = (u->page + 1) % PAGES;
        t->dirty = true;
        return;
    }
    if (hover < 0) return;
    int slot = u->page * SLOTS_PER_PAGE + (hover % 2) * 10 + hover / 2;
    if (t->uiMode == UI_LOAD && !save_exists(t, slot)) return;
    u->slot = slot;
    u->confirm = 1;
    u->confirmHover = 0;
    t->dirty = true;
}

typedef struct CfgItem {
    int id;
    int kind;
} CfgItem;

enum { CK_TOGGLE, CK_SLIDER, CK_BUTTON };

static int *slider_value(IP *t, int id) {
    switch (id) {
    case 0xb4: return &t->winAlpha;
    case 0x70: return &t->speedSlider;
    case 0x84: return &t->autoSlider;
    case 0x98: return &t->volBgm;
    case 0xa2: return &t->volSe;
    case 0xac: return &t->volVoice;
    default: return NULL;
    }
}

static bool toggle_on(IP *t, int id) {
    switch (id) {
    case 0x64: return true;
    case 0x65: return false;
    case 0x79: return t->skipUnread;
    case 0x78: return !t->skipUnread;
    case 0x83: return t->autoMode;
    case 0x82: return !t->autoMode;
    case 0x8d: return t->voiceWait;
    case 0x8c: return !t->voiceWait;
    case 0x97: return t->bgmOn;
    case 0x96: return !t->bgmOn;
    case 0xa1: return t->seOn;
    case 0xa0: return !t->seOn;
    case 0xab: return t->voiceOn;
    case 0xaa: return !t->voiceOn;
    default: return false;
    }
}

static void toggle_set(IP *t, int id) {
    switch (id) {
    case 0x79: t->skipUnread = true; break;
    case 0x78: t->skipUnread = false; break;
    case 0x83: t->autoMode = true; break;
    case 0x82: t->autoMode = false; break;
    case 0x8d: t->voiceWait = true; break;
    case 0x8c: t->voiceWait = false; break;
    case 0x97: t->bgmOn = true; break;
    case 0x96: t->bgmOn = false; break;
    case 0xa1: t->seOn = true; break;
    case 0xa0: t->seOn = false; break;
    case 0xab: t->voiceOn = true; break;
    case 0xaa: t->voiceOn = false; break;
    default: break;
    }
}

static const int toggles[] = {0x64, 0x65, 0x79, 0x78, 0x83, 0x82, 0x8d, 0x8c, 0x97, 0x96, 0xa1, 0xa0, 0xab, 0xaa};
static const int sliders[] = {0xb4, 0x70, 0x84, 0x98, 0xa2, 0xac};

static void config_draw(IP *t) {
    UI *u = t->ui;
    int py = u->cfgPage ? 800 : 0;
    blit_part(t->screen, t->W, t->H, u->sheet, 0, py, 800, 600, 0, 0, 255);
    if (u->cfgPage) return;
    for (size_t i = 0; i < sizeof toggles / sizeof *toggles; i++) {
        Rect dst, src;
        if (!toggle_on(t, toggles[i])) continue;
        if (!find_rect(u->map, (u32)toggles[i] << 8, 0, 600, &dst) || !find_rect(u->map, (u32)toggles[i], 600, 800, &src)) continue;
        blit_part(t->screen, t->W, t->H, u->sheet, src.x, src.y, src.w, src.h, dst.x, dst.y, 255);
    }
    Rect knob = {14, 747, 9, 30};
    find_rect(u->map, 0x0000ff, 600, 800, &knob);
    for (size_t i = 0; i < sizeof sliders / sizeof *sliders; i++) {
        Rect tr;
        int *v = slider_value(t, sliders[i]);
        if (!v || !find_rect(u->map, (u32)sliders[i] << 8, 0, 600, &tr)) continue;
        int x = tr.x + *v * (tr.w - knob.w) / 255;
        blit_part(t->screen, t->W, t->H, u->sheet, knob.x, knob.y, knob.w, knob.h, x, tr.y + (tr.h - knob.h) / 2, 255);
    }
}

static void slider_set(IP *t, int id, Rect tr) {
    int *v = slider_value(t, id);
    if (!v) return;
    *v = clampi((t->mx - tr.x - 4) * 255 / (tr.w - 9), 0, 255);
    t->dirty = true;
}

static void config_apply(IP *t) {
    t->textSpeed = 32 - t->speedSlider * 32 / 255;
    snd_volumes(t);
    save_system(t);
}

static void config_update(IP *t) {
    UI *u = t->ui;
    if (t->rclick) {
        t->rclick = false;
        config_apply(t);
        ui_close(t);
        return;
    }
    if (u->drag && t->lDown) {
        Rect tr;
        if (find_rect(u->map, (u32)u->drag << 8, 0, 600, &tr)) slider_set(t, u->drag, tr);
        return;
    }
    if (u->drag && !t->lDown) {
        u->drag = 0;
        config_apply(t);
    }
    if (!t->click && !(t->lDown && !u->drag)) return;
    u32 c = rgb_at(u->map, t->mx, t->my + (u->cfgPage ? 800 : 0));
    int id = (int)(c >> 8 & 0xff);
    if (u->cfgPage) {
        if (!t->click) return;
        t->click = false;
        if ((c & 0xffff00) == 0x00fb00 || c == 0x00fb01) {
            u->cfgPage = 0;
            t->dirty = true;
        } else if (c == 0x00fa01) {
            config_apply(t);
            ui_close(t);
        }
        return;
    }
    for (size_t i = 0; i < sizeof sliders / sizeof *sliders; i++)
        if (c == (u32)sliders[i] << 8 && t->lDown) {
            u->drag = sliders[i];
            return;
        }
    if (!t->click) return;
    t->click = false;
    if (c == 0x00fa00) {
        config_apply(t);
        ui_close(t);
    } else if (c == 0x00ff00) {
        t->volBgm = t->volSe = t->volVoice = 128;
        config_apply(t);
        t->dirty = true;
    } else if (c == 0x00fc00) {
        u->cfgPage = 1;
        t->dirty = true;
    } else if (c == 0xfffe00) {
        config_apply(t);
        ui_close(t);
        if (t->inGame) ui_open(t, UI_TITLE_CONFIRM);
    } else if (c == 0xffff00) {
        config_apply(t);
        ui_open(t, UI_EXIT);
    } else if ((c & 0xff00ff) == 0 && id) {
        toggle_set(t, id);
        config_apply(t);
        t->dirty = true;
    }
}

static void log_draw(IP *t) {
    UI *u = t->ui;
    fill_rect(t->screen, t->W, t->H, 0, 0, t->W, t->H, 0xa0000000u);
    int y = 34;
    for (int i = u->logTop; i < t->logCount && y < 580; i++) {
        LogEntry *e = &t->logs[i];
        int lines = 0;
        char *joined = e->name ? ip_fmt("%s%s", e->name, e->text) : ip_fmt("%s", e->text);
        int n;
        u32 *cps = text_decode(joined, &n);
        free(joined);
        int x0 = e->name ? 28 : 68, x = x0;
        bool ruby = false;
        for (int k = 0; k < n && y < 580; k++) {
            u32 cp = cps[k];
            if (cp == '~') continue;
            if (cp == '$') {
                ruby = !ruby;
                continue;
            }
            if (ruby) continue;
            if (cp == '\n' || x > 740) {
                y += 27;
                lines++;
                x = 68;
                if (cp == '\n') continue;
            }
            text_glyph(t->screen, t->W, t->H, cp, x, y, 16, 0xffffffffu, 0xc0000000u);
            x += text_wide(cp) ? 19 : 10;
        }
        free(cps);
        y += 27;
    }
    blit_part(t->screen, t->W, t->H, u->sheet, 0, 0, 30, 600, 770, 0, 255);
}

static void log_update(IP *t) {
    UI *u = t->ui;
    if (t->rclick || t->click) {
        t->rclick = t->click = false;
        ui_close(t);
        return;
    }
    if (t->wheel) {
        u->logTop = clampi(u->logTop - t->wheel, 0, t->logCount > 0 ? t->logCount - 1 : 0);
        if (t->wheel < 0 && u->logTop >= t->logCount - 20 + 1) {
            t->wheel = 0;
            ui_close(t);
            return;
        }
        t->wheel = 0;
        t->dirty = true;
    }
}

static void exit_update(IP *t) {
    UI *u = t->ui;
    int x = (t->W - u->sheet->w) / 2, y = (t->H - 164) / 2;
    u32 c = rgb_at(u->map, t->mx - x, t->my - y);
    int hover = (c & 0xff00ff) == 0 && (c >> 8 & 0xff) >= 0xfa ? (int)(c >> 8 & 0xff) : 0;
    if (hover != u->hover) {
        u->hover = hover;
        t->dirty = true;
    }
    if (t->click) {
        t->click = false;
        if (hover == 0xfa) {
            save_system(t);
            t->quit = true;
            ui_close(t);
        } else if (hover == 0xfb) ui_close(t);
    }
    if (t->rclick) {
        t->rclick = false;
        ui_close(t);
    }
}

static void exit_draw(IP *t) {
    UI *u = t->ui;
    int x = (t->W - u->sheet->w) / 2, y = (t->H - 164) / 2;
    blit_part(t->screen, t->W, t->H, u->sheet, 0, 0, u->sheet->w, 164, x, y, 255);
    if (u->hover == 0xfa) blit_part(t->screen, t->W, t->H, u->sheet, 0, 171, 112, 28, x + 21, y + 106, 255);
    if (u->hover == 0xfb) blit_part(t->screen, t->W, t->H, u->sheet, 119, 171, 112, 28, x + 140, y + 106, 255);
}

static Rect confirm_yes(IP *t) { return (Rect){t->W / 2 - 110, t->H / 2 + 10, 100, 32}; }
static Rect confirm_no(IP *t) { return (Rect){t->W / 2 + 10, t->H / 2 + 10, 100, 32}; }

static void title_confirm_draw(IP *t) {
    fill_rect(t->screen, t->W, t->H, t->W / 2 - 160, t->H / 2 - 60, 320, 120, 0xe0402820u);
    char *msg = ip_utf8_to_sjis("タイトルに戻りますか？");
    int n;
    u32 *cps = text_decode(msg, &n);
    free(msg);
    text_line(t->screen, t->W, t->H, cps, n, t->W / 2 - n * 10, t->H / 2 - 40, 16, 20, 0xffffffffu, 0xc0000000u);
    free(cps);
    Rect y = confirm_yes(t), no = confirm_no(t);
    UI *u = t->ui;
    fill_rect(t->screen, t->W, t->H, y.x, y.y, y.w, y.h, u->hover == 1 ? 0xff7e41d7u : 0xff906050u);
    fill_rect(t->screen, t->W, t->H, no.x, no.y, no.w, no.h, u->hover == 2 ? 0xff7e41d7u : 0xff906050u);
    static const u32 yes[] = {'Y', 'E', 'S'}, nn[] = {'N', 'O'};
    text_line(t->screen, t->W, t->H, yes, 3, y.x + 32, y.y + 8, 16, 12, 0xffffffffu, 0);
    text_line(t->screen, t->W, t->H, nn, 2, no.x + 38, no.y + 8, 16, 12, 0xffffffffu, 0);
}

static void title_confirm_update(IP *t) {
    UI *u = t->ui;
    int h = in_rect(confirm_yes(t), t->mx, t->my) ? 1 : in_rect(confirm_no(t), t->mx, t->my) ? 2 : 0;
    if (h != u->hover) {
        u->hover = h;
        t->dirty = true;
    }
    if (t->rclick) {
        t->rclick = false;
        ui_close(t);
        return;
    }
    if (!t->click) return;
    t->click = false;
    if (h == 2) ui_close(t);
    else if (h == 1) {
        ui_close(t);
        game_to_title(t);
    }
}

void ui_update(IP *t) {
    if (!t->ui) return;
    switch (t->uiMode) {
    case UI_SAVE:
    case UI_LOAD: slots_update(t); break;
    case UI_CONFIG: config_update(t); break;
    case UI_LOG: log_update(t); break;
    case UI_EXIT: exit_update(t); break;
    case UI_TITLE_CONFIRM: title_confirm_update(t); break;
    default: break;
    }
}

void ui_draw(IP *t) {
    if (!t->ui) return;
    switch (t->uiMode) {
    case UI_SAVE:
    case UI_LOAD: draw_slots(t); break;
    case UI_CONFIG: config_draw(t); break;
    case UI_LOG: log_draw(t); break;
    case UI_EXIT: exit_draw(t); break;
    case UI_TITLE_CONFIRM: title_confirm_draw(t); break;
    default: break;
    }
}
