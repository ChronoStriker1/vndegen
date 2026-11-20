#include "strip.h"

static const u8 mesgwnd30[68] = {[4] = 7, [8] = 20, [12] = 0x50, [16] = 0xd6, [17] = 1, [36] = 7, [40] = 20, [44] = 0x80, [48] = 0xf8, [49] = 1};
static const u8 mesgwnd31[68] = {[4] = 10, [8] = 32, [12] = 254, [16] = 0x38, [17] = 2, [36] = 10, [40] = 32, [44] = 254, [48] = 0x59, [49] = 2};
static const u8 defaultFreemsg[4] = {0};

static Image *copy_image(BX *b, const char *name) {
    Image *im = name && *name ? bx_image_load(b, name) : NULL;
    if (!im) return NULL;
    Image *c = bx_image_new(im->w, im->h);
    c->x = im->x;
    c->y = im->y;
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return c;
}

static char *slot_path(BX *b, int k) { return bx_fmt("%s/save/buex_ds%02d.dat", b->saveDir, k); }

static void put32(u8 **p, u32 v) {
    wr32(*p, v);
    *p += 4;
}

static void put_tag(u8 **p, const char *tag) {
    size_t n = strlen(tag);
    memcpy(*p, tag, n);
    *p += n;
}

static size_t wear_table_size(BX *b) {
    size_t n = 4;
    for (int i = 1; i <= 32; i++) {
        char key[32];
        snprintf(key, sizeof key, "ch%d max wear no", i);
        if (!bx_cfg_get(&b->cfg, "bustup setting", key)) break;
        n += 4 + (size_t)clampi(bx_cfg_int(&b->cfg, "bustup setting", key, 0), 0, 254) + 1;
    }
    return n;
}

static u8 *default_wear_table(BX *b, size_t *size) {
    *size = wear_table_size(b);
    u8 *d = bx_alloc(*size), *p = d + 4;
    u32 count = 0;
    for (int i = 1; i <= 32; i++) {
        char key[32];
        snprintf(key, sizeof key, "ch%d max wear no", i);
        if (!bx_cfg_get(&b->cfg, "bustup setting", key)) break;
        u32 w = (u32)clampi(bx_cfg_int(&b->cfg, "bustup setting", key, 0), 0, 254) + 1;
        put32(&p, w);
        p += w;
        count++;
    }
    wr32(d, count);
    return d;
}

u8 *bx_strip_serialize(BX *b, Strip *s, size_t *size) {
    size_t wearSize;
    u8 *wear = s->wearTable ? NULL : default_wear_table(b, &wearSize);
    const u8 *wt = s->wearTable ? s->wearTable : wear;
    if (s->wearTable) wearSize = s->wearTableSize;
    const u8 *mw = s->mesgwnd ? s->mesgwnd : b->sc->minor ? mesgwnd31 : mesgwnd30;
    size_t mwSize = s->mesgwnd ? s->mesgwndSize : sizeof mesgwnd31;
    const u8 *fm = s->freemsg ? s->freemsg : defaultFreemsg;
    size_t fmSize = s->freemsg ? s->freemsgSize : sizeof defaultFreemsg;
    size_t head = (size_t)(b->sc->minor ? 16 : 12);
    size_t n = 4 + 9 + 4 + 40 * (size_t)s->charCount + 11 + mwSize + 11 + fmSize + 10 + head + wearSize;
    u8 *d = bx_alloc(n), *p = d;
    *p++ = (u8)s->bgGroup;
    *p++ = (u8)s->bgPage;
    *p++ = (u8)s->bgIndex;
    *p++ = 0;
    put_tag(&p, "--order--");
    put32(&p, (u32)s->charCount);
    for (int i = 0; i < s->charCount; i++) {
        BuState *c = &s->st[i];
        put32(&p, (u32)c->no);
        put32(&p, (u32)c->hidden);
        u32 f;
        memcpy(&f, &c->pos, 4);
        put32(&p, f);
        put32(&p, (u32)c->six);
        for (int k = 0; k < STRIP_ROWS; k++) put32(&p, (u32)*bx_strip_field(c, k));
    }
    put_tag(&p, "--mesgwnd--");
    memcpy(p, mw, mwSize);
    p += mwSize;
    put_tag(&p, "--freemsg--");
    memcpy(p, fm, fmSize);
    p += fmSize;
    put_tag(&p, "--buinfo--");
    *p++ = s->flags;
    memcpy(p, s->head, 3);
    p += 3;
    u32 f;
    memcpy(&f, &s->sight, 4);
    put32(&p, f);
    put32(&p, (u32)s->level);
    if (head == 16) put32(&p, 0);
    memcpy(p, wt, wearSize);
    free(wear);
    *size = n;
    return d;
}

