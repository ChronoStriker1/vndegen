#include "ex.h"

#include <time.h>

enum { UI_NONE, UI_SYSMENU, UI_SAVE, UI_LOAD, UI_BACKLOG };

#define UI_LAYERS 16
#define UI_ITEMS 64
#define ROWS 6
#define ROW_H 153

typedef struct KV {
    char *k, *v;
} KV;

typedef struct ULayer {
    int id, kind, x, y, w, h;
    Image *img;
} ULayer;

typedef struct UItem {
    int id, layer, x, y, states, action, arg;
    Image *img;
} UItem;

struct Ui {
    int kind;
    KV *kv;
    int nkv;
    ULayer layers[UI_LAYERS];
    int nl;
    UItem items[UI_ITEMS];
    int ni;
    int hover, pressed;
    int page, pages, pageStart[16], pageCount[16];
    int top;
    Image *mark, *voiceMark;
    int newest;
    bool fromTitle;
    bool dragging;
};

static void kv_free(Ui *u) {
    for (int i = 0; i < u->nkv; i++) {
        free(u->kv[i].k);
        free(u->kv[i].v);
    }
    free(u->kv);
    u->kv = NULL;
    u->nkv = 0;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
    return s;
}

static void rnf_load(EX *e, Ui *u, const char *rel) {
    char *path = ex_game_path(e, rel);
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    if (!d) {
        ex_log(e, "missing layout %s", rel);
        return;
    }
    char *text = ex_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        char *c = strchr(line, ';');
        if (c) *c = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(line), *v = trim(eq + 1);
        if (!*k) continue;
        u->kv = ex_realloc(u->kv, sizeof(KV) * (size_t)(u->nkv + 1));
        u->kv[u->nkv++] = (KV){ex_strdup(k), ex_strdup(v)};
    }
    free(text);
}

static const char *get(Ui *u, const char *key) {
    for (int i = u->nkv - 1; i >= 0; i--)
        if (!strcmp(u->kv[i].k, key)) return u->kv[i].v;
    return NULL;
}

static int get_int(Ui *u, const char *key, int def) {
    const char *v = get(u, key);
    return v ? (int)strtol(v, NULL, 0) : def;
}

static ULayer *layer(Ui *u, int id) {
    for (int i = 0; i < u->nl; i++)
        if (u->layers[i].id == id) return &u->layers[i];
    return NULL;
}

static UItem *item(Ui *u, int id) {
    for (int i = 0; i < u->ni; i++)
        if (u->items[i].id == id) return &u->items[i];
    return NULL;
}

static void build_from_rnf(EX *e, Ui *u) {
    for (int i = 0; i < u->nkv; i++) {
        char *k = u->kv[i].k;
        if (strncmp(k, "layer.", 6)) continue;
        char *rest;
        int lid = (int)strtol(k + 6, &rest, 10);
        if (!strcmp(rest, ".create") && u->nl < UI_LAYERS) {
            char buf[512];
            snprintf(buf, sizeof buf, "%s", u->kv[i].v);
            char *f[16];
            int n = split_csv(buf, f, 16);
            ULayer *l = &u->layers[u->nl++];
            memset(l, 0, sizeof *l);
            l->id = lid;
            l->kind = field_int(f[0]);
            l->x = n > 1 ? field_int(f[1]) : 0;
            l->y = n > 2 ? field_int(f[2]) : 0;
            if (l->kind == 1 && n > 3) l->img = image_load(e, trim(f[3]));
            else if (n > 4) {
                l->w = field_int(f[3]);
                l->h = field_int(f[4]);
            }
        } else if (!strncmp(rest, ".item.", 6)) {
            char *r2;
            int iid = (int)strtol(rest + 6, &r2, 10);
            UItem *it = item(u, iid);
            if (!it && u->ni < UI_ITEMS) {
                it = &u->items[u->ni++];
                memset(it, 0, sizeof *it);
                it->id = iid;
                it->layer = lid;
                it->states = 3;
            }
            if (!it) continue;
            char buf[512];
            snprintf(buf, sizeof buf, "%s", u->kv[i].v);
            char *f[16];
            int n = split_csv(buf, f, 16);
            if (!strcmp(r2, ".create") && n >= 10) {
                it->x = field_int(f[4]);
                it->y = field_int(f[5]);
                it->img = image_load(e, trim(f[6]));
                it->states = 3 * (field_int(f[9]) > 0 ? field_int(f[9]) : 1);
            } else if (!strcmp(r2, ".action") && n >= 2) {
                it->action = field_int(f[0]);
                it->arg = field_int(f[1]);
            }
        }
    }
}

