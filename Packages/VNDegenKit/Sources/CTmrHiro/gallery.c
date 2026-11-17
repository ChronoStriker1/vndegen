#include "th.h"

enum { SYS_HSC_BASE = 512, SYS_HCG_BASE = 2512 };

static int var_index(TH *t, const char *section, const char *name) {
    const IniSection *s = ini_section(&t->varIni, section);
    if (!s) return -1;
    for (int i = 0; i < s->count; i++)
        if (th_ieq(s->lines[i].key, name)) return i;
    return -1;
}

static u8 *flag_byte(TH *t, int base, int index) {
    if (index < 0 || index >= 2000) return NULL;
    if (t->system000 && (size_t)(base + index) < t->system000Size) return &t->system000[base + index];
    if (!t->galleryMarks) t->galleryMarks = th_alloc(4096);
    return &t->galleryMarks[(base == SYS_HSC_BASE ? 0 : 2048) + index];
}

static bool flag_get(TH *t, int base, int index) {
    u8 *b = flag_byte(t, base, index);
    return b && *b;
}

static void flag_set(TH *t, int base, int index) {
    u8 *b = flag_byte(t, base, index);
    if (!b || *b) return;
    *b = 1;
    if (t->system000) t->systemDirty = true;
}

void gallery_mark_cg(TH *t, const char *name) { flag_set(t, SYS_HCG_BASE, var_index(t, "HCG", name)); }

void gallery_mark_scene(TH *t, const char *name) { flag_set(t, SYS_HSC_BASE, var_index(t, "HSC", name)); }

static bool cg_seen(TH *t, const char *name) { return t->galleryAll || flag_get(t, SYS_HCG_BASE, var_index(t, "HCG", name)); }

static bool scene_seen(TH *t, const char *name) { return t->galleryAll || flag_get(t, SYS_HSC_BASE, var_index(t, "HSC", name)); }

void gallery_page(TH *t, const u8 *b, int len) {
    for (int i = 0; i < t->galCount; i++) free(t->gal[i].list);
    t->galCount = 0;
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%.*s", len > 4 ? len - 4 : 0, (const char *)b + 4);
    char *f[4] = {NULL, NULL, NULL, NULL};
    int n = 0;
    for (char *p = strtok(tmp, ","); p && n < 4; p = strtok(NULL, ",")) f[n++] = p;
    snprintf(t->galNodata, sizeof t->galNodata, "%s", f[0] ? f[0] : "");
    snprintf(t->galIncomplete, sizeof t->galIncomplete, "%s", f[1] ? f[1] : "");
    snprintf(t->galBase, sizeof t->galBase, "%s", f[2] ? f[2] : "");
    snprintf(t->galOn, sizeof t->galOn, "%s", f[3] ? f[3] : "");
}

void gallery_item(TH *t, const u8 *b, int len) {
    if (t->galCount >= 48 || len < 10) return;
    GalItem *g = &t->gal[t->galCount++];
    g->type = rd16(b);
    g->x1 = (i16)rd16(b + 2);
    g->y1 = (i16)rd16(b + 4);
    g->x2 = (i16)rd16(b + 6);
    g->y2 = (i16)rd16(b + 8);
    g->list = th_strdup((const char *)b + 10);
}

static bool item_live(const GalItem *g) { return g->type == 0x0a || g->type == 0x14 || g->type == 0x01 || g->type == 0x1e || g->type == 0x1f; }

static int cg_counts(TH *t, const GalItem *g, int *seen) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", g->list);
    int total = 0;
    *seen = 0;
    for (char *p = strtok(tmp, ","); p; p = strtok(NULL, ",")) {
        total++;
        if (cg_seen(t, p)) (*seen)++;
    }
    return total;
}

static bool item_locked(TH *t, const GalItem *g) {
    if (g->type == 0x0a) {
        int seen;
        cg_counts(t, g, &seen);
        return seen == 0;
    }
    if (g->type == 0x14) return !scene_seen(t, g->list);
    return false;
}

static void open_viewer(TH *t, const GalItem *g) {
    t->viewCount = 0;
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", g->list);
    for (char *p = strtok(tmp, ","); p && t->viewCount < 64; p = strtok(NULL, ","))
        if (cg_seen(t, p)) snprintf(t->viewList[t->viewCount++], sizeof t->viewList[0], "%s", p);
    if (!t->viewCount) return;
    t->viewIndex = 0;
    t->viewImg = image_get(t, t->viewList[0]);
    t->wait = W_CGVIEW;
    t->dirty = true;
}

