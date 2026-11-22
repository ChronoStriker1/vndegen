#include "ex.h"

enum { TAB_CG, TAB_SCENE, TAB_MUSIC };

#define MAX_GROUPS 64
#define MAX_PER_GROUP 64
#define MAX_SCENES 40
#define MAX_TRACKS 32
#define BAR_Y 943
#define TABS_X 1250
#define PAGES_X 120
#define CG_X 107
#define CG_Y 96
#define CG_DX 430
#define CG_DY 260
#define SCENE_X 108
#define SCENE_Y 103
#define LIST_X 108
#define LIST_Y 73

typedef struct Group {
    int count;
    int cg[MAX_PER_GROUP];
} Group;

typedef struct SceneEntry {
    int page, x, y, flag;
    char thumb[64], script[64];
} SceneEntry;

typedef struct MusicTrack {
    int item, x, y, track;
    char file[64];
    Image *img;
} MusicTrack;

struct Extra {
    int tab, page, hover;
    Group groups[MAX_GROUPS];
    int groupCount;
    SceneEntry scenes[MAX_SCENES];
    int sceneCount;
    MusicTrack tracks[MAX_TRACKS];
    int trackCount;
    Image *bg[3], *panel[3], *bar, *tabBar, *pageBar, *musicPanel;
    Image *tabs[4], *pages[4];
    Image *volIcon, *volLabel, *volL, *volR, *volTrack, *volKnob, *digits;
    Image *thumbs[MAX_GROUPS > MAX_SCENES ? MAX_GROUPS : MAX_SCENES];
    int viewGroup, viewIdx;
    Image *view;
    bool barHidden;
    int playing;
};

static bool seen_cg(EX *e, int id) { return id >= 0 && id < EX_SEEN_CG && (e->seenCg[id >> 3] >> (id & 7) & 1); }

static bool seen_scene(EX *e, int n) { return n >= 0 && n < EX_SEEN_SCENES && e->seenScene[n]; }

static char *extra_path(EX *e) { return ex_fmt("%s/vndegen/extra.txt", e->saveDir); }

static void extra_store(EX *e) {
    char *dir = ex_fmt("%s/vndegen", e->saveDir);
    ex_mkdirs(dir);
    free(dir);
    size_t cap = 65536, n = 0;
    char *b = ex_alloc(cap);
    n += (size_t)snprintf(b + n, cap - n, "EXHIBIT-VNDEGEN-EXTRA 1\ncg");
    for (int i = 0; i < EX_SEEN_CG && n + 16 < cap; i++)
        if (seen_cg(e, i)) n += (size_t)snprintf(b + n, cap - n, " %d", i);
    n += (size_t)snprintf(b + n, cap - n, "\nscene");
    for (int i = 0; i < EX_SEEN_SCENES && n + 16 < cap; i++)
        if (e->seenScene[i]) n += (size_t)snprintf(b + n, cap - n, " %d", i);
    n += (size_t)snprintf(b + n, cap - n, "\n");
    char *path = extra_path(e);
    ex_write_file(path, b, n);
    free(path);
    free(b);
}

static void vndegen_seen_load(EX *e) {
    char *path = extra_path(e);
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    if (!d) return;
    char *text = ex_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        bool cg = !strncmp(line, "cg", 2), sc = !strncmp(line, "scene", 5);
        if (!cg && !sc) continue;
        for (char *p = line + (cg ? 2 : 5); *p;) {
            char *end;
            long v = strtol(p, &end, 10);
            if (end == p) break;
            if (cg && v >= 0 && v < EX_SEEN_CG) e->seenCg[v >> 3] |= (u8)(1 << (v & 7));
            if (sc && v >= 0 && v < EX_SEEN_SCENES) e->seenScene[v] = 1;
            p = end;
        }
    }
    free(text);
}

static i32 sys_reg(const u8 *d, int i) { return (i32)((u32)d[22 + i * 4] | (u32)d[23 + i * 4] << 8 | (u32)d[24 + i * 4] << 16 | (u32)d[25 + i * 4] << 24); }

static void set_sys_reg(u8 *d, int i, i32 v) {
    for (int k = 0; k < 4; k++) d[22 + i * 4 + k] = (u8)((u32)v >> (8 * k));
}