static const u8 *find_tag(const u8 *d, size_t n, size_t from, const char *tag) {
    size_t t = strlen(tag);
    for (size_t k = from; k + t <= n; k++)
        if (!memcmp(d + k, tag, t)) return d + k;
    return NULL;
}

static u8 *dup_bytes(const u8 *p, size_t n) {
    u8 *d = bx_alloc(n ? n : 1);
    memcpy(d, p, n);
    return d;
}

bool bx_strip_deserialize(BX *b, Strip *s, const u8 *d, size_t n) {
    const u8 *order = find_tag(d, n, 0, "--order--"), *mw = find_tag(d, n, 0, "--mesgwnd--"), *fm = find_tag(d, n, 0, "--freemsg--"),
             *bu = find_tag(d, n, 0, "--buinfo--");
    size_t head = (size_t)(b->sc->minor ? 16 : 12);
    if (!order || !mw || !fm || !bu || order != d + 4 || mw > fm || fm > bu || bu + 10 + head > d + n) return false;
    u32 count = rd32(order + 9);
    if (count > STRIP_CHARS || order + 13 + 40 * (size_t)count > mw) return false;
    s->bgGroup = d[0];
    s->bgPage = d[1];
    s->bgIndex = d[2];
    BuState st[STRIP_CHARS];
    memset(st, 0, sizeof st);
    for (u32 i = 0; i < count; i++) {
        const u8 *r = order + 13 + 40 * i;
        BuState *c = &st[i];
        c->no = (int)rd32(r);
        c->hidden = (int)rd32(r + 4);
        u32 f = rd32(r + 8);
        memcpy(&c->pos, &f, 4);
        c->six = (int)rd32(r + 12);
        for (int k = 0; k < STRIP_ROWS; k++) *bx_strip_field(c, k) = (int)rd32(r + 16 + 4 * (size_t)k);
        if (c->no < 1 || c->no > s->charCount) return false;
    }
    for (u32 i = 0; i < count && (int)i < s->charCount; i++) s->st[i] = st[i];
    free(s->mesgwnd);
    free(s->freemsg);
    free(s->wearTable);
    s->mesgwndSize = (size_t)(fm - mw - 11);
    s->mesgwnd = dup_bytes(mw + 11, s->mesgwndSize);
    s->freemsgSize = (size_t)(bu - fm - 11);
    s->freemsg = dup_bytes(fm + 11, s->freemsgSize);
    const u8 *info = bu + 10;
    s->flags = info[0];
    memcpy(s->head, info + 1, 3);
    u32 f = rd32(info + 4);
    memcpy(&s->sight, &f, 4);
    s->sight = s->sight < 0 ? 0 : s->sight > 1 ? 1 : s->sight;
    s->level = clampi((int)rd32(info + 8), 0, 2);
    s->wearTableSize = (size_t)(d + n - (info + head));
    s->wearTable = dup_bytes(info + head, s->wearTableSize);
    s->dirty = true;
    return true;
}

static Image *thumb_from(const u8 *d, size_t n, int w, int h) {
    if (n < (size_t)w * (size_t)h * 4) return NULL;
    Image *t = bx_image_new(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const u8 *p = d + ((size_t)(h - 1 - y) * (size_t)w + (size_t)x) * 4;
            t->px[(size_t)y * w + x] = 0xff000000u | (u32)p[2] | (u32)p[1] << 8 | (u32)p[0] << 16;
        }
    return t;
}

