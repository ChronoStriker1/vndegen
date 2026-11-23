#include "bsx.h"

#include <ctype.h>

#define SEC "dress screen"
#define DR_LINES 48
#define DR_LEVELS 8

typedef struct DressLine {
    int code, label;
    int lv[DR_LEVELS];
} DressLine;

struct DressPhase {
    Image *base, *base2, *figure;
    u32 *pageBg;
    MBtn modes[3], targets[BX_DRESS_CH], cstate[DR_LEVELS], back, bulkBtn, levels[24];
    MBtn costumes[DR_LINES];
    MBtn views[3], knob;
    int meter[4];
    bool drag;
    int targetCount;
    int ngng[DR_LEVELS], ngngCount;
    int bodyX, bodyY, figureX, figureY;
    int mode, page, item, chosen;
    DressLine lines[DR_LINES], bulk;
    int lineCount;
};

bool bx_dress_available(BX *b) { return bx_cfg_section(&b->cfg, SEC); }

static Image *own(BX *b, const char *key) {
    const char *v = bx_cfg_get(&b->cfg, SEC, key);
    char name[64];
    if (!v || bx_cfg_field(v, 0, name, sizeof name) <= 0) return NULL;
    Image *im = bx_image_load(b, name);
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static bool body_exists(BX *b, const char *name) {
    char body[16];
    snprintf(body, sizeof body, "%.10s", name);
    return bx_res_find(&b->arc, RX_GFX, body);
}

static bool with_under(BX *b, char *alt, char keep) {
    alt[8] = keep;
    if (body_exists(b, alt)) return true;
    for (char u = '1'; u <= '9'; u++) {
        alt[8] = u;
        if (body_exists(b, alt)) return true;
    }
    alt[8] = keep;
    return false;
}

static int wanted(const BX *b, int ch, int outfit) {
    if (b->dressAll) return b->dressAll - 1;
    if (ch < 1 || ch > BX_DRESS_CH) return 0;
    const u8 *r = b->dressRec[ch - 1];
    if (r[0]) return r[0] - 1;
    return outfit >= 0 && outfit < BX_DRESS_WEAR ? r[outfit] : 0;
}

bool bx_dress_name(BX *b, const char *name, char *out) {
    size_t n = strlen(name);
    if (n < 10 || n > 12 || tolower((u8)name[0]) != 'b' || tolower((u8)name[1]) != 'u' || !bx_dress_available(b)) return false;
    for (int i = 2; i < 9; i++)
        if (i != 4 && !isdigit((u8)name[i])) return false;
    int ch = (name[2] - '0') * 10 + name[3] - '0', outfit = (name[6] - '0') * 10 + name[7] - '0';
    int want = clampi(wanted(b, ch, outfit), 0, 9), from = name[5] - '0';
    if (want <= from) return false;
    char alt[16];
    snprintf(alt, sizeof alt, "%s", name);
    for (int d = want; d > from; d--) {
        alt[5] = (char)('0' + d);
        if (with_under(b, alt, name[8])) {
            snprintf(out, 64, "%s", alt);
            return true;
        }
    }
    return false;
}

static bool parse_line(BX *b, int ch, int k, DressLine *l) {
    char key[32];
    snprintf(key, sizeof key, "ch%02d dress%02d", ch, k);
    int v[2 + DR_LEVELS] = {0};
    int n = bx_cfg_ints(&b->cfg, SEC, key, v, 2 + DR_LEVELS);
    if (n < 3) return false;
    memset(l, 0, sizeof *l);
    l->code = v[0];
    l->label = v[1];
    for (int i = 0; i < DR_LEVELS && i + 2 < n; i++) l->lv[i] = v[i + 2];
    return true;
}

static void page_free(DressPhase *d) {
    for (int i = 0; i < d->lineCount; i++) bx_mbtn_free(&d->costumes[i]);
    d->lineCount = 0;
    d->figure = NULL;
}

static int shown_outfit(BX *b, int ch, char *under) {
    for (int i = 0; i < BX_OBJS; i++) {
        const Obj *o = &b->scene.obj[i];
        if (!o->used || o->kind != OK_CHAR || strlen(o->name) < 10 || atoi((char[3]){o->name[2], o->name[3], 0}) != ch) continue;
        if (under) *under = o->name[8];
        return (o->name[6] - '0') * 10 + o->name[7] - '0';
    }
    return -1;
}

static int digit_of(const DressPhase *d, int row) { return row < d->ngngCount ? d->ngng[row] : row; }

static int item_digit(BX *b, const DressPhase *d) {
    const u8 *r = b->dressRec[d->page];
    if (d->item < 0) return r[0] ? r[0] - 1 : -1;
    int code = d->lines[d->item].code;
    return code >= 0 && code < BX_DRESS_WEAR ? r[code] : 0;
}

static void load_figure(BX *b, DressPhase *d) {
    d->figure = NULL;
    if (d->item < 0) return;
    int ch = d->page + 1, code = d->lines[d->item].code;
    char under = '1';
    shown_outfit(b, ch, &under);
    char name[32];
    snprintf(name, sizeof name, "co%02d%d%02d%c", ch, clampi(item_digit(b, d), 0, 9), code, under);
    if (!bx_res_find(&b->arc, RX_GFX, name))
        for (char u = '1'; u <= '9'; u++) {
            name[7] = u;
            if (bx_res_find(&b->arc, RX_GFX, name)) break;
        }
    u8 head[0x1a];
    if (!bx_res_head(&b->arc, RX_GFX, name, head, sizeof head)) return;
    d->figure = bx_image_load(b, name);
    d->figureX = (b->W - rd16(head + 0x16)) / 2 + d->bodyX;
    d->figureY = (b->H - rd16(head + 0x18)) / 2 + d->bodyY;
}

static void open_page(BX *b, DressPhase *d, int target) {
    page_free(d);
    d->page = d->chosen = target;
    int ch = target + 1;
    if (!d->pageBg) d->pageBg = bx_alloc(sizeof(u32) * (size_t)b->W * (size_t)b->H);
    Scene sc = b->shown;
    for (int i = 0; i < BX_OBJS; i++)
        if (sc.obj[i].kind == OK_CHAR) sc.obj[i].used = false;
    bx_compose(b, &sc, d->pageBg);
    bool bulk = false;
    for (int k = 1; k < 100 && d->lineCount < DR_LINES; k++) {
        DressLine l;
        if (!parse_line(b, ch, k, &l)) break;
        if (!l.code) {
            d->bulk = l;
            bulk = true;
            continue;
        }
        char key[32];
        snprintf(key, sizeof key, "dress%02d", l.label);
        int pos[2] = {0, 0};
        bx_cfg_ints(&b->cfg, SEC, "dress pos", pos, 2);
        int space[2] = {0, 0};
        bx_cfg_ints(&b->cfg, SEC, "dress space", space, 2);
        int i = d->lineCount;
        MBtn t;
        if (!bx_mbtn_cfg(b, &t, SEC, key, 0, 0)) continue;
        t.x = pos[0] + space[0] * i;
        t.y = pos[1] + space[1] * i;
        d->costumes[i] = t;
        d->lines[d->lineCount++] = l;
    }
    if (!bulk) {
        memset(&d->bulk, 0, sizeof d->bulk);
        for (int i = 0; i < d->ngngCount; i++) d->bulk.lv[i] = i + 1;
    }
    d->item = -1;
    int worn = shown_outfit(b, ch, NULL);
    if (!b->dressRec[target][0])
        for (int i = 0; i < d->lineCount; i++)
            if (d->lines[i].code == worn && bx_museum_worn(b, ch, worn)) d->item = i;
    load_figure(b, d);
}

void bx_dress_free(BX *b) {
    DressPhase *d = b->dressPhase;
    if (!d) return;
    page_free(d);
    free(d->pageBg);
    bx_image_free(d->base);
    bx_image_free(d->base2);
    for (int i = 0; i < 3; i++) bx_mbtn_free(&d->modes[i]);
    for (int i = 0; i < BX_DRESS_CH; i++) bx_mbtn_free(&d->targets[i]);
    for (int i = 0; i < DR_LEVELS; i++) bx_mbtn_free(&d->cstate[i]);
    for (int i = 0; i < 24; i++) bx_mbtn_free(&d->levels[i]);
    bx_mbtn_free(&d->back);
    bx_mbtn_free(&d->bulkBtn);
    for (int i = 0; i < 3; i++) bx_mbtn_free(&d->views[i]);
    bx_mbtn_free(&d->knob);
    free(d);
    b->dressPhase = NULL;
}

bool bx_dress_open(BX *b) {
    if (!bx_dress_available(b) || (b->replaying && !bx_cfg_int(&b->cfg, SEC, "enable in replay mode", 0))) return false;
    bx_dress_free(b);
    DressPhase *d = bx_alloc(sizeof *d);
    b->dressPhase = d;
    d->base = own(b, "base");
    d->base2 = own(b, "base2");
    for (int i = 0; i < 3; i++) {
        char key[16];
        snprintf(key, sizeof key, "mode%d", i);
        bx_mbtn_cfg(b, &d->modes[i], SEC, key, 0, 0);
    }
    for (int i = 0; i < BX_DRESS_CH; i++) {
        char key[16];
        snprintf(key, sizeof key, "target%d", i);
        if (!bx_mbtn_cfg(b, &d->targets[i], SEC, key, 0, 0)) break;
        d->targetCount = i + 1;
    }
    if (bx_cfg_int(&b->cfg, SEC, "show common button", 1))
        for (int i = 0; i < DR_LEVELS; i++) {
            char key[24];
            snprintf(key, sizeof key, "common state%d", i);
            bx_mbtn_cfg(b, &d->cstate[i], SEC, key, 0, 0);
        }
    for (int i = 1; i < 24; i++) {
        char key[16];
        snprintf(key, sizeof key, "level%d", i);
        bx_mbtn_cfg(b, &d->levels[i], SEC, key, 0, 0);
    }
    bx_mbtn_cfg(b, &d->back, SEC, "back button", 0, 0);
    if (bx_cfg_int(&b->cfg, SEC, "enable view control", 0) && bx_cfg_int(&b->cfg, "free-sight screen", "mode", 0)) {
        static const char *keys[3] = {"face button", "bust button", "hip button"};
        for (int i = 0; i < 3; i++) bx_mbtn_cfg(b, &d->views[i], "free-sight screen", keys[i], 0, 0);
        bx_mbtn_cfg(b, &d->knob, "free-sight screen", "knob", 0, 0);
        bx_cfg_ints(&b->cfg, "free-sight screen", "meter region", d->meter, 4);
    }
    bx_mbtn_cfg(b, &d->bulkBtn, SEC, "dress00", 0, 0);
    d->ngngCount = bx_cfg_ints(&b->cfg, SEC, "ngng convert table", d->ngng, DR_LEVELS);
    int pos[2] = {0, 0};
    bx_cfg_ints(&b->cfg, SEC, "whole body pos", pos, 2);
    d->bodyX = pos[0];
    d->bodyY = pos[1];
    d->mode = b->dressAll ? 1 : 0;
    d->page = d->chosen = -1;
    b->phaseBack = b->phase;
    b->phase = PH_DRESS;
    return true;
}

static void close_dress(BX *b) {
    bx_dress_free(b);
    b->phase = b->phaseBack;
    b->phaseBack = PH_NONE;
}

static void changed(BX *b, DressPhase *d) {
    b->shownDirty = true;
    if (d->page >= 0) load_figure(b, d);
}

static void page_click(BX *b, DressPhase *d) {
    u8 *r = b->dressRec[d->page];
    int ch = d->page + 1;
    if (bx_mbtn_on(b, &d->bulkBtn)) {
        d->item = -1;
        if (!r[0]) r[0] = 1;
        changed(b, d);
        return;
    }
    for (int i = 0; i < d->lineCount; i++)
        if (bx_mbtn_on(b, &d->costumes[i]) && bx_museum_worn(b, ch, d->lines[i].code)) {
            d->item = i;
            r[0] = 0;
            changed(b, d);
            return;
        }
    const DressLine *l = d->item < 0 ? &d->bulk : &d->lines[d->item];
    for (int row = 0; row < DR_LEVELS; row++) {
        int id = l->lv[row];
        if (id <= 0 || id >= 24 || !bx_mbtn_on(b, &d->levels[id])) continue;
        int digit = digit_of(d, row);
        if (d->item < 0) r[0] = (u8)(digit + 1);
        else if (l->code < BX_DRESS_WEAR) r[l->code] = (u8)digit;
        changed(b, d);
        return;
    }
}

static bool on_meter(const BX *b, const DressPhase *d) {
    int half = d->knob.img ? d->knob.img->h / d->knob.frames / 2 : 0;
    return d->knob.img && d->meter[3] > d->meter[1] && b->mx >= d->meter[0] && b->mx < d->meter[2] && b->my >= d->meter[1] - half && b->my <= d->meter[3] + half;
}

static void meter_update(BX *b, DressPhase *d) {
    if (d->page >= 0 || !d->knob.img) {
        d->drag = false;
        return;
    }
    if (b->lPress && on_meter(b, d)) d->drag = true;
    if (!b->lDown) d->drag = false;
    if (!d->drag) return;
    float t = (float)(b->my - d->meter[1]) / (float)(d->meter[3] - d->meter[1]);
    b->sight = t < 0 ? 0 : t > 1 ? 1 : t;
    b->viewLevel = 4;
    b->shownDirty = true;
}

void bx_dress_update(BX *b) {
    DressPhase *d = b->dressPhase;
    if (!d) return;
    meter_update(b, d);
    bool click = b->click;
    b->click = false;
    if (b->rclick || (click && bx_mbtn_on(b, &d->back))) {
        b->rclick = false;
        if (d->page >= 0) {
            page_free(d);
            d->page = -1;
        } else close_dress(b);
        return;
    }
    if (!click || on_meter(b, d)) return;
    for (int i = 0; i < 3 && d->page < 0; i++)
        if (bx_mbtn_on(b, &d->views[i])) {
            bx_view_set(b, i + 1);
            return;
        }
    if (bx_mbtn_on(b, &d->modes[0])) {
        if (d->mode) {
            d->mode = 0;
            b->dressAll = 0;
            b->shownDirty = true;
        }
        return;
    }
    if (bx_mbtn_on(b, &d->modes[1]) && d->cstate[0].img) {
        if (!d->mode) {
            page_free(d);
            d->page = -1;
            d->mode = 1;
            if (!b->dressAll) b->dressAll = 1;
            b->shownDirty = true;
        }
        return;
    }
    if (d->mode) {
        for (int i = 0; i < DR_LEVELS; i++)
            if (bx_mbtn_on(b, &d->cstate[i])) {
                b->dressAll = (u8)(i + 1);
                b->shownDirty = true;
                return;
            }
        return;
    }
    for (int i = 0; i < d->targetCount; i++)
        if (bx_mbtn_on(b, &d->targets[i])) {
            open_page(b, d, i);
            return;
        }
    if (d->page >= 0) page_click(b, d);
}

static int state(BX *b, const MBtn *t, bool on) { return on ? 2 : bx_mbtn_on(b, t) ? 1 : 0; }

void bx_dress_draw(BX *b, u32 *dst) {
    DressPhase *d = b->dressPhase;
    if (!d) return;
    if (d->page >= 0 && d->pageBg) memcpy(dst, d->pageBg, sizeof(u32) * (size_t)b->W * (size_t)b->H);
    Image *base = d->page >= 0 && d->base2 ? d->base2 : d->base;
    if (base) bx_blit(dst, b->W, b->H, base, base->x, base->y, 255);
    if (d->page >= 0 && d->figure) bx_blit(dst, b->W, b->H, d->figure, d->figureX + d->figure->x, d->figureY + d->figure->y, 255);
    bx_mbtn_draw(b, dst, &d->modes[2], state(b, &d->modes[2], d->page < 0));
    bx_mbtn_draw(b, dst, &d->modes[1], state(b, &d->modes[1], d->mode == 1));
    bx_mbtn_draw(b, dst, &d->modes[0], state(b, &d->modes[0], d->mode == 0));
    if (d->mode)
        for (int i = 0; i < DR_LEVELS; i++) bx_mbtn_draw(b, dst, &d->cstate[i], state(b, &d->cstate[i], b->dressAll == i + 1));
    else
        for (int i = 0; i < d->targetCount; i++) bx_mbtn_draw(b, dst, &d->targets[i], state(b, &d->targets[i], d->chosen == i));
    bx_mbtn_draw(b, dst, &d->back, bx_mbtn_on(b, &d->back) ? 1 : 0);
    if (d->page < 0) {
        for (int i = 0; i < 3; i++) bx_mbtn_draw(b, dst, &d->views[i], state(b, &d->views[i], bx_view_level(b) == i + 1));
        MBtn k = d->knob;
        k.y += (int)(b->sight * (float)(d->meter[3] - d->meter[1]) + 0.5f);
        bx_mbtn_draw(b, dst, &k, 0);
        return;
    }
    int ch = d->page + 1;
    bx_mbtn_draw(b, dst, &d->bulkBtn, state(b, &d->bulkBtn, d->item < 0));
    for (int i = 0; i < d->lineCount; i++) {
        bool seen = bx_museum_worn(b, ch, d->lines[i].code);
        bx_mbtn_draw(b, dst, &d->costumes[i], seen ? state(b, &d->costumes[i], d->item == i) : 3);
    }
    const DressLine *l = d->item < 0 ? &d->bulk : &d->lines[d->item];
    int digit = item_digit(b, d);
    for (int row = 0; row < DR_LEVELS; row++) {
        int id = l->lv[row];
        if (id > 0 && id < 24) bx_mbtn_draw(b, dst, &d->levels[id], state(b, &d->levels[id], digit_of(d, row) == digit));
    }
}
