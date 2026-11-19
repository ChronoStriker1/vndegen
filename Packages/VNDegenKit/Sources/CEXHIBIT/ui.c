#include "ex.h"

#include <ctype.h>

#include <time.h>

enum { UI_NONE, UI_SYSMENU, UI_SAVE, UI_LOAD, UI_BACKLOG, UI_CONFIG };

typedef struct Cfg Cfg;
static void cfg_build(EX *e, Ui *u);
static void cfg_free(Cfg *cf);
static void cfg_draw(EX *e, Ui *u, u32 *dst);
static void cfg_input(EX *e, Ui *u);

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
    Cfg *cfg;
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
    bool block = false;
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        char *c = strchr(line, ';');
        if (c) *c = 0;
        char *t = trim(line);
        if (!strncmp(t, "<--", 3)) {
            block = true;
            continue;
        }
        if (!strncmp(t, "-->", 3)) {
            block = false;
            continue;
        }
        char *eq = strchr(t, '=');
        if (block && u->nkv && (!eq || isdigit((u8)*t) || *t == '-')) {
            KV *last = &u->kv[u->nkv - 1];
            char *joined = ex_fmt("%s%s%s", last->v, *last->v ? "\n" : "", t);
            free(last->v);
            last->v = joined;
            continue;
        }
        if (!eq) continue;
        *eq = 0;
        char *k = trim(t), *v = trim(eq + 1);
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
    cfg_free(u->cfg);
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
    case 104:
    case 12:
        kind = UI_CONFIG;
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
    else if (kind == UI_CONFIG) {
        rnf_load(e, u, "res/c/settings.layout.1st.rnf");
        build_from_rnf(e, u);
        for (int i = 0; i < u->ni;) {
            if (u->items[i].layer != 851) {
                image_free(u->items[i].img);
                u->items[i] = u->items[--u->ni];
            } else i++;
        }
        cfg_build(e, u);
        for (int i = 0; i < u->nl; i++)
            if (u->layers[i].id >= 852 && u->layers[i].id <= 856) {
                ULayer tmp = u->layers[i];
                (void)tmp;
            }
    }
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
    if (u->kind == UI_CONFIG) {
        cfg_input(e, u);
        return;
    }
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
        if (u->kind == UI_CONFIG && l->id >= 852) continue;
        if (l->img) blit(dst, EX_W, EX_H, l->img, l->x, l->y, 255);
    }
    if (u->kind == UI_CONFIG) {
        cfg_draw(e, u, dst);
        return;
    }
    if (u->kind == UI_SYSMENU) {
        for (int i = 0; i < u->ni; i++) draw_item(e, u, dst, &u->items[i], u->hover == u->items[i].id ? 1 : 0);
    } else if (u->kind == UI_SAVE || u->kind == UI_LOAD) draw_slots(e, u, dst);
    else if (u->kind == UI_BACKLOG) draw_backlog(e, u, dst);
}

typedef struct CItem {
    int id, layer, kind, x, y, w, states, action, target, step, var;
    Image *img, *knob, *track, *digits;
    int digitX, digitY;
    char *cond;
} CItem;

typedef struct CRelate {
    int layer, type, x, y, cellH, item;
    Image *img;
} CRelate;

typedef struct Cfg {
    CItem items[128];
    int ni;
    CRelate rel[96];
    int nr;
    int page;
    int pageLayer[5];
    int dragging;
} Cfg;

static int *cfg_value(EX *e, int var, int itemId) {
    switch (itemId) {
    case 38: return &e->cfg.windowT;
    case 41: return &e->cfg.textSpeed;
    case 44: return &e->cfg.autoSpeed;
    case 48: return &e->cfg.vol[0];
    case 53: return &e->cfg.vol[1];
    case 58: return &e->cfg.vol[2];
    case 63: return &e->cfg.vol[3];
    case 68: return &e->cfg.vol[4];
    case 73: return &e->cfg.vol[5];
    case 81: return &e->cfg.charVol[0];
    case 86: return &e->cfg.charVol[1];
    case 91: return &e->cfg.charVol[2];
    case 96: return &e->cfg.charVol[3];
    case 101: return &e->cfg.charVol[4];
    default: break;
    }
    (void)var;
    return NULL;
}