static void pc_seen_sync(EX *e) {
    if (!e->className) return;
    char *path = ex_fmt("%s/%s.system.dat", e->saveDir, e->className);
    size_t n;
    u8 *d = ex_read_file(path, &n);
    if (!d || n < 22 || memcmp(d + 12, "FCReg", 6)) {
        free(d);
        free(path);
        return;
    }
    int count = (int)((u32)d[18] | (u32)d[19] << 8 | (u32)d[20] << 16 | (u32)d[21] << 24);
    if (count < 193 || n < 22 + (size_t)count * 4) {
        free(d);
        free(path);
        return;
    }
    int nCg = sys_reg(d, 189), nScene = sys_reg(d, 190), cgBase = 193, sceneBase = cgBase + nCg;
    if (nCg < 0 || nScene < 0 || sceneBase + nScene > count) {
        free(d);
        free(path);
        return;
    }
    bool changed = false;
    for (int id = 0; id < nCg && id < EX_SEEN_CG; id++) {
        if (sys_reg(d, cgBase + id)) e->seenCg[id >> 3] |= (u8)(1 << (id & 7));
        else if (seen_cg(e, id)) {
            set_sys_reg(d, cgBase + id, 1);
            changed = true;
        }
    }
    for (int s = 0; s < nScene && s < EX_SEEN_SCENES; s++) {
        if (sys_reg(d, sceneBase + s)) e->seenScene[s] = 1;
        else if (e->seenScene[s]) {
            set_sys_reg(d, sceneBase + s, 1);
            changed = true;
        }
    }
    if (changed) ex_write_file(path, d, n);
    free(d);
    free(path);
}

void extra_load(EX *e) {
    vndegen_seen_load(e);
    pc_seen_sync(e);
}

void extra_mark_cg(EX *e, int id) {
    if (id < 0 || id >= EX_SEEN_CG || seen_cg(e, id)) return;
    e->seenCg[id >> 3] |= (u8)(1 << (id & 7));
    extra_store(e);
    pc_seen_sync(e);
}

void extra_mark_scene(EX *e, int n) {
    if (n < 0 || n >= EX_SEEN_SCENES || e->seenScene[n]) return;
    e->seenScene[n] = 1;
    extra_store(e);
    pc_seen_sync(e);
}

static Op *find_op(EX *e, const char *script, int code, const char *prefix) {
    Script *sc = rld_get(e, script);
    for (int i = 0; sc && i < sc->count; i++) {
        Op *op = &sc->ops[i];
        if (op->op == code && op->ns && (!prefix || !strncmp(op->s[0], prefix, strlen(prefix)))) return op;
    }
    return NULL;
}

static void parse_groups(EX *e, Extra *x) {
    Op *op = find_op(e, "cgMode", 132, "0,0,NUMBER");
    if (!op) return;
    char *s = ex_strdup(op->s[0]);
    char *at = strchr(s, '@');
    char *stop = at ? strstr(at, ",NUMBER") : NULL;
    if (stop) *stop = 0;
    while (at && x->groupCount < MAX_GROUPS) {
        char *next = strchr(at + 1, '@');
        if (next) *next = 0;
        Group *g = &x->groups[x->groupCount++];
        int tok[5 * MAX_PER_GROUP + 1], nt = 0;
        for (char *p = at + 1; *p && nt < 5 * MAX_PER_GROUP + 1;) {
            char *end;
            long v = strtol(p, &end, 10);
            if (end == p) {
                p++;
                continue;
            }
            tok[nt++] = (int)v;
            p = end;
        }
        for (int i = 1; i + 4 < nt + 1 && g->count < MAX_PER_GROUP; i += 5) g->cg[g->count++] = tok[i + 1];
        at = next;
    }
    free(s);
}

static void parse_scenes(EX *e, Extra *x) {
    char *path = ex_game_path(e, "res/c/replay1.rnf");
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    if (!d) return;
    char *text = ex_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        if (strncmp(line, "tn", 2) || !strchr(line, '=') || x->sceneCount >= MAX_SCENES) continue;
        char *f[8];
        int k = split_csv(strchr(line, '=') + 1, f, 8);
        if (k < 7) continue;
        SceneEntry *s = &x->scenes[x->sceneCount++];
        s->page = field_int(f[0]);
        s->x = field_int(f[2]);
        s->y = field_int(f[3]);
        snprintf(s->thumb, sizeof s->thumb, "%s", f[4]);
        s->flag = field_int(f[5]);
        snprintf(s->script, sizeof s->script, "%s", f[6]);
        char *sp = strpbrk(s->script, " \t;");
        if (sp) *sp = 0;
    }
    free(text);
}