static void free_ui(Ui *u) {
    for (int i = 0; i < u->nl; i++) image_free(u->layers[i].img);
    for (int i = 0; i < u->ni; i++) image_free(u->items[i].img);
    image_free(u->mark);
    image_free(u->voiceMark);
    kv_free(u);
    free(u);
}

static int cell_w(const UItem *it) { return it->img ? it->img->w / (it->states > 0 ? it->states : 1) : 0; }

static void item_pos(Ui *u, const UItem *it, int *x, int *y) {
    ULayer *l = layer(u, it->layer);
    *x = it->x + (l ? l->x : 0);
    *y = it->y + (l ? l->y : 0);
}

static bool hit(EX *e, Ui *u, const UItem *it) {
    if (!it->img) return false;
    int x, y;
    item_pos(u, it, &x, &y);
    return e->mouseX >= x && e->mouseX < x + cell_w(it) && e->mouseY >= y && e->mouseY < y + it->img->h;
}

static void draw_item(EX *e, Ui *u, u32 *dst, const UItem *it, int state) {
    (void)e;
    if (!it->img) return;
    int x, y, w = cell_w(it);
    item_pos(u, it, &x, &y);
    if (state >= it->states) state = 0;
    blit_part(dst, EX_W, EX_H, it->img, state * w, 0, w, it->img->h, x, y, 255);
}

static void open_sysmenu(EX *e, Ui *u) {
    const char *spec = e->sysmenuSpec;
    if (!spec) return;
    char *buf = ex_strdup(spec);
    char *f[128];
    int n = split_csv(buf, f, 128);
    if (n > 37) {
        ULayer *bg = &u->layers[u->nl++];
        memset(bg, 0, sizeof *bg);
        bg->id = 1;
        bg->kind = 1;
        bg->img = image_load(e, f[32]);
        bg->x = field_int(f[33]);
        bg->y = field_int(f[34]);
        ULayer *panel = &u->layers[u->nl++];
        memset(panel, 0, sizeof *panel);
        panel->id = 2;
        panel->kind = 1;
        panel->img = image_load(e, f[35]);
        panel->x = field_int(f[36]);
        panel->y = field_int(f[37]);
        for (int i = 42; i + 4 < n && u->ni < UI_ITEMS; i += 7) {
            int id = field_int(f[i]);
            if (id < 0) break;
            UItem *it = &u->items[u->ni++];
            memset(it, 0, sizeof *it);
            it->id = id;
            it->layer = 2;
            it->img = image_load(e, f[i + 1]);
            it->x = field_int(f[i + 2]);
            it->y = field_int(f[i + 3]);
            it->action = field_int(f[i + 4]);
            it->states = 3;
        }
    }
    free(buf);
}

static void open_slots(EX *e, Ui *u, bool save) {
    rnf_load(e, u, save ? "res/c/save.layout.rnf" : "res/c/load.layout.rnf");
    build_from_rnf(e, u);
    u->pages = get_int(u, "pages", 10);
    if (u->pages > 16) u->pages = 16;
    for (int p = 0; p < u->pages; p++) {
        char key[32];
        snprintf(key, sizeof key, "page.%d", p);
        const char *v = get(u, key);
        u->pageStart[p] = p * 9;
        u->pageCount[p] = 9;
        if (v) {
            char buf[64];
            snprintf(buf, sizeof buf, "%s", v);
            char *f[4];
            if (split_csv(buf, f, 4) >= 2) {
                u->pageStart[p] = field_int(f[0]);
                u->pageCount[p] = field_int(f[1]);
            }
        }
    }
    ULayer *ml = layer(u, 854);
    if (ml) {
        u->mark = ml->img;
        ml->img = NULL;
    }
    u->newest = -1;
    long long best = 0;
    for (int i = 0; i < EX_SLOTS; i++)
        if (e->slots[i].used && e->slots[i].time >= best) {
            best = e->slots[i].time;
            u->newest = i;
        }
    u->page = e->lastPage >= 0 && e->lastPage < u->pages ? e->lastPage : 0;
}