static bool *cfg_flag(EX *e, int itemId) {
    for (int i = 0; i < EX_CFG_FLAGS; i++)
        if (e->cfg.flagId[i] == itemId) return &e->cfg.flag[i];
    for (int i = 0; i < EX_CFG_FLAGS; i++)
        if (!e->cfg.flagId[i]) {
            e->cfg.flagId[i] = itemId;
            return &e->cfg.flag[i];
        }
    return NULL;
}

static bool cfg_on(EX *e, CItem *c) {
    if (c->id >= 46 && c->id <= 66 && c->layer == 854) {
        int k = (c->id - 46) / 5;
        return k >= 0 && k < 6 && e->cfg.mute[k];
    }
    if (c->id >= 79 && c->id <= 99 && c->layer == 855) {
        int k = (c->id - 79) / 5;
        return k >= 0 && k < 5 && e->cfg.charMute[k];
    }
    if (c->id == 32) return e->L[12] == 0;
    bool *f = cfg_flag(e, c->id);
    return f && *f;
}

static void cfg_toggle(EX *e, CItem *c) {
    if (c->id >= 46 && c->id <= 66 && c->layer == 854) {
        int k = (c->id - 46) / 5;
        if (k >= 0 && k < 6) e->cfg.mute[k] = !e->cfg.mute[k];
    } else if (c->id >= 79 && c->id <= 99 && c->layer == 855) {
        int k = (c->id - 79) / 5;
        if (k >= 0 && k < 5) e->cfg.charMute[k] = !e->cfg.charMute[k];
    } else if (c->id == 32) e->L[12] = e->L[12] == 0 ? 1 : 0;
    else {
        bool *f = cfg_flag(e, c->id);
        if (f) *f = !*f;
    }
}

void cfg_apply(EX *e) {
    Config *c = &e->cfg;
    e->windowAlpha = 255 * (100 - c->windowT) / 100;
    for (int i = 0; i < e->msg.ctrlCount; i++)
        if (e->msg.ctrls[i].slider) e->msg.ctrls[i].value = e->windowAlpha;
    e->textMs = c->textSpeed >= 100 ? 15 : 15 + (100 - c->textSpeed) * 2;
    e->autoMs = 3500 - c->autoSpeed * 40;
    if (e->autoMs < 300) e->autoMs = 300;
    e->volBgm = c->mute[0] ? 0 : c->vol[0];
    e->volSe = c->mute[1] ? 0 : c->vol[1];
    e->volLoop = c->mute[2] ? 0 : c->vol[2];
    e->volEnv = c->mute[3] ? 0 : c->vol[3];
    e->volUi = c->mute[4] ? 0 : c->vol[4];
    e->volVoice = c->mute[5] ? 0 : c->vol[5];
    snd_apply_volumes(e);
}

void cfg_defaults(EX *e) {
    Config *c = &e->cfg;
    memset(c, 0, sizeof *c);
    c->windowT = 50;
    c->textSpeed = 100;
    c->autoSpeed = 50;
    for (int i = 0; i < 5; i++) c->vol[i] = 50;
    c->vol[5] = 100;
    for (int i = 0; i < 5; i++) c->charVol[i] = 70;
    int on[] = {30, 31, 35, 36};
    for (size_t i = 0; i < sizeof on / sizeof on[0]; i++) {
        c->flagId[i] = on[i];
        c->flag[i] = true;
    }
}

void cfg_save(EX *e) {
    Config *c = &e->cfg;
    char *dir = ex_fmt("%s/vndegen", e->saveDir);
    ex_mkdirs(dir);
    free(dir);
    char buf[2048];
    int n = snprintf(buf, sizeof buf, "EXHIBIT-VNDEGEN-CONFIG 1\nwindow %d\ntext %d\nauto %d\nvol %d %d %d %d %d %d\nmute %d %d %d %d %d %d\nchar %d %d %d %d %d\ncharmute %d %d %d %d %d\nL12 %d\nL18 %d\nflags", c->windowT, c->textSpeed, c->autoSpeed,
                     c->vol[0], c->vol[1], c->vol[2], c->vol[3], c->vol[4], c->vol[5], c->mute[0], c->mute[1], c->mute[2], c->mute[3], c->mute[4], c->mute[5], c->charVol[0], c->charVol[1], c->charVol[2], c->charVol[3], c->charVol[4],
                     c->charMute[0], c->charMute[1], c->charMute[2], c->charMute[3], c->charMute[4], e->L[12], e->L[18]);
    for (int i = 0; i < EX_CFG_FLAGS && n < (int)sizeof buf - 16; i++)
        if (c->flagId[i]) n += snprintf(buf + n, sizeof buf - (size_t)n, " %d=%d", c->flagId[i], c->flag[i]);
    n += snprintf(buf + n, sizeof buf - (size_t)n, "\n");
    char *path = ex_fmt("%s/vndegen/config.txt", e->saveDir);
    ex_write_file(path, buf, (size_t)n);
    free(path);
}