static void load_slot_thumbs(BX *b, Strip *s) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    for (int k = 0; k < s->slotCount; k++) {
        bx_image_free(s->slotThumbs[k]);
        s->slotThumbs[k] = NULL;
        char *path = slot_path(b, k);
        size_t n;
        u8 *d = bx_read_file(path, &n);
        free(path);
        s->slotUsed[k] = d != NULL;
        if (d) s->slotThumbs[k] = thumb_from(d, n, tw, th);
        free(d);
    }
}

void bx_strip_data_open(BX *b, Strip *s) {
    const LNode *ds = bx_layout_get(s->layout, "data_store");
    s->dsBase = copy_image(b, bx_layout_str(ds, "base"));
    const LNode *cmd = bx_layout_get(ds, "edit_command");
    s->cmdBase = copy_image(b, bx_layout_str(bx_layout_get(cmd, "base"), "name"));
    bx_mbtn_node(b, &s->cmdSave, bx_layout_get(cmd, "save"), 0, 0);
    bx_mbtn_node(b, &s->cmdLoad, bx_layout_get(cmd, "load"), 0, 0);
    bx_mbtn_node(b, &s->cmdSlot, bx_layout_get(cmd, "toslot"), 0, 0);
    bx_mbtn_node(b, &s->output, bx_layout_get(cmd, "output"), 0, 0);
    bx_mbtn_node(b, &s->clear, bx_layout_get(cmd, "clear"), 0, 0);
    const LNode *tp = bx_layout_get(ds, "thumb_pos"), *sp = bx_layout_get(ds, "slot_pos");
    for (int i = 0; tp && i < tp->count && s->slotCount < STRIP_SLOTS; i++) bx_layout_ints(&tp->kids[i], NULL, s->slotThumbPos[s->slotCount++], 2);
    for (int i = 0; sp && i < sp->count && s->outCount < STRIP_OUTPUTS; i++) bx_layout_ints(&sp->kids[i], NULL, s->outPos[s->outCount++], 2);
    for (int i = 0; i < STRIP_OUTPUTS; i++) s->outputs[i] = -1;
    s->menuSlot = -1;
}

void bx_strip_data_close(Strip *s) {
    bx_image_free(s->dsBase);
    bx_image_free(s->cmdBase);
    MBtn *bs[] = {&s->cmdSave, &s->cmdLoad, &s->cmdSlot, &s->output, &s->clear};
    for (size_t i = 0; i < sizeof bs / sizeof *bs; i++) bx_mbtn_free(bs[i]);
    for (int k = 0; k < STRIP_SLOTS; k++) bx_image_free(s->slotThumbs[k]);
    free(s->mesgwnd);
    free(s->freemsg);
    free(s->wearTable);
}

void bx_strip_data_enter(BX *b, Strip *s) {
    s->menuSlot = -1;
    load_slot_thumbs(b, s);
}

static void save_slot(BX *b, Strip *s, int k) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    u32 *full = bx_alloc(sizeof(u32) * (size_t)b->W * (size_t)b->H);
    bx_strip_compose(b, s, full);
    Image *t = bx_scaled(b, full, tw, th);
    free(full);
    size_t stateSize;
    u8 *state = bx_strip_serialize(b, s, &stateSize);
    size_t px = (size_t)tw * (size_t)th * 4;
    u8 *d = bx_alloc(px + stateSize);
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            u32 c = t->px[(size_t)y * tw + x];
            u8 *p = d + ((size_t)(th - 1 - y) * (size_t)tw + (size_t)x) * 4;
            p[0] = (u8)(c >> 16);
            p[1] = (u8)(c >> 8);
            p[2] = (u8)c;
            p[3] = 0xff;
        }
    memcpy(d + px, state, stateSize);
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/save", b->saveDir);
    bx_mkdirs(dir);
    char *path = slot_path(b, k);
    if (bx_write_file(path, d, px + stateSize)) bx_log(b, "saved %s", path);
    free(path);
    free(d);
    free(state);
    bx_image_free(t);
    load_slot_thumbs(b, s);
    bx_snd_play(b, CH_SYS, RX_SYS, "done", false, 0);
}