static void open_backlog(EX *e, Ui *u) {
    rnf_load(e, u, "res/c/backlog.layout2.rnf");
    build_from_rnf(e, u);
    u->voiceMark = image_load(e, "res/g/sy/01/1920_197.gyu");
    int total = e->histCount - (e->msg.active ? 1 : 0);
    u->top = total - ROWS;
    if (u->top < 0) u->top = 0;
    for (int i = 0; i < u->ni; i++)
        if (u->items[i].layer == 851) {
            image_free(u->items[i].img);
            u->items[i].img = NULL;
        }
}

bool ui_active(EX *e) { return e->ui != NULL; }

void ui_close(EX *e) {
    if (!e->ui) return;
    free_ui(e->ui);
    e->ui = NULL;
}

static void capture_shot(EX *e) {
    if (!e->saveShot) e->saveShot = ex_alloc(sizeof(u32) * EX_W * EX_H);
    memcpy(e->saveShot, e->screen, sizeof(u32) * EX_W * EX_H);
}

static bool resume_latest(EX *e) {
    int best = -1;
    long long t = 0;
    for (int i = 0; i < EX_SLOTS; i++)
        if (e->slots[i].used && e->slots[i].time >= t) {
            t = e->slots[i].time;
            best = i;
        }
    return best >= 0 && load_slot(e, best);
}

bool ui_open(EX *e, int action) {
    int kind = UI_NONE;
    switch (action) {
    case 102:
    case 3:
        kind = UI_SAVE;
        break;
    case 103:
    case 7:
        kind = UI_LOAD;
        break;
    case 105:
        kind = UI_BACKLOG;
        break;
    case 135:
        kind = UI_SYSMENU;
        break;
    case 179:
        return resume_latest(e);
    case 111:
        capture_shot(e);
        return save_slot(e, 99);
    case 112:
        return e->slots[99].used && load_slot(e, 99);
    case 1:
        ui_close(e);
        vm_goto_script(e, "title");
        e->wait = W_NONE;
        reset_for_title(e);
        return true;
    case 121:
        e->quit = true;
        return true;
    default:
        ex_log(e, "menu action %d", action);
        return false;
    }
    bool fromTitle = e->view.active;
    if (kind == UI_SAVE && fromTitle) return false;
    if (!e->ui) capture_shot(e);
    if (kind == UI_BACKLOG && e->histCount - (e->msg.active ? 1 : 0) <= 0) return false;
    ui_close(e);
    Ui *u = ex_alloc(sizeof *u);
    u->kind = kind;
    u->hover = -1;
    u->pressed = -1;
    u->fromTitle = fromTitle;
    e->ui = u;
    if (kind == UI_SYSMENU) open_sysmenu(e, u);
    else if (kind == UI_SAVE || kind == UI_LOAD) open_slots(e, u, kind == UI_SAVE);
    else if (kind == UI_BACKLOG) open_backlog(e, u);
    e->lPress = e->rPress = false;
    return true;
}

static void slot_click(EX *e, Ui *u, int itemId) {
    int k = itemId - 10;
    if (k < 0 || k >= u->pageCount[u->page]) return;
    int slot = u->pageStart[u->page] + k;
    if (slot < 0 || slot >= EX_SLOTS) return;
    snd_play_file(e, "res/s/e/990.wav", EX_CH_UI);
    if (u->kind == UI_SAVE) {
        if (save_slot(e, slot)) u->newest = slot;
        e->lastPage = u->page;
        return;
    }
    if (!e->slots[slot].used) return;
    e->lastPage = u->page;
    ui_close(e);
    if (!load_slot(e, slot)) ex_log(e, "can't load slot %d", slot);
}

static void backlog_scroll(EX *e, Ui *u, int delta) {
    int total = e->histCount - (e->msg.active ? 1 : 0);
    int maxTop = total - ROWS;
    if (maxTop < 0) maxTop = 0;
    u->top += delta;
    if (u->top < 0) u->top = 0;
    if (u->top > maxTop) u->top = maxTop;
}

static HistEntry *hist(EX *e, int i) {
    if (i < 0 || i >= e->histCount) return NULL;
    return &e->history[(e->histHead + i) % EX_HISTORY];
}