void cfg_load(EX *e) {
    cfg_defaults(e);
    char *path = ex_fmt("%s/vndegen/config.txt", e->saveDir);
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    if (d) {
        char *text = ex_alloc(n + 1);
        memcpy(text, d, n);
        free(d);
        Config *c = &e->cfg;
        for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
            if (sscanf(line, "window %d", &c->windowT) == 1) continue;
            if (sscanf(line, "text %d", &c->textSpeed) == 1) continue;
            if (sscanf(line, "auto %d", &c->autoSpeed) == 1) continue;
            if (sscanf(line, "vol %d %d %d %d %d %d", &c->vol[0], &c->vol[1], &c->vol[2], &c->vol[3], &c->vol[4], &c->vol[5]) == 6) continue;
            int m[6];
            if (sscanf(line, "mute %d %d %d %d %d %d", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
                for (int i = 0; i < 6; i++) c->mute[i] = m[i] != 0;
                continue;
            }
            if (sscanf(line, "char %d %d %d %d %d", &c->charVol[0], &c->charVol[1], &c->charVol[2], &c->charVol[3], &c->charVol[4]) == 5) continue;
            if (sscanf(line, "charmute %d %d %d %d %d", &m[0], &m[1], &m[2], &m[3], &m[4]) == 5) {
                for (int i = 0; i < 5; i++) c->charMute[i] = m[i] != 0;
                continue;
            }
            int v;
            if (sscanf(line, "L12 %d", &v) == 1) e->L[12] = v;
            else if (sscanf(line, "L18 %d", &v) == 1) e->L[18] = v;
            else if (!strncmp(line, "flags", 5)) {
                char *p = line + 5;
                for (int i = 0; i < EX_CFG_FLAGS && *p;) {
                    int id, on;
                    int used = 0;
                    if (sscanf(p, " %d=%d%n", &id, &on, &used) != 2) break;
                    c->flagId[i] = id;
                    c->flag[i] = on != 0;
                    i++;
                    p += used;
                }
            }
        }
        free(text);
    }
    cfg_apply(e);
}

static void cfg_build(EX *e, Ui *u) {
    Cfg *cf = ex_alloc(sizeof *cf);
    u->cfg = cf;
    cf->dragging = -1;
    int pl[5] = {852, 853, 854, 855, 856};
    memcpy(cf->pageLayer, pl, sizeof pl);
    for (int i = 0; i < u->nkv; i++) {
        char *k = u->kv[i].k;
        if (strncmp(k, "layer.", 6)) continue;
        char *rest;
        int lid = (int)strtol(k + 6, &rest, 10);
        if (lid < 852 || lid > 856) continue;
        if (strncmp(rest, ".item.", 6)) continue;
        char *r2;
        int iid = (int)strtol(rest + 6, &r2, 10);
        if (!strcmp(r2, ".relate")) {
            char *val = ex_strdup(u->kv[i].v);
            for (char *line = strtok(val, "\n"); line; line = strtok(NULL, "\n")) {
                char *f[16];
                char buf[512];
                snprintf(buf, sizeof buf, "%s", trim(line));
                int n = split_csv(buf, f, 16);
                if (n < 6 || cf->nr >= 96) continue;
                CRelate *r = &cf->rel[cf->nr++];
                r->type = field_int(f[0]);
                r->layer = field_int(f[1]);
                r->x = field_int(f[3]);
                r->y = field_int(f[4]);
                r->img = image_load(e, trim(f[5]));
                r->cellH = r->type == 2 && n > 7 ? field_int(f[7]) : 0;
                r->item = iid;
            }
            free(val);
            continue;
        }
        CItem *c = NULL;
        for (int j = 0; j < cf->ni; j++)
            if (cf->items[j].id == iid && cf->items[j].layer == lid) c = &cf->items[j];
        if (!c) {
            if (cf->ni >= 128) continue;
            c = &cf->items[cf->ni++];
            memset(c, 0, sizeof *c);
            c->id = iid;
            c->layer = lid;
        }
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", u->kv[i].v);
        char *f[40];
        int n = split_csv(buf, f, 40);
        if (!strcmp(r2, ".create") && n >= 10) {
            c->kind = field_int(f[0]);
            c->x = field_int(f[4]);
            c->y = field_int(f[5]);
            if (c->kind >= 4 && n > 29) {
                c->w = field_int(f[14]);
                c->knob = image_load(e, trim(f[16]));
                c->track = image_load(e, trim(f[20]));
                c->digits = image_load(e, trim(f[25]));
                c->digitX = field_int(f[27]);
                c->digitY = field_int(f[28]);
            } else {
                c->img = image_load(e, trim(f[6]));
                c->states = 3 * (field_int(f[9]) > 0 ? field_int(f[9]) : 1);
                c->cond = ex_strdup(trim(f[2]));
            }
        } else if (!strcmp(r2, ".action") && n >= 2) {
            c->action = field_int(f[0]);
            if (c->action == 251 && n > 3) {
                c->target = field_int(f[2]);
                c->step = field_int(f[3]);
            }
        }
    }
}