static void leave(TH *t, const char *label) {
    t->wait = W_NONE;
    t->hover = -1;
    t->dirty = t->sceneDirty = true;
    vm_goto(t, NULL, label);
}

static const char *cancel_label(TH *t) {
    for (int i = 0; i < t->galCount; i++)
        if (t->gal[i].type == 0x02) return t->gal[i].list;
    return NULL;
}

void gallery_update(TH *t) {
    if (t->wait == W_CGVIEW) {
        if (t->click || t->keyPress[0x0d] || t->keyPress[0x20]) {
            if (++t->viewIndex >= t->viewCount) {
                t->wait = W_GALLERY;
            } else {
                t->viewImg = image_get(t, t->viewList[t->viewIndex]);
            }
            t->dirty = true;
        } else if (t->rclick || t->keyPress[0x1b]) {
            t->wait = W_GALLERY;
            t->dirty = true;
        }
        return;
    }
    int old = t->hover;
    t->hover = -1;
    for (int i = 0; i < t->galCount; i++) {
        GalItem *g = &t->gal[i];
        if (item_live(g) && t->mx >= g->x1 && t->my >= g->y1 && t->mx < g->x2 && t->my < g->y2) t->hover = i;
    }
    if (t->hover != old) t->dirty = true;
    if (t->rclick || t->keyPress[0x1b]) {
        const char *c = cancel_label(t);
        if (c) leave(t, c);
        return;
    }
    if (!t->click || t->hover < 0) return;
    GalItem *g = &t->gal[t->hover];
    if (item_locked(t, g)) return;
    switch (g->type) {
    case 0x0a: open_viewer(t, g); break;
    case 0x01: leave(t, g->list); break;
    case 0x1e:
        snprintf(t->bgmName, sizeof t->bgmName, "%s", g->list);
        snd_play(t, CH_BGM, PAC_BGM, g->list, true, 0);
        break;
    case 0x1f:
        snd_stop(t, CH_BGM, 0);
        t->bgmName[0] = 0;
        break;
    case 0x14: {
        char scene[64];
        snprintf(scene, sizeof scene, "%s", g->list);
        t->wait = W_NONE;
        t->replayDepth = t->sp + 1;
        snprintf(t->replayScene, sizeof t->replayScene, "%s", t->sc ? t->sc->name : "");
        t->replayPc = t->pc;
        t->pc = t->galLoopPc;
        vm_call(t, scene, NULL);
        t->dirty = t->sceneDirty = true;
        break;
    }
    default: break;
    }
}

void gallery_draw(TH *t) {
    if (t->wait == W_CGVIEW) {
        if (t->viewImg) blit(t->screen, t->W, t->H, t->viewImg, 0, 0, 255);
        return;
    }
    if (t->wait != W_GALLERY) return;
    Image *base = t->galBase[0] ? image_get(t, t->galBase) : NULL;
    Image *on = t->galOn[0] ? image_get(t, t->galOn) : NULL;
    if (base) blit(t->screen, t->W, t->H, base, 0, 0, 255);
    for (int i = 0; i < t->galCount; i++) {
        GalItem *g = &t->gal[i];
        if (g->type != 0x0a && g->type != 0x14) continue;
        if (item_locked(t, g)) {
            Image *nd = t->galNodata[0] ? image_get(t, t->galNodata) : NULL;
            if (nd) blit(t->screen, t->W, t->H, nd, g->x1, g->y1, 255);
        } else if (g->type == 0x0a && !t->galleryAll) {
            int seen, total = cg_counts(t, g, &seen);
            Image *inc = t->galIncomplete[0] ? image_get(t, t->galIncomplete) : NULL;
            if (seen < total && inc) blit(t->screen, t->W, t->H, inc, g->x1, g->y1, 255);
        }
    }
    if (t->hover >= 0 && t->hover < t->galCount && on) {
        GalItem *g = &t->gal[t->hover];
        if (!item_locked(t, g)) blit_part(t->screen, t->W, t->H, on, g->x1, g->y1, g->x2 - g->x1, g->y2 - g->y1, g->x1, g->y1, 255);
    }
}

void gallery_free(TH *t) {
    for (int i = 0; i < t->galCount; i++) free(t->gal[i].list);
    t->galCount = 0;
    free(t->galleryMarks);
    t->galleryMarks = NULL;
}