void ui_input(EX *e) {
    Ui *u = e->ui;
    if (!u) return;
    int hover = -1;
    for (int i = 0; i < u->ni; i++)
        if (hit(e, u, &u->items[i])) hover = u->items[i].id;
    if (hover != u->hover && hover >= 0) snd_play_file(e, "res/s/e/993.wav", EX_CH_UI);
    u->hover = hover;
    int wheel = e->wheel;
    e->wheel = 0;
    if (u->kind == UI_BACKLOG) {
        if (wheel) {
            int total = e->histCount - (e->msg.active ? 1 : 0);
            if (wheel < 0 && u->top >= total - ROWS) {
                ui_close(e);
                return;
            }
            backlog_scroll(e, u, -wheel);
        }
        if (e->lPress) {
            e->lPress = false;
            switch (hover) {
            case 5: backlog_scroll(e, u, -ROWS); break;
            case 6: backlog_scroll(e, u, -1); break;
            case 8: backlog_scroll(e, u, 1); break;
            case 9: backlog_scroll(e, u, ROWS); break;
            default:
                for (int r = 0; r < ROWS; r++) {
                    HistEntry *h = hist(e, u->top + r);
                    int y = 81 + r * ROW_H;
                    if (h && h->voice >= 0 && e->mouseX >= 527 && e->mouseX < 567 && e->mouseY >= y + 3 && e->mouseY < y + 43) snd_play_voice(e, h->voice);
                }
                break;
            }
        }
        if (e->rPress) {
            e->rPress = false;
            ui_close(e);
        }
        return;
    }
    if (e->rPress) {
        e->rPress = false;
        ui_close(e);
        return;
    }
    if (u->kind == UI_SYSMENU && wheel > 0) {
        ui_open(e, 105);
        return;
    }
    if (!e->lPress) return;
    e->lPress = false;
    if (hover < 0) {
        if (u->kind == UI_SYSMENU) ui_close(e);
        return;
    }
    UItem *it = item(u, hover);
    if (u->kind == UI_SYSMENU) {
        snd_play_file(e, "res/s/e/990.wav", EX_CH_UI);
        int action = it->action;
        if (action == 3 || action == 7 || action == 105) ui_open(e, action);
        else {
            ui_close(e);
            ui_open(e, action);
        }
        return;
    }
    if (hover >= 10 && hover <= 18) slot_click(e, u, hover);
    else if (hover >= 30 && hover < 30 + u->pages) {
        u->page = hover - 30;
        snd_play_file(e, "res/s/e/990.wav", EX_CH_UI);
    } else if (hover == 98) {
        bool fromTitle = u->fromTitle;
        int page = u->page;
        if (!(u->kind == UI_LOAD && fromTitle)) {
            ui_open(e, u->kind == UI_SAVE ? 103 : 102);
            if (e->ui) e->ui->page = page;
        }
    } else if (hover == 99) ui_close(e);
}