static void cfg_free(Cfg *cf) {
    if (!cf) return;
    for (int i = 0; i < cf->ni; i++) {
        CItem *c = &cf->items[i];
        image_free(c->img);
        image_free(c->knob);
        image_free(c->track);
        image_free(c->digits);
        free(c->cond);
    }
    for (int i = 0; i < cf->nr; i++) image_free(cf->rel[i].img);
    free(cf);
}

static ULayer *cfg_layer(Ui *u, int id) { return layer(u, id); }

static void citem_rect(Ui *u, CItem *c, int *x, int *y, int *w, int *h) {
    ULayer *l = cfg_layer(u, c->layer);
    *x = c->x + (l ? l->x : 0);
    *y = c->y + (l ? l->y : 0);
    if (c->kind >= 4) {
        *w = c->w > 0 ? c->w : 342;
        *h = 76;
    } else if (c->img) {
        *w = c->img->w / (c->states > 0 ? c->states : 1);
        *h = c->img->h;
    } else *w = *h = 0;
}

static int slider_value_at(Ui *u, CItem *c, int mx) {
    int x, y, w, h;
    citem_rect(u, c, &x, &y, &w, &h);
    int kw = c->knob ? c->knob->w / 2 : 34;
    int span = w - kw - 16;
    int v = span > 0 ? (mx - x - 8 - kw / 2) * 100 / span : 0;
    return v < 0 ? 0 : v > 100 ? 100 : v;
}

