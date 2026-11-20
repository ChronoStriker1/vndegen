#include "eagls.h"

#define PAGE_SLOTS 16
#define MAX_PAGES 5
#define MAX_VIEW 16

static const char *const cgSlots[] = {
    "a01",  "a02",   "a03",  "a04",   "a05",  "a06",   "a07",  "b01",  "b02",   "b03",  "b04",  "b05",  "b06",  "b07",  "c01",  "c02",
    "c03",  "c04",   "xa01", "xa02",  "xa03", "xa04",  "xa05", "xa06", "xa07",  "xb01", "xb02", "xb03", "xb04", "xb05", "xb06", "xb07",
    "xb08a", "xb08b", "xb09", "xc01a", "xc01b", "xc02", "xc02b", "xc03", "xc03b", "xc04", "xc04b", "xc05", "xc06", "xc07", "xc08", "xc09b",
    "xc09", "xc10",  "xd01", "xd02",  "xd03a", "xd03b", "xd04a", "xd04b", "xd07", "xe01", "xe02", "xe04", "xe04b", "xe05", "xe05b", "xe07",
    "xe09", "xe09b", "xe11", "d01",   "d03b", "z01",   "z02",  "z03",  "z04",   "z05",
};

static const char *const sceneSlots[] = {
    "a01",  "a02",  "a03",  "a_e",  "b01",  "b02",  "b03",  "b04",  "b_e",  "z00",  "z01",  "z02",  "z03",  "z04",  "xa01", "xa02",
    "xa03", "xa04", "xa05", "xa06", "xa07", "xb01", "xb02", "xb03", "xb04", "xb05", "xb06", "xb07", "xb08", "xb09", "xc01", "xc02",
    "xc03", "xc04", "xc05", "xc06", "xc07", "xc08", "xc09", "xc10", "xd01", "xd02", "xd03", "xd04", "xd07", "xe01", "xe02", "xe04",
    "xe05", "xe07", "xe09", "xe11", "d01",  "49p",
};

enum { G_RETURN = 45, G_BACK = 30, G_NEXT = 35, G_PAGE1 = 11, G_SLOT0 = 70 };

struct Gallery {
    int kind;
    int pages, count;
    const char *const *slots;
    Image *base[MAX_PAGES], *thumb[MAX_PAGES], *sel[MAX_PAGES], *mask;
    int page, hover;
    char view[MAX_VIEW][16];
    int viewCount, viewIdx, viewPart;
    Image *viewImg;
    int lastScenePage;
};

static bool seen(EG *e, const char *flag) {
    char name[48];
    snprintf(name, sizeof name, "@%s", flag);
    return var_find(&e->vars, name) != NULL;
}

static bool slot_open(EG *e, Gallery *g, int i) {
    if (i < 0 || i >= g->count) return false;
    const char *s = g->slots[i];
    if (g->kind == UI_SCENE) {
        char f[32];
        snprintf(f, sizeof f, "Reco_%s", s);
        return seen(e, f);
    }
    if (seen(e, s)) return true;
    for (int r = 1; r <= 9; r++) {
        char f[32];
        snprintf(f, sizeof f, "%sr%d", s, r);
        if (seen(e, f)) return true;
    }
    return false;
}

static void free_images(Gallery *g) {
    for (int p = 0; p < MAX_PAGES; p++) {
        image_free(g->base[p]);
        image_free(g->thumb[p]);
        image_free(g->sel[p]);
        g->base[p] = g->thumb[p] = g->sel[p] = NULL;
    }
    image_free(g->mask);
    image_free(g->viewImg);
    g->mask = g->viewImg = NULL;
}

void gallery_free(EG *e) {
    if (!e->gallery) return;
    free_images(e->gallery);
    free(e->gallery);
    e->gallery = NULL;
}

static Gallery *gallery(EG *e) {
    if (!e->gallery) e->gallery = eg_alloc(sizeof(Gallery));
    return e->gallery;
}

static void opaque(Image *im) {
    if (im) im->alpha = false;
}

void gallery_open(EG *e, int kind) {
    Gallery *g = gallery(e);
    int keep = g->lastScenePage;
    free_images(g);
    memset(g, 0, sizeof *g);
    g->lastScenePage = keep;
    g->kind = kind;
    const char *prefix = kind == UI_CG ? "CGMode" : "SCMode";
    g->slots = kind == UI_CG ? cgSlots : sceneSlots;
    g->count = kind == UI_CG ? (int)(sizeof cgSlots / sizeof *cgSlots) : (int)(sizeof sceneSlots / sizeof *sceneSlots);
    g->pages = (g->count + PAGE_SLOTS - 1) / PAGE_SLOTS;
    for (int p = 0; p < g->pages && p < MAX_PAGES; p++) {
        char n[32];
        snprintf(n, sizeof n, "%s1-%d", prefix, p + 1);
        opaque(g->base[p] = eg_image_load(e, n));
        snprintf(n, sizeof n, "%s1-%dC", prefix, p + 1);
        opaque(g->thumb[p] = eg_image_load(e, n));
        snprintf(n, sizeof n, "%s1-%dS", prefix, p + 1);
        opaque(g->sel[p] = eg_image_load(e, n));
    }
    char n[32];
    snprintf(n, sizeof n, "%sMask", prefix);
    g->mask = eg_image_load(e, n);
    g->page = kind == UI_SCENE ? g->lastScenePage : 0;
    g->hover = 0;
    e->uiMode = kind;
}