static int db_lookup(EX *e, int table, int key) {
    Script *sc = rld_get(e, "def");
    for (int i = 0; sc && i < sc->count; i++) {
        Op *op = &sc->ops[i];
        if (op->op != 61 || !op->nv || op->v[0] != table || !op->ns) continue;
        for (const char *p = op->s[0]; *p;) {
            char *end;
            long k = strtol(p, &end, 10);
            if (end == p) break;
            long v = strtol(end, &end, 10);
            if (k == key) return (int)v;
            p = end;
            while (*p == '\r' || *p == '\n') p++;
        }
    }
    return -1;
}

static void parse_tracks(EX *e, Extra *x) {
    Script *sc = rld_get(e, "musicMode");
    for (int i = 0; sc && i < sc->count; i++) {
        Op *op = &sc->ops[i];
        if (op->op != 203 || !op->ns || !strstr(op->s[0], "layer.611.item")) continue;
        char *text = ex_strdup(op->s[0]);
        for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
            int item;
            if (sscanf(line, "layer.611.item.%d.create=", &item) != 1 || x->trackCount >= MAX_TRACKS) continue;
            char *f[12];
            int k = split_csv(strchr(line, '=') + 1, f, 12);
            if (k < 7) continue;
            MusicTrack *t = &x->tracks[x->trackCount++];
            t->item = item;
            t->x = field_int(f[4]);
            t->y = field_int(f[5]);
            snprintf(t->file, sizeof t->file, "%s", f[6]);
            t->track = db_lookup(e, 1, item);
        }
        free(text);
        break;
    }
}

static Image *load(EX *e, const char *rel) { return image_load(e, rel); }

static Image *cg_pic(EX *e, int id) {
    char rel[64];
    snprintf(rel, sizeof rel, "res/g/ev/%02d/%04d.gyu", id / 100, id);
    return image_load(e, rel);
}

static void free_thumbs(Extra *x) {
    for (int i = 0; i < (int)(sizeof x->thumbs / sizeof *x->thumbs); i++) {
        image_free(x->thumbs[i]);
        x->thumbs[i] = NULL;
    }
}

static int page_count(Extra *x) {
    if (x->tab == TAB_CG) return (x->groupCount + 11) / 12;
    if (x->tab == TAB_SCENE) {
        int m = 0;
        for (int i = 0; i < x->sceneCount; i++)
            if (x->scenes[i].page + 1 > m) m = x->scenes[i].page + 1;
        return m;
    }
    return 1;
}

static void set_tab(EX *e, Extra *x, int tab) {
    free_thumbs(x);
    x->tab = tab;
    x->page = 0;
    x->hover = -1;
    (void)e;
}

void extra_open(EX *e, int tab) {
    if (e->extra) return;
    Extra *x = ex_alloc(sizeof *x);
    e->extra = x;
    parse_groups(e, x);
    parse_scenes(e, x);
    parse_tracks(e, x);
    static const int bgs[3] = {330, 370, 350};
    static const char *const ovl[3] = {"res/g/sy/02/1920_2100.gyu", "res/g/sy/02/1920_2200.gyu", "res/g/sy/02/1920_2300.gyu"};
    for (int i = 0; i < 3; i++) {
        x->bg[i] = cg_pic(e, bgs[i]);
        x->panel[i] = load(e, ovl[i]);
    }
    x->bar = load(e, "res/g/sy/02/1920_2092.gyu");
    x->tabBar = load(e, "res/g/sy/02/1920_2001.gyu");
    x->pageBar = load(e, "res/g/sy/02/1920_2010.gyu");
    x->musicPanel = load(e, "res/g/sy/02/1920_2390.gyu");
    static const char *const tabs[4] = {"res/g/sy/02/1920_2002.gyu", "res/g/sy/02/1920_2003.gyu", "res/g/sy/02/1920_2004.gyu", "res/g/sy/02/1920_2009.gyu"};
    for (int i = 0; i < 4; i++) {
        x->tabs[i] = load(e, tabs[i]);
        char rel[48];
        snprintf(rel, sizeof rel, "res/g/sy/02/1920_%d.gyu", 2011 + i);
        x->pages[i] = load(e, rel);
    }
    for (int i = 0; i < x->trackCount; i++) x->tracks[i].img = load(e, x->tracks[i].file);
    x->volIcon = load(e, "res/g/sy/01/1920_631.gyu");
    x->volLabel = load(e, "res/g/sy/01/1920_630.gyu");
    x->volL = load(e, "res/g/sy/01/1920_621l.gyu");
    x->volR = load(e, "res/g/sy/01/1920_621r.gyu");
    x->volTrack = load(e, "res/g/sy/01/1920_621.gyu");
    x->volKnob = load(e, "res/g/sy/01/1920_621t.gyu");
    x->digits = load(e, "res/g/sy/01/1920_621n.gyu");
    x->playing = -1;
    set_tab(e, x, tab);
    e->lPress = e->rPress = false;
}