static void cfg_draw(EX *e, Ui *u, u32 *dst) {
    Cfg *cf = u->cfg;
    int pageLayer = cf->pageLayer[cf->page];
    ULayer *pl = cfg_layer(u, pageLayer);
    if (pl && pl->img) blit(dst, EX_W, EX_H, pl->img, pl->x, pl->y, 255);
    for (int i = 0; i < cf->nr; i++) {
        CRelate *r = &cf->rel[i];
        if (r->layer != pageLayer || !r->img) continue;
        ULayer *l = cfg_layer(u, r->layer);
        int lx = l ? l->x : 0, ly = l ? l->y : 0;
        if (r->type == 2 && r->cellH > 0) {
            bool on = false;
            for (int j = 0; j < cf->ni; j++)
                if (cf->items[j].id == r->item && cf->items[j].layer == r->layer) on = cfg_on(e, &cf->items[j]);
            blit_part(dst, EX_W, EX_H, r->img, 0, on ? r->cellH : 0, r->img->w, r->cellH, lx + r->x, ly + r->y, 255);
        } else blit(dst, EX_W, EX_H, r->img, lx + r->x, ly + r->y, 255);
    }
    for (int i = 0; i < cf->ni; i++) {
        CItem *c = &cf->items[i];
        if (c->layer != pageLayer) continue;
        int x, y, w, h;
        citem_rect(u, c, &x, &y, &w, &h);
        bool hov = e->mouseX >= x && e->mouseX < x + w && e->mouseY >= y && e->mouseY < y + h;
        if (c->kind >= 4) {
            int *v = cfg_value(e, 0, c->id);
            int val = v ? *v : 50;
            if (c->track) blit_part(dst, EX_W, EX_H, c->track, 0, 0, c->track->w, 76, x, y, 255);
            if (c->knob) {
                int kw = c->knob->w / 2, span = w - kw - 16;
                blit_part(dst, EX_W, EX_H, c->knob, (hov || cf->dragging == c->id) ? kw : 0, 0, kw, c->knob->h, x + 8 + span * val / 100, y + 1, 255);
            }
            if (c->digits) {
                ULayer *l = cfg_layer(u, c->layer);
                int dw = c->digits->w / 11, dx = c->digitX + (l ? l->x : 0), dy = c->digitY + (l ? l->y : 0);
                char num[8];
                snprintf(num, sizeof num, "%3d", val);
                for (int k = 0; k < 3; k++)
                    if (num[k] >= '0' && num[k] <= '9') blit_part(dst, EX_W, EX_H, c->digits, (num[k] - '0') * dw, 0, dw, c->digits->h, dx + k * dw, dy, 255);
            }
            continue;
        }
        if (!c->img) continue;
        int st = hov ? 1 : 0;
        if (c->states >= 6 && cfg_on(e, c)) st += 3;
        if (c->id >= 76 && c->id <= 78 && c->cond && vm_cond(e, c->cond)) st = 2;
        int cw = c->img->w / c->states;
        if (st >= c->states) st = 0;
        blit_part(dst, EX_W, EX_H, c->img, st * cw, 0, cw, c->img->h, x, y, 255);
    }
    for (int i = 0; i < u->ni; i++) {
        UItem *it = &u->items[i];
        if (it->layer != 851) continue;
        int st = u->hover == it->id ? 1 : 0;
        if (it->id >= 10 && it->id <= 14 && it->id - 10 == cf->page) st = 2;
        draw_item(e, u, dst, it, st);
    }
}

static void cfg_input(EX *e, Ui *u) {
    Cfg *cf = u->cfg;
    int pageLayer = cf->pageLayer[cf->page];
    if (cf->dragging >= 0) {
        for (int i = 0; i < cf->ni; i++) {
            CItem *c = &cf->items[i];
            if (c->id != cf->dragging || c->layer != pageLayer) continue;
            int *v = cfg_value(e, 0, c->id);
            if (v) *v = slider_value_at(u, c, e->mouseX);
        }
        if (!e->lDown) {
            cf->dragging = -1;
            cfg_apply(e);
            cfg_save(e);
        }
        e->lPress = false;
        return;
    }
    if (e->rPress) {
        e->rPress = false;
        cfg_save(e);
        ui_close(e);
        return;
    }
    if (!e->lPress) return;
    e->lPress = false;
    if (u->hover >= 10 && u->hover <= 14) {
        cf->page = u->hover - 10;
        snd_play_file(e, "res/s/e/990.wav", EX_CH_UI);
        return;
    }
    if (u->hover == 17) {
        cfg_save(e);
        ui_close(e);
        return;
    }
    if (u->hover == 16) {
        cfg_defaults(e);
        e->L[12] = 0;
        e->L[18] = 0;
        cfg_apply(e);
        cfg_save(e);
        return;
    }
    for (int i = 0; i < cf->ni; i++) {
        CItem *c = &cf->items[i];
        if (c->layer != pageLayer) continue;
        int x, y, w, h;
        citem_rect(u, c, &x, &y, &w, &h);
        if (e->mouseX < x || e->mouseX >= x + w || e->mouseY < y || e->mouseY >= y + h) continue;
        if (c->kind >= 4) {
            cf->dragging = c->id;
            int *v = cfg_value(e, 0, c->id);
            if (v) *v = slider_value_at(u, c, e->mouseX);
            return;
        }
        snd_play_file(e, "res/s/e/990.wav", EX_CH_UI);
        if (c->action == 251) {
            int *v = cfg_value(e, 0, c->target);
            if (v) {
                *v += c->step * 5;
                if (*v < 0) *v = 0;
                if (*v > 100) *v = 100;
            }
        } else if (c->id >= 76 && c->id <= 78) e->L[18] = (c->id - 76) * 2;
        else if (c->states >= 6) cfg_toggle(e, c);
        cfg_apply(e);
        cfg_save(e);
        return;
    }
}