static void fmt_time(long long t, char *out, size_t n, bool twoLines) {
    time_t tt = (time_t)t;
    struct tm tm;
    localtime_r(&tt, &tm);
    if (twoLines) snprintf(out, n, "%04d/%02d/%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    else snprintf(out, n, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static void draw_slots(EX *e, Ui *u, u32 *dst) {
    ULayer *info = layer(u, 853);
    int ix = info ? info->x : 116, iy = info ? info->y : 100;
    for (int k = 0; k < 9; k++) {
        UItem *it = item(u, 10 + k);
        if (!it) continue;
        int slot = u->pageStart[u->page] + k;
        bool used = k < u->pageCount[u->page] && slot < EX_SLOTS && e->slots[slot].used;
        int state = u->hover == it->id ? 1 : 0;
        if (u->kind == UI_LOAD && !used) state = 2;
        draw_item(e, u, dst, it, state);
        char key[32];
        snprintf(key, sizeof key, "tn.%d.settings", 10 + k);
        const char *tn = get(u, key);
        if (used && tn) {
            char buf[64];
            snprintf(buf, sizeof buf, "%s", tn);
            char *f[6];
            if (split_csv(buf, f, 6) >= 3) {
                ULayer *l = layer(u, it->layer);
                Image *th = slot_thumb(e, slot);
                if (th) blit(dst, EX_W, EX_H, th, (l ? l->x : 0) + field_int(f[1]), (l ? l->y : 0) + field_int(f[2]), 255);
            }
        }
        snprintf(key, sizeof key, "info.clip.%d", 10 + k);
        const char *clip = get(u, key);
        int cx = 0, cy = 0;
        if (clip) {
            char buf[64];
            snprintf(buf, sizeof buf, "%s", clip);
            char *f[6];
            if (split_csv(buf, f, 6) >= 2) {
                cx = field_int(f[0]);
                cy = field_int(f[1]);
            }
        }
        char label[16];
        snprintf(label, sizeof label, "%03d", slot);
        text_draw_sjis(e, dst, label, ix + cx + 4, iy + cy + 44, 42, 0xf0f0f0, 0, 255);
        if (used) {
            char date[32], tod[32];
            fmt_time(e->slots[slot].time, date, sizeof date, true);
            fmt_time(e->slots[slot].time, tod, sizeof tod, false);
            text_draw_sjis(e, dst, date, ix + cx + 4, iy + cy + 72, 24, 0xf0f0f0, 0, 255);
            text_draw_sjis(e, dst, tod, ix + cx + 20, iy + cy + 98, 24, 0xf0f0f0, 0, 255);
            if (e->slots[slot].memo) text_wrap_sjis(e, dst, e->slots[slot].memo, ix + cx + 4, iy + cy + 112, 182, 24, 18, 0xf0f0f0, 0, 255, 4);
            if (slot == u->newest && u->mark) {
                snprintf(key, sizeof key, "mark.%d", 10 + k);
                const char *mk = get(u, key);
                if (mk) {
                    char buf[64];
                    snprintf(buf, sizeof buf, "%s", mk);
                    char *f[3];
                    if (split_csv(buf, f, 3) >= 2) blit(dst, EX_W, EX_H, u->mark, field_int(f[0]), field_int(f[1]), 255);
                }
            }
        }
    }
    for (int p = 0; p < u->pages; p++) {
        UItem *it = item(u, 30 + p);
        if (it) draw_item(e, u, dst, it, p == u->page ? 2 : u->hover == it->id ? 1 : 0);
    }
    for (int id = 98; id <= 99; id++) {
        UItem *it = item(u, id);
        if (!it) continue;
        int state = u->hover == id ? 1 : 0;
        if (id == 98 && u->kind == UI_LOAD && u->fromTitle) state = 2;
        draw_item(e, u, dst, it, state);
    }
}

static void draw_backlog(EX *e, Ui *u, u32 *dst) {
    int total = e->histCount - (e->msg.active ? 1 : 0);
    for (int r = 0; r < ROWS; r++) {
        HistEntry *h = hist(e, u->top + r);
        if (!h || u->top + r >= total) break;
        int y = 81 + r * ROW_H;
        if (h->name && *h->name) text_draw_sjis(e, dst, h->name, 117, y + 40, 32, 0xffffff, 0, 255);
        if (h->voice >= 0 && u->voiceMark) blit(dst, EX_W, EX_H, u->voiceMark, 527, y + 3, 255);
        text_wrap_sjis(e, dst, h->text, 581, y + 40 - text_ascent(e, 32), 1240, 50, 32, 0xffffff, 0, 255, 3);
    }
    for (int i = 0; i < u->ni; i++) {
        UItem *it = &u->items[i];
        if (it->layer != 850) continue;
        draw_item(e, u, dst, it, u->hover == it->id ? 1 : 0);
    }
    Image *thumb = image_load(e, "res/g/sy/01/1920_150t.gyu");
    if (thumb) {
        int maxTop = total - ROWS;
        int y = 156 + (maxTop > 0 ? (769 - thumb->h) * u->top / maxTop : 769 - thumb->h);
        blit(dst, EX_W, EX_H, thumb, 1844 + (34 - thumb->w) / 2, y, 255);
        image_free(thumb);
    }
}

void ui_draw(EX *e, u32 *dst) {
    Ui *u = e->ui;
    if (!u) return;
    for (int i = 0; i < u->nl; i++) {
        ULayer *l = &u->layers[i];
        if (l->img) blit(dst, EX_W, EX_H, l->img, l->x, l->y, 255);
    }
    if (u->kind == UI_SYSMENU) {
        for (int i = 0; i < u->ni; i++) draw_item(e, u, dst, &u->items[i], u->hover == u->items[i].id ? 1 : 0);
    } else if (u->kind == UI_SAVE || u->kind == UI_LOAD) draw_slots(e, u, dst);
    else if (u->kind == UI_BACKLOG) draw_backlog(e, u, dst);
}