static int mask_at(Gallery *g, int x, int y) {
    const Image *m = g->mask;
    if (!m || x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    return (int)(m->px[(size_t)y * m->w + x] & 0xff);
}

static void copy_color(EG *e, u32 *dst, const Image *src, const Image *mask, int color) {
    if (!src || !mask) return;
    int w = src->w < mask->w ? src->w : mask->w, h = src->h < mask->h ? src->h : mask->h;
    for (int y = 0; y < h && y < e->H; y++) {
        const u32 *m = mask->px + (size_t)y * mask->w, *s = src->px + (size_t)y * src->w;
        u32 *d = dst + (size_t)y * e->W;
        for (int x = 0; x < w && x < e->W; x++)
            if ((int)(m[x] & 0xff) == color) d[x] = s[x] | 0xff000000u;
    }
}

static int slot_index(Gallery *g, int color) {
    int k = color - G_SLOT0;
    if (k < 0 || k >= PAGE_SLOTS) return -1;
    int i = g->page * PAGE_SLOTS + k;
    return i < g->count ? i : -1;
}

static void view_load(EG *e, Gallery *g) {
    image_free(g->viewImg);
    g->viewImg = eg_image_load(e, g->view[g->viewIdx]);
    opaque(g->viewImg);
    g->viewPart = 0;
}

static void start_view(EG *e, Gallery *g, const char *base) {
    g->viewCount = 0;
    if (seen(e, base)) snprintf(g->view[g->viewCount++], 16, "%s", base);
    for (int r = 1; r <= 9 && g->viewCount < MAX_VIEW; r++) {
        char f[16];
        snprintf(f, sizeof f, "%sr%d", base, r);
        if (seen(e, f)) snprintf(g->view[g->viewCount++], 16, "%s", f);
    }
    if (!g->viewCount) snprintf(g->view[g->viewCount++], 16, "%s", base);
    g->viewIdx = 0;
    view_load(e, g);
}

static void view_next(EG *e, Gallery *g) {
    if (g->viewImg && g->viewImg->h > e->H && g->viewPart == 0) {
        g->viewPart = 1;
        return;
    }
    if (++g->viewIdx >= g->viewCount) {
        image_free(g->viewImg);
        g->viewImg = NULL;
        g->viewCount = 0;
        return;
    }
    view_load(e, g);
}

static void start_scene(EG *e, Gallery *g, const char *s) {
    char file[32], label[32];
    if (!strcmp(s, "xa01")) snprintf(file, sizeof file, "sc00b.dat");
    else if (!strcmp(s, "49p")) snprintf(file, sizeof file, "49p.dat");
    else snprintf(file, sizeof file, "sc_%s.dat", s);
    snprintf(label, sizeof label, "Reco_%s", s);
    g->lastScenePage = g->page;
    e->uiMode = UI_NONE;
    e->scriptUi = false;
    Thread *t = &e->th[0];
    t->sp = 0;
    t->wait = W_NONE;
    vm_goto(e, t, file, label);
}

void gallery_update(EG *e) {
    Gallery *g = e->gallery;
    if (!g) return;
    if (g->viewCount) {
        if (e->rclick) {
            image_free(g->viewImg);
            g->viewImg = NULL;
            g->viewCount = 0;
        } else if (e->click) view_next(e, g);
        return;
    }
    int c = mask_at(g, e->mx, e->my);
    int idx = slot_index(g, c);
    if (idx >= 0 && !slot_open(e, g, idx)) c = 0;
    if (c >= G_PAGE1 && c < G_PAGE1 + MAX_PAGES && c - G_PAGE1 >= g->pages) c = 0;
    if (c != g->hover && c) snd_play(e, "SYS_OV", 0, 2, 0);
    g->hover = c;
    if (e->rclick) c = G_RETURN;
    else if (!e->click || !c) return;
    snd_play(e, "SYS_OK", 0, 2, 0);
    if (c == G_RETURN) {
        if (g->kind == UI_SCENE) g->lastScenePage = 0;
        e->uiMode = UI_NONE;
        if (e->scriptUi) e->scriptUiDone = true;
    } else if (c == G_BACK) g->page = (g->page + g->pages - 1) % g->pages;
    else if (c == G_NEXT) g->page = (g->page + 1) % g->pages;
    else if (c >= G_PAGE1 && c < G_PAGE1 + g->pages) g->page = c - G_PAGE1;
    else if (idx >= 0) {
        if (g->kind == UI_CG) start_view(e, g, g->slots[idx]);
        else start_scene(e, g, g->slots[idx]);
    }
}

void gallery_draw(EG *e, u32 *dst) {
    Gallery *g = e->gallery;
    if (!g) return;
    if (g->viewCount && g->viewImg) {
        Image *im = g->viewImg;
        int sy = g->viewPart && im->h > e->H ? im->h - e->H : 0;
        blit_part(dst, e->W, e->H, im, 0, sy, e->W, e->H, 0, 0, 255);
        return;
    }
    int p = g->page;
    blit(dst, e->W, e->H, g->base[p], 0, 0, 255);
    for (int k = 0; k < PAGE_SLOTS; k++) {
        int i = p * PAGE_SLOTS + k;
        if (i < g->count && slot_open(e, g, i)) copy_color(e, dst, g->thumb[p], g->mask, G_SLOT0 + k);
    }
    copy_color(e, dst, g->sel[p], g->mask, G_PAGE1 + p);
    if (g->hover) copy_color(e, dst, g->sel[p], g->mask, g->hover);
}

bool gallery_scene_end(EG *e, Thread *t) {
    Var *v = var_find(&e->vars, ":SCENEFLG");
    if (!v || !var_at(v, 0)->i) return false;
    t->sp = 0;
    vm_goto(e, t, "TitleSC.dat", "SceneStart");
    return true;
}