bool extra_active(EX *e) { return e->extra != NULL; }

void extra_free(EX *e) {
    Extra *x = e->extra;
    if (!x) return;
    free_thumbs(x);
    for (int i = 0; i < 3; i++) {
        image_free(x->bg[i]);
        image_free(x->panel[i]);
    }
    image_free(x->bar);
    image_free(x->tabBar);
    image_free(x->pageBar);
    image_free(x->musicPanel);
    for (int i = 0; i < 4; i++) {
        image_free(x->tabs[i]);
        image_free(x->pages[i]);
    }
    for (int i = 0; i < x->trackCount; i++) image_free(x->tracks[i].img);
    Image *vol[] = {x->volIcon, x->volLabel, x->volL, x->volR, x->volTrack, x->volKnob, x->digits};
    for (size_t i = 0; i < sizeof vol / sizeof *vol; i++) image_free(vol[i]);
    image_free(x->view);
    free(x);
    e->extra = NULL;
}

static bool group_open(EX *e, Group *g) {
    for (int i = 0; i < g->count; i++)
        if (seen_cg(e, g->cg[i])) return true;
    return false;
}

static void draw_state(u32 *dst, const Image *im, int states, int state, int x, int y) {
    if (!im) return;
    int w = im->w / states;
    blit_part(dst, EX_W, EX_H, im, state * w, 0, w, im->h, x, y, 255);
}

static bool in_box(EX *e, int x, int y, int w, int h) { return e->mouseX >= x && e->mouseX < x + w && e->mouseY >= y && e->mouseY < y + h; }

enum { HIT_TAB = 1000, HIT_PAGE = 2000, HIT_VOL_L = 3000, HIT_VOL_R, HIT_VOL_BAR };

#define VOL_X 100
#define VOL_Y 948

static int hit(EX *e, Extra *x) {
    for (int i = 0; i < 4; i++)
        if (in_box(e, TABS_X + i * 170, BAR_Y, 150, 136)) return HIT_TAB + i;
    if (x->tab != TAB_MUSIC)
        for (int i = 0; i < page_count(x) && i < 4; i++)
            if (in_box(e, PAGES_X + i * 100, BAR_Y, 80, 136)) return HIT_PAGE + i;
    if (x->tab == TAB_CG)
        for (int k = 0; k < 12; k++) {
            int i = x->page * 12 + k;
            if (i < x->groupCount && in_box(e, CG_X + (k % 4) * CG_DX, CG_Y + (k / 4) * CG_DY, 416, 234)) return i;
        }
    if (x->tab == TAB_SCENE)
        for (int i = 0; i < x->sceneCount; i++) {
            SceneEntry *s = &x->scenes[i];
            if (s->page == x->page && in_box(e, SCENE_X + s->x, SCENE_Y + s->y, 416, 326)) return i;
        }
    if (x->tab == TAB_MUSIC) {
        if (in_box(e, VOL_X + 204, VOL_Y + 46, 76, 76)) return HIT_VOL_L;
        if (in_box(e, VOL_X + 622, VOL_Y + 46, 76, 76)) return HIT_VOL_R;
        if (in_box(e, VOL_X + 280, VOL_Y + 46, 342, 76)) return HIT_VOL_BAR;
    }
    if (x->tab == TAB_MUSIC)
        for (int i = 0; i < x->trackCount; i++)
            if (in_box(e, LIST_X + x->tracks[i].x, LIST_Y + x->tracks[i].y, 842, 66)) return i;
    return -1;
}

