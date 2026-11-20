#include "br.h"

typedef struct SelCfg {
    int x, y, w, base, itemW, itemH, gap, ox, oy;
    u32 win, cursor, text, edge;
    int winA, cursorA;
    bool edgeOn;
} SelCfg;

static int kv(Node *en, const char *k, int d) { return sx_int(sx_cdr_value(sx_find(en, k)), d); }

static SelCfg sel_cfg(BR *e, int id) {
    Node *en = sx_assoc(sx_find(e->cfg.root, ":select-window"), id);
    if (!en) en = sx_assoc(sx_find(e->cfg.root, ":select-window"), 0);
    SelCfg c;
    c.x = kv(en, "window-x", 50);
    c.y = kv(en, "window-y", 100);
    c.w = kv(en, "window-width", 700);
    c.base = kv(en, "window-height-base", 20);
    c.itemW = kv(en, "item-width", 680);
    c.itemH = kv(en, "item-height", 32);
    c.gap = kv(en, "item-height-gap", 4);
    c.ox = kv(en, "item-offset-x", 10);
    c.oy = kv(en, "item-offset-y", 10);
    c.win = (u32)kv(en, "window-color-r", 0) | (u32)kv(en, "window-color-g", 120) << 8 | (u32)kv(en, "window-color-b", 120) << 16;
    c.winA = kv(en, "window-color-a", 255);
    c.cursor = (u32)kv(en, "cursor-color-r", 255) | (u32)kv(en, "cursor-color-g", 255) << 8 | (u32)kv(en, "cursor-color-b", 255) << 16;
    c.cursorA = kv(en, "cursor-color-a", 178);
    c.text = (u32)kv(en, "item-text-color.r", 255) | (u32)kv(en, "item-text-color.g", 255) << 8 | (u32)kv(en, "item-text-color.b", 255) << 16;
    c.edge = (u32)kv(en, "item-text-edge-color.r", 0) | (u32)kv(en, "item-text-edge-color.g", 0) << 8 | (u32)kv(en, "item-text-edge-color.b", 0) << 16;
    c.edgeOn = kv(en, "item-text-edge-color.a", 255) > 0;
    return c;
}

void choice_start(BR *e, Node *call) {
    Choice *ch = &e->choice;
    for (int i = 0; i < ch->count; i++) free(ch->item[i]);
    memset(ch, 0, sizeof *ch);
    for (int i = 0; i < 8; i++) {
        Node *a = i + 3 < call->n ? call->k[i + 3] : NULL;
        if (!a || a->t != N_STR || !a->s[0]) continue;
        ch->item[ch->count] = br_strdup(a->s);
        ch->index[ch->count++] = i;
    }
    ch->hover = -1;
    ch->active = ch->count > 0;
    if (ch->active) e->wait = W_CHOICE;
}

static int item_at(BR *e, SelCfg *c) {
    Choice *ch = &e->choice;
    for (int i = 0; i < ch->count; i++) {
        int y = c->y + c->oy + i * (c->itemH + c->gap);
        if (e->mouseX >= c->x + c->ox && e->mouseX < c->x + c->ox + c->itemW && e->mouseY >= y && e->mouseY < y + c->itemH) return i;
    }
    return -1;
}

void choice_tick(BR *e) {
    Choice *ch = &e->choice;
    SelCfg c = sel_cfg(e, 0);
    int h = item_at(e, &c);
    if (h != ch->hover && h >= 0) snd_se_hover(e);
    ch->hover = h;
    if (e->lPress && h >= 0) {
        e->reflag = ch->index[h];
        ch->active = false;
        e->wait = W_NONE;
    }
}

void choice_draw(BR *e, u32 *dst) {
    Choice *ch = &e->choice;
    if (!ch->active || e->wait != W_CHOICE) return;
    SelCfg c = sel_cfg(e, 0);
    int h = c.base + ch->count * (c.itemH + c.gap) - c.gap;
    fill_rect(dst, BR_W, BR_H, c.x, c.y, c.w, h, c.win, c.winA);
    int size = c.itemH;
    for (int i = 0; i < ch->count; i++) {
        int y = c.y + c.oy + i * (c.itemH + c.gap);
        if (i == ch->hover) fill_rect(dst, BR_W, BR_H, c.x + c.ox, y, c.itemW, c.itemH, c.cursor, c.cursorA);
        font_draw(e, dst, BR_W, BR_H, ch->item[i], c.x + c.ox, y, size, c.text, c.edge, c.edgeOn ? 1 : 0, 255);
    }
}

void choice_free(BR *e) {
    for (int i = 0; i < e->choice.count; i++) free(e->choice.item[i]);
    memset(&e->choice, 0, sizeof e->choice);
}
