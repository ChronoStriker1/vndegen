#include "bsx.h"

typedef struct BgvChar {
    char sig[8];
    MBtn stop, min, max;
    MBar vol;
    Image *bar;
    int barX, barY;
    int view[4], offset[2], step, lines, top;
    char **labels, **voices;
    int count;
} BgvChar;

struct BgvPanel {
    LNode *layout;
    Image *base;
    MBtn close, stop, min, max;
    MBar vol;
    int font, cell;
    BgvChar ch[8];
    int count;
    int drag, grab;
    bool overlay;
};

enum { DRAG_NONE = -1, DRAG_VOL = 100, DRAG_SCROLL = 200 };

static Image *copy_image(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static void load_bar(BX *b, MBar *r, const LNode *n) {
    int pos[2] = {0, 0}, hit[4];
    bx_layout_ints(n, "pos", pos, 2);
    Image *img = copy_image(b, bx_layout_str(n, "name"));
    bx_mbar_set(r, img, pos[0], pos[1], bx_layout_ints(n, "hit_region", hit, 4) == 4 ? hit : NULL);
}

static void load_char(BX *b, BgvChar *c, const LNode *n) {
    snprintf(c->sig, sizeof c->sig, "%s", bx_layout_str(n, "signature") ? bx_layout_str(n, "signature") : "");
    const LNode *ctl = bx_layout_get(n, "control");
    bx_mbtn_node(b, &c->stop, bx_layout_get(ctl, "stop"), 0, 0);
    bx_mbtn_node(b, &c->min, bx_layout_get(ctl, "min"), 0, 0);
    bx_mbtn_node(b, &c->max, bx_layout_get(ctl, "max"), 0, 0);
    load_bar(b, &c->vol, bx_layout_get(ctl, "volume"));
    const LNode *sb = bx_layout_get(ctl, "scroll_bar");
    int pos[2] = {0, 0};
    bx_layout_ints(sb, "pos", pos, 2);
    c->bar = copy_image(b, bx_layout_str(sb, "name"));
    c->barX = pos[0];
    c->barY = pos[1];
    const LNode *view = bx_layout_get(n, "view");
    bx_layout_ints(view, "pos", c->view, 4);
    bx_layout_ints(view, "offset", c->offset, 2);
    int iv[2] = {0, 27};
    bx_layout_ints(view, "interval", iv, 2);
    c->step = iv[1] > 0 ? iv[1] : 27;
    c->lines = clampi(bx_layout_int(view, "lines", 25), 1, 200);
    const LNode *list = bx_layout_get(n, "list");
    c->count = list ? list->count : 0;
    c->labels = bx_alloc(sizeof(char *) * (size_t)(c->count ? c->count : 1));
    c->voices = bx_alloc(sizeof(char *) * (size_t)(c->count ? c->count : 1));
    for (int i = 0; i < c->count; i++) {
        c->labels[i] = list->kids[i].key ? list->kids[i].key : "";
        c->voices[i] = list->kids[i].str ? list->kids[i].str : "";
    }
}

BgvPanel *bx_bgv_open(BX *b, bool overlay) {
    const char *name = bx_cfg_get(&b->cfg, "museum phase - bgv", "layout");
    LNode *root = bx_layout_load(b, name);
    if (!root) return NULL;
    BgvPanel *p = bx_alloc(sizeof *p);
    p->layout = root;
    p->overlay = overlay;
    p->drag = DRAG_NONE;
    const LNode *ov = bx_layout_get(root, "overlay_mode");
    const char *base = bx_cfg_get(&b->cfg, "museum phase - bgv", "base");
    p->base = copy_image(b, base ? base : bx_layout_str(ov, "base"));
    if (overlay) bx_mbtn_node(b, &p->close, bx_layout_get(ov, "close"), 0, 0);
    const LNode *ctl = bx_layout_get(root, "control");
    bx_mbtn_node(b, &p->stop, bx_layout_get(ctl, "stop"), 0, 0);
    bx_mbtn_node(b, &p->min, bx_layout_get(ctl, "min"), 0, 0);
    bx_mbtn_node(b, &p->max, bx_layout_get(ctl, "max"), 0, 0);
    load_bar(b, &p->vol, bx_layout_get(ctl, "volume"));
    int block[2] = {22, 22};
    bx_layout_ints(bx_layout_get(root, "font"), "block", block, 2);
    p->font = block[1] * 6 / 7;
    p->cell = block[0] / 2;
    const LNode *chars = bx_layout_get(root, "characters");
    for (int i = 0; chars && i < chars->count && p->count < 8; i++) load_char(b, &p->ch[p->count++], &chars->kids[i]);
    return p;
}

void bx_bgv_free(BgvPanel *p) {
    if (!p) return;
    for (int i = 0; i < p->count; i++) {
        BgvChar *c = &p->ch[i];
        bx_mbtn_free(&c->stop);
        bx_mbtn_free(&c->min);
        bx_mbtn_free(&c->max);
        bx_image_free(c->vol.img);
        bx_image_free(c->bar);
        free(c->labels);
        free(c->voices);
    }
    bx_mbtn_free(&p->close);
    bx_mbtn_free(&p->stop);
    bx_mbtn_free(&p->min);
    bx_mbtn_free(&p->max);
    bx_image_free(p->vol.img);
    bx_image_free(p->base);
    bx_layout_free(p->layout);
    free(p);
}

static void apply_level(BX *b, const BgvChar *c, int i) { bx_snd_loop_level(b, CH_BGV, c->sig, (float)b->bgvLevel[i] / 100.0f); }

static int row_at(const BX *b, const BgvChar *c) {
    if (b->mx < c->view[0] || b->mx >= c->view[0] + c->view[2] || b->my < c->view[1]) return -1;
    int r = (b->my - c->view[1]) / c->step;
    if (r >= c->lines || (b->my - c->view[1]) % c->step >= c->view[3]) return -1;
    r += c->top;
    return r < c->count ? r : -1;
}

static int knob(const BgvChar *c, int *y) {
    int track = c->bar ? c->bar->h : c->lines * c->step;
    int h = c->count > c->lines ? track * c->lines / c->count : track;
    if (y) *y = c->barY + (c->count ? track * c->top / c->count : 0);
    return h < 12 ? 12 : h;
}

static bool on_bar(const BX *b, const BgvChar *c) {
    if (!c->bar) return false;
    return b->mx >= c->barX && b->mx < c->barX + c->bar->w && b->my >= c->barY && b->my < c->barY + c->bar->h;
}

static void scroll_to(BgvChar *c, int top) { c->top = clampi(top, 0, c->count > c->lines ? c->count - c->lines : 0); }

static void set_volume(BX *b, int v) {
    b->volBgv = clampi(v, 0, 100);
    bx_settings_apply(b);
}

bool bx_bgv_update(BX *b, BgvPanel *p) {
    if (p->drag != DRAG_NONE) {
        if (!b->lDown) {
            if (p->drag == DRAG_VOL) bx_settings_save(b);
            p->drag = DRAG_NONE;
        } else if (p->drag == DRAG_VOL) set_volume(b, bx_mbar_value(b, &p->vol));
        else if (p->drag >= DRAG_SCROLL) {
            BgvChar *c = &p->ch[p->drag - DRAG_SCROLL];
            int track = c->bar ? c->bar->h : 1;
            scroll_to(c, (b->my - p->grab - c->barY) * c->count / (track > 0 ? track : 1));
        } else {
            int i = p->drag;
            b->bgvLevel[i] = bx_mbar_value(b, &p->ch[i].vol);
            apply_level(b, &p->ch[i], i);
        }
    }
    if (b->wheel)
        for (int i = 0; i < p->count; i++) {
            BgvChar *c = &p->ch[i];
            if (b->mx >= c->view[0] && b->mx < c->barX + (c->bar ? c->bar->w : 0) + 4 && b->my >= c->view[1] && b->my < c->view[1] + c->lines * c->step) {
                scroll_to(c, c->top - b->wheel * 3);
                b->wheel = 0;
            }
        }
    if (b->lPress) {
        if (bx_mbar_on(b, &p->vol)) {
            p->drag = DRAG_VOL;
            set_volume(b, bx_mbar_value(b, &p->vol));
        }
        for (int i = 0; i < p->count; i++) {
            BgvChar *c = &p->ch[i];
            if (bx_mbar_on(b, &c->vol)) {
                p->drag = i;
                b->bgvLevel[i] = bx_mbar_value(b, &c->vol);
                apply_level(b, c, i);
            } else if (on_bar(b, c)) {
                int y, h = knob(c, &y);
                if (b->my < y || b->my >= y + h) scroll_to(c, c->top + (b->my < y ? -c->lines : c->lines));
                knob(c, &y);
                p->grab = clampi(b->my - y, 0, h);
                p->drag = DRAG_SCROLL + i;
            }
        }
    }
    if (b->rclick && p->overlay) {
        b->rclick = false;
        return true;
    }
    if (!b->click) return false;
    if (p->overlay && bx_mbtn_on(b, &p->close)) {
        b->click = false;
        bx_snd_play(b, CH_SYS, RX_SYS, "cancel", false, 0);
        return true;
    }
    if (bx_mbtn_on(b, &p->stop)) bx_snd_loop_stop(b, CH_BGV, NULL, 300);
    else if (bx_mbtn_on(b, &p->min)) set_volume(b, b->volBgv - 10);
    else if (bx_mbtn_on(b, &p->max)) set_volume(b, b->volBgv + 10);
    else {
        for (int i = 0; i < p->count; i++) {
            BgvChar *c = &p->ch[i];
            int r = row_at(b, c);
            if (r >= 0 && c->voices[r][0]) {
                bx_snd_loop(b, CH_BGV, c->sig, RX_VOICE, c->voices[r]);
                apply_level(b, c, i);
            } else if (bx_mbtn_on(b, &c->stop)) bx_snd_loop_stop(b, CH_BGV, c->sig, 300);
            else if (bx_mbtn_on(b, &c->min) || bx_mbtn_on(b, &c->max)) {
                b->bgvLevel[i] = clampi(b->bgvLevel[i] + (bx_mbtn_on(b, &c->min) ? -10 : 10), 0, 100);
                apply_level(b, c, i);
            } else continue;
            b->click = false;
            return false;
        }
        return false;
    }
    bx_settings_save(b);
    b->click = false;
    return false;
}

void bx_bgv_draw(BX *b, BgvPanel *p, u32 *dst) {
    if (p->base) bx_blit(dst, b->W, b->H, p->base, p->base->x, p->base->y, 255);
    for (int i = 0; i < p->count; i++) {
        BgvChar *c = &p->ch[i];
        int hover = p->drag == DRAG_NONE ? row_at(b, c) : -1;
        for (int k = 0; k < c->lines && c->top + k < c->count; k++) {
            int r = c->top + k;
            u32 color = r == hover ? 0xff0000ffu : 0xffffffffu;
            int x = c->view[0] + c->offset[0], y = c->view[1] + c->offset[1] + k * c->step;
            bx_text_draw(NULL, dst, b->W, b->H, c->labels[r], x, y, p->font, p->cell, 0xff000000u, 1, 0);
            bx_text_draw(NULL, dst, b->W, b->H, c->labels[r], x + 1, y, p->font, p->cell, 0xff000000u, 1, 0);
            bx_text_draw(NULL, dst, b->W, b->H, c->labels[r], x, y, p->font, p->cell, color, 0, 0);
            bx_text_draw(NULL, dst, b->W, b->H, c->labels[r], x + 1, y, p->font, p->cell, color, 0, 0);
        }
        if (c->bar) {
            int y, h = knob(c, &y), cap = h < 12 ? h / 2 : 6;
            bx_blit_part(dst, b->W, b->H, c->bar, 0, 0, c->bar->w, h - cap, c->barX, y, 255);
            bx_blit_part(dst, b->W, b->H, c->bar, 0, c->bar->h - cap, c->bar->w, cap, c->barX, y + h - cap, 255);
        }
        bx_mbar_draw(b, dst, &c->vol, b->bgvLevel[i]);
        bx_mbtn_draw(b, dst, &c->stop, bx_mbtn_state(b, &c->stop));
        bx_mbtn_draw(b, dst, &c->min, bx_mbtn_state(b, &c->min));
        bx_mbtn_draw(b, dst, &c->max, bx_mbtn_state(b, &c->max));
    }
    bx_mbar_draw(b, dst, &p->vol, b->volBgv);
    bx_mbtn_draw(b, dst, &p->stop, bx_mbtn_state(b, &p->stop));
    bx_mbtn_draw(b, dst, &p->min, bx_mbtn_state(b, &p->min));
    bx_mbtn_draw(b, dst, &p->max, bx_mbtn_state(b, &p->max));
    if (p->overlay) bx_mbtn_draw(b, dst, &p->close, bx_mbtn_state(b, &p->close));
}