static void close_to_title(EX *e) {
    extra_free(e);
    reset_for_title(e);
    vm_goto_script(e, "title");
}

static void start_scene(EX *e, SceneEntry *s) {
    char script[64];
    snprintf(script, sizeof script, "%s", s->script);
    extra_free(e);
    reset_for_title(e);
    e->R[1020] = 1;
    vm_goto_script(e, script);
}

static void cg_show(EX *e, Extra *x) {
    image_free(x->view);
    x->view = cg_pic(e, x->groups[x->viewGroup].cg[x->viewIdx]);
}

void extra_input(EX *e) {
    Extra *x = e->extra;
    if (!x) return;
    if (x->view) {
        Group *g = &x->groups[x->viewGroup];
        bool onBar = e->mouseY >= EX_H - 80;
        if (e->rPress) x->barHidden = !x->barHidden;
        else if (e->lPress && onBar && e->mouseX >= EX_W - 300) {
            image_free(x->view);
            x->view = NULL;
        } else if (e->lPress && onBar && x->barHidden) x->barHidden = false;
        else if (e->lPress) {
            if (++x->viewIdx >= g->count) {
                image_free(x->view);
                x->view = NULL;
            } else cg_show(e, x);
        }
        e->lPress = e->rPress = false;
        return;
    }
    int h = hit(e, x);
    bool locked = (x->tab == TAB_CG && h >= 0 && h < HIT_TAB && !group_open(e, &x->groups[h])) ||
                  (x->tab == TAB_SCENE && h >= 0 && h < HIT_TAB && !seen_scene(e, x->scenes[h].flag));
    if (h != x->hover && h >= 0 && !locked) snd_play_file(e, "res/s/e/993.wav", EX_CH_UI);
    x->hover = h;
    if (e->rPress) {
        e->rPress = false;
        snd_stop(e, EX_CH_MUSIC, 300);
        close_to_title(e);
        return;
    }
    if (!e->lPress) return;
    e->lPress = false;
    if (h < 0 || locked) return;
    snd_play_file(e, "res/s/e/990.wav", EX_CH_UI);
    if (h >= HIT_VOL_L) {
        int *v = &e->cfg.vol[0];
        if (h == HIT_VOL_L) *v -= 5;
        else if (h == HIT_VOL_R) *v += 5;
        else *v = (e->mouseX - (VOL_X + 280 + 17)) * 100 / (342 - 34);
        *v = *v < 0 ? 0 : *v > 100 ? 100 : *v;
        cfg_apply(e);
        cfg_save(e);
    } else if (h >= HIT_PAGE) x->page = h - HIT_PAGE;
    else if (h >= HIT_TAB) {
        int t = h - HIT_TAB;
        if (t == 3) {
            snd_stop(e, EX_CH_MUSIC, 300);
            close_to_title(e);
        } else if (t != x->tab) set_tab(e, x, t);
    } else if (x->tab == TAB_CG) {
        x->viewGroup = h;
        x->viewIdx = 0;
        x->barHidden = false;
        cg_show(e, x);
    } else if (x->tab == TAB_SCENE) start_scene(e, &x->scenes[h]);
    else if (x->tab == TAB_MUSIC && x->tracks[h].track > 0) {
        x->playing = h;
        snd_play_music(e, x->tracks[h].track, 300);
    }
}