static void load_slot(BX *b, Strip *s, int k) {
    char *path = slot_path(b, k);
    size_t n;
    u8 *d = bx_read_file(path, &n);
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    size_t px = (size_t)tw * (size_t)th * 4;
    if (!d || n <= px || !bx_strip_deserialize(b, s, d + px, n - px)) bx_log(b, "can't read %s", path);
    else bx_snd_play(b, CH_SYS, RX_SYS, "done", false, 0);
    free(d);
    free(path);
}

static int slot_at(BX *b, const Strip *s) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    for (int k = 0; k < s->slotCount; k++)
        if (b->mx >= s->slotThumbPos[k][0] && b->my >= s->slotThumbPos[k][1] && b->mx < s->slotThumbPos[k][0] + tw && b->my < s->slotThumbPos[k][1] + th)
            return k;
    return -1;
}

static void menu_buttons(BX *b, Strip *s) {
    int tw, th;
    bx_thumb_size(b, &tw, &th);
    int x = s->slotThumbPos[s->menuSlot][0], y = s->slotThumbPos[s->menuSlot][1] + th;
    if (s->cmdBase && y + s->cmdBase->h > b->H) y = s->slotThumbPos[s->menuSlot][1] - s->cmdBase->h;
    MBtn *bs[] = {&s->cmdSave, &s->cmdLoad, &s->cmdSlot};
    for (int i = 0; i < 3; i++) {
        bs[i]->x = x;
        bs[i]->y = y + i * (bs[i]->img ? bs[i]->img->h / bs[i]->frames : 34);
    }
}

void bx_strip_data_update(BX *b, Strip *s) {
    if (b->rclick && s->menuSlot >= 0) {
        b->rclick = false;
        s->menuSlot = -1;
        return;
    }
    if (!b->click) return;
    b->click = false;
    if (s->menuSlot >= 0) {
        int k = s->menuSlot;
        menu_buttons(b, s);
        s->menuSlot = -1;
        if (bx_mbtn_on(b, &s->cmdSave)) save_slot(b, s, k);
        else if (bx_mbtn_on(b, &s->cmdLoad)) load_slot(b, s, k);
        else if (bx_mbtn_on(b, &s->cmdSlot)) {
            for (int i = 0; i < s->outCount; i++)
                if (s->outputs[i] < 0) {
                    s->outputs[i] = k;
                    break;
                }
        }
        return;
    }
    if (bx_mbtn_on(b, &s->clear)) {
        for (int i = 0; i < STRIP_OUTPUTS; i++) s->outputs[i] = -1;
        return;
    }
    if (bx_mbtn_on(b, &s->output)) {
        bx_log(b, "image output isn't supported");
        return;
    }
    int k = slot_at(b, s);
    if (k < 0) return;
    if (!s->slotUsed[k]) save_slot(b, s, k);
    else s->menuSlot = k;
}

void bx_strip_data_draw(BX *b, Strip *s, u32 *dst) {
    if (s->dsBase) bx_blit(dst, b->W, b->H, s->dsBase, s->dsBase->x, s->dsBase->y, 255);
    for (int k = 0; k < s->slotCount; k++)
        if (s->slotThumbs[k]) bx_blit(dst, b->W, b->H, s->slotThumbs[k], s->slotThumbPos[k][0], s->slotThumbPos[k][1], 255);
    for (int i = 0; i < s->outCount; i++)
        if (s->outputs[i] >= 0 && s->slotThumbs[s->outputs[i]]) bx_blit(dst, b->W, b->H, s->slotThumbs[s->outputs[i]], s->outPos[i][0], s->outPos[i][1], 255);
    bx_mbtn_draw(b, dst, &s->output, bx_mbtn_state(b, &s->output));
    bx_mbtn_draw(b, dst, &s->clear, bx_mbtn_state(b, &s->clear));
    if (s->menuSlot < 0) return;
    menu_buttons(b, s);
    MBtn *bs[] = {&s->cmdSave, &s->cmdLoad, &s->cmdSlot};
    for (int i = 0; i < 3; i++) bx_mbtn_draw(b, dst, bs[i], bx_mbtn_state(b, bs[i]));
}