void extra_draw(EX *e, u32 *dst) {
    Extra *x = e->extra;
    if (!x) return;
    if (x->view) {
        for (size_t i = 0; i < (size_t)EX_W * EX_H; i++) dst[i] = 0xff000000u;
        blit(dst, EX_W, EX_H, x->view, 0, 0, 255);
        if (!x->barHidden) {
            char n[32];
            snprintf(n, sizeof n, "%d / %d", x->viewIdx + 1, x->groups[x->viewGroup].count);
            text_draw_sjis(e, dst, n, 940, EX_H - 30, 30, 0xffffff, 0x202020, 255);
            text_draw_sjis(e, dst, "Return", EX_W - 260, EX_H - 30, 30, 0xffffff, 0x202020, 255);
        }
        return;
    }
    int t = x->tab;
    for (size_t i = 0; i < (size_t)EX_W * EX_H; i++) dst[i] = 0xff000000u;
    if (x->bg[t]) blit(dst, EX_W, EX_H, x->bg[t], 0, 0, 255);
    if (x->panel[t]) blit(dst, EX_W, EX_H, x->panel[t], 0, 0, 255);
    if (x->bar) blit(dst, EX_W, EX_H, x->bar, 0, BAR_Y, 255);
    if (x->tabBar) blit(dst, EX_W, EX_H, x->tabBar, TABS_X, BAR_Y, 255);
    for (int i = 0; i < 4; i++) draw_state(dst, x->tabs[i], 3, i == t ? 2 : x->hover == HIT_TAB + i ? 1 : 0, TABS_X + i * 170, BAR_Y);
    if (t == TAB_MUSIC) {
        if (x->musicPanel) blit(dst, EX_W, EX_H, x->musicPanel, VOL_X, VOL_Y, 255);
        draw_state(dst, x->volIcon, 6, 0, VOL_X, VOL_Y);
        if (x->volLabel) blit_part(dst, EX_W, EX_H, x->volLabel, 0, 0, x->volLabel->w, x->volLabel->h, VOL_X + 192, VOL_Y + 4, 255);
        draw_state(dst, x->volL, 3, x->hover == HIT_VOL_L ? 1 : 0, VOL_X + 204, VOL_Y + 46);
        draw_state(dst, x->volR, 3, x->hover == HIT_VOL_R ? 1 : 0, VOL_X + 622, VOL_Y + 46);
        if (x->volTrack) blit_part(dst, EX_W, EX_H, x->volTrack, 0, 0, 342, 76, VOL_X + 280, VOL_Y + 46, 255);
        int vol = e->cfg.vol[0];
        if (x->volKnob) blit_part(dst, EX_W, EX_H, x->volKnob, 0, 0, 34, 74, VOL_X + 280 + 4 + (342 - 42) * vol / 100, VOL_Y + 47, 255);
        if (x->digits) {
            char num[8];
            snprintf(num, sizeof num, "%3d", vol);
            int dw = x->digits->w / 11;
            for (int k = 0; k < 3; k++)
                if (num[k] >= '0' && num[k] <= '9') blit_part(dst, EX_W, EX_H, x->digits, (num[k] - '0') * dw, 0, dw, x->digits->h, VOL_X + 702 + k * dw, VOL_Y + 64, 255);
        }
        for (int i = 0; i < x->trackCount; i++) {
            MusicTrack *tr = &x->tracks[i];
            int st = (i == x->playing ? 3 : 0) + (x->hover == i ? 1 : 0);
            draw_state(dst, tr->img, 6, st, LIST_X + tr->x, LIST_Y + tr->y);
        }
        return;
    }
    if (x->pageBar) blit(dst, EX_W, EX_H, x->pageBar, PAGES_X, BAR_Y, 255);
    for (int i = 0; i < page_count(x) && i < 4; i++)
        draw_state(dst, x->pages[i], 3, i == x->page ? 2 : x->hover == HIT_PAGE + i ? 1 : 0, PAGES_X + i * 100, BAR_Y);
    if (t == TAB_CG) {
        for (int k = 0; k < 12; k++) {
            int i = x->page * 12 + k;
            if (i >= x->groupCount) break;
            if (!x->thumbs[i]) {
                char rel[48];
                snprintf(rel, sizeof rel, "res/g/sy/tn/%04d.gyu", i);
                x->thumbs[i] = load(e, rel);
            }
            int st = group_open(e, &x->groups[i]) ? (x->hover == i ? 1 : 0) : 2;
            draw_state(dst, x->thumbs[i], 3, st, CG_X + (k % 4) * CG_DX, CG_Y + (k / 4) * CG_DY);
        }
    } else {
        for (int i = 0; i < x->sceneCount; i++) {
            SceneEntry *s = &x->scenes[i];
            if (s->page != x->page) continue;
            if (!x->thumbs[i]) x->thumbs[i] = load(e, s->thumb);
            int st = seen_scene(e, s->flag) ? (x->hover == i ? 1 : 0) : 2;
            draw_state(dst, x->thumbs[i], 3, st, SCENE_X + s->x, SCENE_Y + s->y);
        }
    }
}
