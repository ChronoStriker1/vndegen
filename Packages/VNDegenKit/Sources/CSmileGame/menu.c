#include "sg.h"

#include <sys/stat.h>
#include <time.h>

enum {
    P_MAIN,
    P_ITEMS,
    P_ITEM_TARGET,
    P_SKILL_WHO,
    P_SKILLS,
    P_SKILL_TARGET,
    P_EQUIP_WHO,
    P_EQUIP,
    P_EQUIP_PICK,
    P_STATUS_WHO,
    P_STATUS,
    P_SAVE,
    P_SAVE_CONFIRM,
    P_QUIT,
    P_CONFIG,
    P_SHOP,
    P_SHOP_BUY,
    P_SHOP_SELL,
    P_INN,
    P_COUNT,
};

enum { MAIN_ITEMS, MAIN_SKILLS, MAIN_EQUIP, MAIN_STATUS, MAIN_SAVE, MAIN_QUIT, MAIN_CONFIG, MAIN_CLOSE, MAIN_COUNT };

enum { CONFIG_BGM, CONFIG_SE, CONFIG_SPEED, CONFIG_CURSOR, CONFIG_RESET, CONFIG_QUIT, CONFIG_ROWS };

enum { SLOTS_PER_PAGE = 8, SAVE_PAGES = 5, LIST_ROWS = 7 };

static const u32 kWhite = 0xffffffffu;
static const u32 kGray = 0xff808080u;
static const u32 kDown = 0xffb4a0ffu;
static const u32 kUp = 0xffffd090u;
static const int kOpenMs = 160;

struct Menu {
    int mode;
    int page;
    int cur[P_COUNT];
    int top[P_COUNT];
    int member;
    int savePage;
    u32 animMs;
    bool closing;
    bool quit;
    int bgm, se, speed, remember;
    Guid shopItems[32];
    i32 shopPrices[32];
    int shopCount;
    int innPrice;
    int qty;
    bool result;
    SaveSlot slots[SAVE_SLOTS];
    char note[256];
    Image *faces[8];
    Guid faceIds[8];
    Image *equipIcons, *guide, *arrow, *arrowSelected, *pageIcons;
    bool sysLoaded;
};

static Menu *menu_get(SG *g) {
    SgB *b = sgb(g);
    if (!b) return NULL;
    if (!b->menu) {
        b->menu = sg_alloc(sizeof(Menu));
        b->menu->bgm = 100;
        b->menu->se = 100;
        b->menu->speed = 1;
        b->menu->remember = 1;
    }
    return b->menu;
}

void menu_free(SgB *b) {
    Menu *m = b->menu;
    if (!m) return;
    for (int i = 0; i < 8; i++) img_free(m->faces[i]);
    img_free(m->equipIcons);
    img_free(m->guide);
    img_free(m->arrow);
    img_free(m->arrowSelected);
    img_free(m->pageIcons);
    free(m);
    b->menu = NULL;
}

static Image *pack_image(SG *g, const char *path) {
    size_t size = 0;
    u8 *data = pack_read(&g->data.pack, path, &size);
    Image *im = data ? img_decode(data, size) : NULL;
    free(data);
    if (!im) sg_log(g, "menu: image missing %s", path);
    return im;
}

static void load_system(SG *g, Menu *m) {
    if (m->sysLoaded) return;
    m->sysLoaded = true;
    m->equipIcons = pack_image(g, "samples/common/res/system/equip_icons.png");
    m->guide = pack_image(g, "samples/common/res/system/button_guide.png");
    m->arrow = pack_image(g, "samples/common/res/system/arrow.png");
    m->arrowSelected = pack_image(g, "samples/common/res/system/arrow_selected.png");
    m->pageIcons = pack_image(g, "samples/common/res/system/page_icons.png");
}

static const Res *window_res(SG *g, const char *name) {
    for (int i = 0; i < g->res.count; i++)
        if (g->res.items[i].kind == RES_WINDOW && strcmp(g->res.items[i].name, name) == 0) return &g->res.items[i];
    return NULL;
}

static void nine(SG *g, const Image *im, const i32 *border, int x, int y, int w, int h, int alpha) {
    int l = border[0], t = border[1], rr = border[2], b = border[3];
    if (w < l + rr) l = rr = w / 2;
    if (h < t + b) t = b = h / 2;
    int iw = im->w, ih = im->h;
    blit(g->fb, g->w, g->h, im, 0, 0, l, t, x, y, l, t, alpha);
    blit(g->fb, g->w, g->h, im, l, 0, iw - l - rr, t, x + l, y, w - l - rr, t, alpha);
    blit(g->fb, g->w, g->h, im, iw - rr, 0, rr, t, x + w - rr, y, rr, t, alpha);
    blit(g->fb, g->w, g->h, im, 0, t, l, ih - t - b, x, y + t, l, h - t - b, alpha);
    blit(g->fb, g->w, g->h, im, l, t, iw - l - rr, ih - t - b, x + l, y + t, w - l - rr, h - t - b, alpha);
    blit(g->fb, g->w, g->h, im, iw - rr, t, rr, ih - t - b, x + w - rr, y + t, rr, h - t - b, alpha);
    blit(g->fb, g->w, g->h, im, 0, ih - b, l, b, x, y + h - b, l, b, alpha);
    blit(g->fb, g->w, g->h, im, l, ih - b, iw - l - rr, b, x + l, y + h - b, w - l - rr, b, alpha);
    blit(g->fb, g->w, g->h, im, iw - rr, ih - b, rr, b, x + w - rr, y + h - b, rr, b, alpha);
}

static void window(SG *g, const char *name, int x, int y, int w, int h) {
    const Res *r = window_res(g, name);
    Image *im = r ? sg_image(g, r->id) : NULL;
    if (!im) {
        fill_rect(g->fb, g->w, g->h, x, y, w, h, 0xa0000000u);
        return;
    }
    nine(g, im, r->border, x, y, w, h, 255);
}

static void panel(SG *g, int x, int y, int w, int h) { window(g, "default", x, y, w, h); }
static void button(SG *g, int x, int y, int w, int h) { window(g, "default_selectable", x, y, w, h); }
static void highlight(SG *g, int x, int y, int w, int h) { window(g, "default_selected", x, y, w, h); }
static void strip(SG *g, int x, int y, int w, int h) { fill_rect(g->fb, g->w, g->h, x, y, w, h, 0x26ffffffu); }

static void text(SG *g, int x, int y, int px, const char *s, u32 color) {
    if (!s) return;
    font_draw(g->font, g->fb, g->w, g->h, x + 1, y + 1, px, s, 0x80000000u);
    font_draw(g->font, g->fb, g->w, g->h, x, y, px, s, color);
}

static void text_right(SG *g, int right, int y, int px, const char *s, u32 color) {
    if (s) text(g, right - font_width(g->font, px, s), y, px, s, color);
}

static void text_center(SG *g, int cx, int y, int px, const char *s, u32 color) {
    if (s) text(g, cx - font_width(g->font, px, s) / 2, y, px, s, color);
}

static void text_fit(SG *g, int cx, int y, int px, int maxW, const char *s, u32 color) {
    if (!s) return;
    int size = px;
    while (size > 12 && font_width(g->font, size, s) > maxW) size--;
    text_center(g, cx, y + (px - size) / 2, size, s, color);
}

static void textf_right(SG *g, int right, int y, int px, u32 color, const char *fmt, int v) {
    char buf[32];
    snprintf(buf, sizeof buf, fmt, v);
    text_right(g, right, y, px, buf, color);
}

static void icon(SG *g, Guid sheet, int ix, int iy, int x, int y) {
    Image *im = sg_image(g, sheet);
    if (im) blit(g->fb, g->w, g->h, im, ix * 32, iy * 32, 32, 32, x, y, 32, 32, 255);
}

static Tables *T(SG *g) { return &sgb(g)->tables; }
static Party *P(SG *g) { return &sgb(g)->party; }
static const char *MT(SG *g, int i) { return T(g)->text.menu[i] ? T(g)->text.menu[i] : ""; }

void sgb_sound(SG *g, int which) { snd_se(g, T(g)->text.sounds[which], 100, 100); }

static const SaveSlot *slot_info(Menu *m, int slot, char *date, size_t dateSize) {
    if (slot < 0 || slot >= SAVE_SLOTS || !m->slots[slot].used) return NULL;
    struct tm tm;
    localtime_r(&m->slots[slot].written, &tm);
    strftime(date, dateSize, "%Y/%m/%d %H:%M:%S", &tm);
    return &m->slots[slot];
}

static bool save_slot(SG *g, Menu *m, int slot) {
    bool ok = sg_save_write(g, slot);
    sg_save_slots(g, m->slots, SAVE_SLOTS);
    return ok;
}

static char *map_name(SG *g, Guid id) {
    const SgrRec *rec = game_record(&g->data, SGR_MAP, 0, id, NULL);
    if (!rec) return sg_strdup("");
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    char *name = rd_str(&r);
    return name ? name : sg_strdup("");
}

static Image *face_image(SG *g, Menu *m, Guid face) {
    if (guid_zero(face)) return NULL;
    int free_slot = -1;
    for (int i = 0; i < 8; i++) {
        if (m->faces[i] && guid_eq(m->faceIds[i], face)) return m->faces[i];
        if (!m->faces[i] && free_slot < 0) free_slot = i;
    }
    const Res *r = res_find(&g->res, face);
    if (!r || free_slot < 0) return NULL;
    const char *base = strrchr(r->path, '/');
    const char *bs = strrchr(r->path, '\\');
    if (bs && (!base || bs > base)) base = bs;
    char *path = sg_fmt("%s/%s_normal.png", r->path, base ? base + 1 : r->path);
    Image *im = pack_image(g, path);
    free(path);
    m->faces[free_slot] = im;
    m->faceIds[free_slot] = face;
    return im;
}

static const char *state_name(SG *g, const Member *mb) {
    if (mb->hp <= 0) return MT(g, MT_DOWN);
    if (mb->state == 1) return MT(g, MT_POISON);
    return MT(g, MT_NORMAL);
}

static void member_box(SG *g, Menu *m, const Member *mb, int x, int y, int lower) {
    const HeroDef *h = hero_def(T(g), mb->hero);
    if (!h) return;
    Image *face = face_image(g, m, h->face);
    if (face) {
        int sw = face->w - 60 < 352 ? face->w - 60 : 352;
        if (sw > 0) blit(g->fb, g->w, g->h, face, 60, 106, sw, 225, x + 8, y + 8, sw * 4 / 5, 180, 255);
    }
    strip(g, x + 8, y + 10, 282, 26);
    text_right(g, x + 280, y + 10, 18, h->name, kWhite);
    strip(g, x + 210, y + 44, 80, 20);
    text(g, x + 221, y + 42, 17, MT(g, MT_LV), kWhite);
    textf_right(g, x + 281, y + 42, 17, kWhite, "%d", mb->level);
    strip(g, x + 155, y + 89 + lower, 135, 100);
    char buf[64];
    y += lower;
    text(g, x + 161, y + 87, 17, MT(g, MT_HP), kWhite);
    snprintf(buf, sizeof buf, "%d", mb->hp);
    text(g, x + 166, y + 107, 17, buf, kWhite);
    text(g, x + 217, y + 107, 17, "/", kWhite);
    textf_right(g, x + 279, y + 107, 17, kWhite, "%d", member_max_hp(T(g), mb));
    text(g, x + 161, y + 127, 17, MT(g, MT_MP), kWhite);
    snprintf(buf, sizeof buf, "%d", mb->mp);
    text(g, x + 166, y + 147, 17, buf, kWhite);
    text(g, x + 217, y + 147, 17, "/", kWhite);
    textf_right(g, x + 279, y + 147, 17, kWhite, "%d", member_max_mp(T(g), mb));
    snprintf(buf, sizeof buf, "%s: %s", MT(g, MT_STATE), state_name(g, mb));
    text(g, x + 160, y + 167, 17, buf, kWhite);
}

static void member_grid(SG *g, Menu *m, int sel) {
    Party *p = P(g);
    for (int i = 0; i < p->count && i < 4; i++) {
        int x = 340 + (i % 2) * 305, y = 28 + (i / 2) * 198;
        if (i == sel) highlight(g, x, y, 298, 196);
        member_box(g, m, &p->members[i], x, y, 0);
    }
}

static void member_side(SG *g, Menu *m, const Member *mb) {
    panel(g, 8, 17, 312, 486);
    member_box(g, m, mb, 16, 23, 12);
}

static void guide_icon(SG *g, Menu *m, int col, int row, int x, int y) {
    if (m->guide) blit(g->fb, g->w, g->h, m->guide, col * 48, row * 32, 48, 32, x, y, 43, 29, 255);
}

static int guide_item(SG *g, Menu *m, int x, int c1, int r1, int c2, int r2, const char *label) {
    guide_icon(g, m, c1, r1, x, 514);
    text_outlined(g, x + 34, 512, 18, "/", kWhite);
    guide_icon(g, m, c2, r2, x + 40, 514);
    text_outlined(g, x + 88, 513, 18, label, kWhite);
    return 88 + font_width(g->font, 18, label);
}

static void guide(SG *g, Menu *m, bool status) {
    if (m->mode == MENU_TITLE) return;
    const char *labels[3] = {status ? MT(g, MT_CHOOSE_CHAR) : MT(g, MT_CURSOR), status ? MT(g, MT_CLOSE) : MT(g, MT_DECIDE),
                             T(g)->text.shop[SHOP_QUIT] ? T(g)->text.shop[SHOP_QUIT] : ""};
    static const int icons[3][4] = {{0, 3, 0, 0}, {1, 3, 2, 0}, {2, 3, 1, 0}};
    int w = 2 * 57;
    for (int i = 0; i < 3; i++) w += 88 + font_width(g->font, 18, labels[i]);
    int x = 495 - w / 2;
    for (int i = 0; i < 3; i++) {
        int c2 = i == 0 && status ? 1 : icons[i][2], r2 = i == 0 && status ? 2 : icons[i][3];
        x += guide_item(g, m, x, icons[i][0], icons[i][1], c2, r2, labels[i]) + 57;
    }
}

static void bottom_info(SG *g) {
    panel(g, 328, 436, 624, 66);
    const char *name = g->field.loaded && g->field.map.name ? g->field.map.name : "";
    text(g, 354, 453, 22, name, kWhite);
    char buf[96];
    snprintf(buf, sizeof buf, "%s : %dG", T(g)->text.money ? T(g)->text.money : "", P(g)->money);
    text(g, 754, 443, 17, buf, kWhite);
    u64 s = P(g)->playMs / 1000;
    snprintf(buf, sizeof buf, "%s %02d:%02d:%02d", T(g)->text.playTime ? T(g)->text.playTime : "", (int)(s / 3600), (int)(s / 60 % 60),
             (int)(s % 60));
    text(g, 754, 470, 17, buf, kWhite);
}

static void bottom_text(SG *g, const char *s) {
    panel(g, 328, 436, 624, 66);
    text_fit(g, 640, 452, 24, 580, s, kWhite);
}

static void quit_button(SG *g, int x, int y, int w, bool sel) {
    button(g, x, y, w, 48);
    if (sel) highlight(g, x, y, w, 48);
    text_center(g, x + w / 2, y + 12, 20, T(g)->text.shop[SHOP_QUIT], kWhite);
}

static void main_column(SG *g, Menu *m, int sel) {
    static const int labels[MAIN_COUNT] = {MT_ITEMS, MT_SKILLS, MT_EQUIP, MT_STATUS, MT_SAVE, MT_QUIT_GAME, -1, MT_CLOSE};
    panel(g, 60, 40, 208, 440);
    for (int i = 0; i < MAIN_COUNT; i++) {
        int y = 48 + 53 * i;
        button(g, 68, y, 192, 53);
        if (i == sel) highlight(g, 68, y, 192, 53);
        const char *s = labels[i] < 0 ? T(g)->text.config[CFG_TITLE] : MT(g, labels[i]);
        text(g, 78, y + 10, 21, s, kWhite);
    }
}

typedef struct List {
    int count;
    Guid ids[256];
    int counts[256];
} List;

static void item_list(SG *g, List *l, int kind) {
    Party *p = P(g);
    l->count = 0;
    for (int i = 0; i < p->itemCount && l->count < 256; i++) {
        const ItemDef *it = item_def(T(g), p->items[i].item);
        if (!it) continue;
        if (kind && it->kind != kind) continue;
        l->ids[l->count] = it->id;
        l->counts[l->count++] = p->items[i].count;
    }
}

static bool item_field_usable(const ItemDef *it) {
    return it && ((it->kind == ITEM_EXPENDABLE && it->usable[1]) || it->kind == ITEM_SKILLBOOK);
}

static void draw_entry(SG *g, Guid iconSheet, int ix, int iy, const char *name, const char *right, int x, int y, bool sel, bool dim) {
    button(g, x, y, 298, 44);
    if (sel) highlight(g, x, y, 298, 44);
    bool hasIcon = !guid_zero(iconSheet);
    if (hasIcon) icon(g, iconSheet, ix, iy, x + 8, y + 6);
    u32 c = dim ? kGray : kWhite;
    text(g, hasIcon ? x + 42 : x + 4, y + 10, 21, name, c);
    if (right) text_right(g, x + 288, y + 12, 17, right, c);
}

static int list_rows(int count) { return (count + 1) / 2; }

static void scroll_to(Menu *m, int page, int cur) {
    int row = cur / 2;
    if (row < m->top[page]) m->top[page] = row;
    if (row >= m->top[page] + LIST_ROWS) m->top[page] = row - LIST_ROWS + 1;
}

static void draw_item_page(SG *g, Menu *m, int page, const List *l, bool prices, bool sell) {
    panel(g, 328, 16, 624, 422);
    int cur = m->cur[page];
    for (int i = m->top[page] * 2; i < l->count && i < (m->top[page] + LIST_ROWS) * 2; i++) {
        const ItemDef *it = item_def(T(g), l->ids[i]);
        if (!it) continue;
        int row = i / 2 - m->top[page];
        char right[32];
        if (prices) snprintf(right, sizeof right, "%dG", sell ? it->price / 2 : m->shopPrices[i]);
        else snprintf(right, sizeof right, "%d", l->counts[i]);
        bool dim = page == P_ITEMS && !item_field_usable(it) && !(it->kind >= ITEM_WEAPON && it->kind <= ITEM_ACCESSORY);
        draw_entry(g, it->icon, it->iconX, it->iconY, it->name, right, 340 + (i % 2) * 305, 28 + row * 48, i == cur, dim);
    }
    if (l->count == 0) draw_entry(g, (Guid){{0}}, 0, 0, MT(g, MT_NO_ITEMS), NULL, 340, 28, cur == 0, false);
    quit_button(g, 490, 380, 300, cur == (l->count ? l->count : 1));
}

static void shop_list(SG *g, Menu *m, List *l) {
    l->count = 0;
    for (int i = 0; i < m->shopCount; i++) {
        l->ids[l->count] = m->shopItems[i];
        l->counts[l->count++] = 0;
    }
}

static void skill_list(SG *g, const Member *mb, List *l) {
    l->count = 0;
    const HeroDef *h = hero_def(T(g), mb->hero);
    if (!h) return;
    for (int i = 0; i < h->skillCount && l->count < 256; i++) {
        if (h->skills[i].level > mb->level) continue;
        l->ids[l->count] = h->skills[i].skill;
        l->counts[l->count++] = 0;
    }
    for (int i = 0; i < mb->learnedCount && l->count < 256; i++) {
        bool dup = false;
        for (int k = 0; k < l->count && !dup; k++) dup = guid_eq(l->ids[k], mb->learned[i]);
        if (dup) continue;
        l->ids[l->count] = mb->learned[i];
        l->counts[l->count++] = 0;
    }
}

static void draw_skills(SG *g, Menu *m) {
    Member *mb = &P(g)->members[m->member];
    member_side(g, m, mb);
    panel(g, 328, 16, 624, 422);
    List l;
    skill_list(g, mb, &l);
    int cur = m->cur[P_SKILLS];
    for (int i = m->top[P_SKILLS] * 2; i < l.count && i < (m->top[P_SKILLS] + LIST_ROWS) * 2; i++) {
        const SkillDef *s = skill_def(T(g), l.ids[i]);
        if (!s) continue;
        char right[16];
        snprintf(right, sizeof right, "%d", s->mpCost);
        int row = i / 2 - m->top[P_SKILLS];
        draw_entry(g, s->icon, s->iconX, s->iconY, s->name, right, 340 + (i % 2) * 305, 28 + row * 48, i == cur, !s->fieldUse);
    }
    if (l.count == 0) draw_entry(g, (Guid){{0}}, 0, 0, MT(g, MT_NO_SKILLS), NULL, 340, 28, cur == 0, false);
    quit_button(g, 490, 380, 300, cur == (l.count ? l.count : 1));
    const SkillDef *s = cur < l.count ? skill_def(T(g), l.ids[cur]) : NULL;
    panel(g, 328, 436, 624, 66);
    if (s) {
        char buf[256];
        snprintf(buf, sizeof buf, "MP %d / %s", s->mpCost, s->desc);
        text_fit(g, 640, 452, 24, 580, buf, kWhite);
    }
}

static void stat_rows(SG *g, const Member *mb, const Member *after, int x, int y, int valueRight) {
    Tables *t = T(g);
    int vals[6] = {member_attack(t, mb), member_element_attack(t, mb), member_defense(t, mb), member_speed(t, mb), member_hit(t, mb),
                   member_evasion(t, mb)};
    int next[6] = {0};
    if (after) {
        next[0] = member_attack(t, after);
        next[1] = member_element_attack(t, after);
        next[2] = member_defense(t, after);
        next[3] = member_speed(t, after);
        next[4] = member_hit(t, after);
        next[5] = member_evasion(t, after);
    }
    const char *labels[6] = {MT(g, MT_ATTACK), t->text.config[CFG_ELEMENT_ATTACK], MT(g, MT_DEFENSE), MT(g, MT_SPEED), MT(g, MT_HIT),
                             MT(g, MT_EVASION)};
    for (int i = 0; i < 6; i++) {
        int yy = y + i * 24;
        text(g, x, yy + 2, x > 300 ? 18 : 16, labels[i], kWhite);
        textf_right(g, valueRight, yy, 21, kWhite, i >= 4 ? "%d%%" : "%d", vals[i]);
        if (after && next[i] != vals[i]) {
            text(g, valueRight + 10, yy, 21, "\xe2\x86\x92", kWhite);
            textf_right(g, valueRight + 105, yy, 21, next[i] < vals[i] ? kDown : kUp, i >= 4 ? "%d%%" : "%d", next[i]);
        }
    }
}

static void equip_candidates(SG *g, Menu *m, List *l) {
    Member *mb = &P(g)->members[m->member];
    int slot = m->cur[P_EQUIP];
    List all;
    item_list(g, &all, slot_kind(slot));
    l->count = 0;
    l->ids[l->count] = (Guid){{0}};
    l->counts[l->count++] = 0;
    for (int i = 0; i < all.count; i++) {
        if (!member_can_equip(T(g), mb, item_def(T(g), all.ids[i]))) continue;
        l->ids[l->count] = all.ids[i];
        l->counts[l->count++] = all.counts[i];
    }
}

static void item_line(SG *g, const ItemDef *it, char *buf, size_t size) {
    if (!it) {
        buf[0] = 0;
        return;
    }
    if (it->kind == ITEM_WEAPON) snprintf(buf, size, "%s %d / %s", MT(g, MT_ATTACK), it->attack, it->desc);
    else if (it->kind >= ITEM_SHIELD && it->kind <= ITEM_ACCESSORY && it->eq[EQ_DEFENSE])
        snprintf(buf, size, "%s %d / %s", MT(g, MT_DEFENSE), it->eq[EQ_DEFENSE], it->desc);
    else snprintf(buf, size, "%s", it->desc);
}

static void draw_equip(SG *g, Menu *m, bool picking) {
    Member *mb = &P(g)->members[m->member];
    member_side(g, m, mb);
    panel(g, 328, 16, 624, 422);
    if (!picking) text(g, 338, 28, 22, MT(g, MT_EQUIP), kWhite);
    static const int iconCol[SLOT_COUNT] = {4, 3, 2, 5, 0, 0};
    int cur = m->cur[P_EQUIP];
    for (int i = 0; i < SLOT_COUNT && !picking; i++) {
        int y = 63 + 48 * i;
        button(g, 337, y + 4, 36, 36);
        if (m->equipIcons) blit(g->fb, g->w, g->h, m->equipIcons, iconCol[i] * 32, 0, 32, 32, 339, y + 6, 32, 32, 255);
        button(g, 387, y, 553, 44);
        if (!picking && i == cur) highlight(g, 387, y, 553, 44);
        const ItemDef *it = item_def(T(g), mb->equip[i]);
        if (it) {
            icon(g, it->icon, it->iconX, it->iconY, 395, y + 6);
            text(g, 430, y + 10, 21, it->name, kWhite);
        } else {
            text(g, 391, y + 10, 21, MT(g, MT_NONE), kWhite);
        }
    }
    if (!picking) {
        button(g, 362, 380, 556, 48);
        if (cur == SLOT_COUNT) highlight(g, 362, 380, 556, 48);
        text_center(g, 640, 392, 20, T(g)->text.shop[SHOP_QUIT], kWhite);
        char buf[256];
        item_line(g, cur < SLOT_COUNT ? item_def(T(g), mb->equip[cur]) : NULL, buf, sizeof buf);
        bottom_text(g, buf);
        stat_rows(g, mb, NULL, 34, 294, 193);
        return;
    }
    List l;
    equip_candidates(g, m, &l);
    int pc = m->cur[P_EQUIP_PICK];
    for (int i = m->top[P_EQUIP_PICK] * 2; i < l.count && i < (m->top[P_EQUIP_PICK] + LIST_ROWS) * 2; i++) {
        const ItemDef *it = item_def(T(g), l.ids[i]);
        int row = i / 2 - m->top[P_EQUIP_PICK];
        char right[16];
        snprintf(right, sizeof right, "%d", l.counts[i]);
        if (it) draw_entry(g, it->icon, it->iconX, it->iconY, it->name, right, 340 + (i % 2) * 305, 28 + row * 48, i == pc, false);
        else draw_entry(g, (Guid){{0}}, 0, 0, MT(g, MT_REMOVE), NULL, 340, 28 + row * 48, i == pc, false);
    }
    quit_button(g, 490, 380, 300, pc == l.count);
    const ItemDef *old = item_def(T(g), mb->equip[cur]);
    highlight(g, 20, 230, 288, 42);
    if (old) {
        icon(g, old->icon, old->iconX, old->iconY, 28, 235);
        text(g, 66, 240, 20, old->name, kWhite);
    } else {
        text(g, 28, 240, 20, MT(g, MT_NONE), kWhite);
    }
    Member after = *mb;
    if (pc < l.count) after.equip[cur] = l.ids[pc];
    stat_rows(g, mb, pc < l.count ? &after : NULL, 34, 294, 193);
    panel(g, 328, 436, 624, 66);
    if (pc < l.count && item_def(T(g), l.ids[pc])) {
        char buf[256];
        item_line(g, item_def(T(g), l.ids[pc]), buf, sizeof buf);
        text_fit(g, 640, 452, 24, 580, buf, kWhite);
    }
}

static void draw_status(SG *g, Menu *m) {
    Member *mb = &P(g)->members[m->member];
    const HeroDef *h = hero_def(T(g), mb->hero);
    Image *face = h ? face_image(g, m, h->face) : NULL;
    if (face) blit(g->fb, g->w, g->h, face, 0, 0, face->w, face->h, 14, 10, face->w, face->h, 255);
    panel(g, 328, 16, 624, 486);
    Tables *t = T(g);
    if (!h) return;
    text(g, 354, 30, 21, h->name, kWhite);
    text(g, 355, 66, 16, MT(g, MT_LV), kWhite);
    textf_right(g, 597, 63, 21, kWhite, "%d", mb->level);
    char buf[64];
    text(g, 355, 90, 16, MT(g, MT_HP), kWhite);
    snprintf(buf, sizeof buf, "%d/%d", mb->hp, member_max_hp(t, mb));
    text_right(g, 597, 87, 21, buf, kWhite);
    text(g, 355, 114, 16, MT(g, MT_MP), kWhite);
    snprintf(buf, sizeof buf, "%d/%d", mb->mp, member_max_mp(t, mb));
    text_right(g, 597, 111, 21, buf, kWhite);
    stat_rows(g, mb, NULL, 355, 143, 597);
    for (int i = 0; i < SLOT_COUNT; i++) {
        const ItemDef *it = item_def(t, mb->equip[i]);
        int y = 68 + 32 * i;
        if (it) {
            icon(g, it->icon, it->iconX, it->iconY, 622, y - 2);
            text(g, 659, y, 21, it->name, kWhite);
        } else {
            text(g, 659, y, 21, MT(g, MT_NONE), kWhite);
        }
    }
    text(g, 615, 290, 16, MT(g, MT_EXP), kWhite);
    textf_right(g, 857, 287, 21, kWhite, "%d", mb->exp);
    text(g, 615, 314, 16, MT(g, MT_NEXT), kWhite);
    textf_right(g, 857, 311, 21, kWhite, "%d", exp_for_level(mb->level + 1) - mb->exp);
    fill_rect(g->fb, g->w, g->h, 355, 338, 565, 74, 0x60000000u);
    if (h->desc) text(g, 370, 343, 20, h->desc, kWhite);
    button(g, 490, 445, 300, 46);
    highlight(g, 490, 445, 300, 46);
    text_center(g, 640, 457, 20, MT(g, MT_CLOSE), kWhite);
}

static void draw_save(SG *g, Menu *m) {
    panel(g, 8, 17, 312, 486);
    panel(g, 328, 17, 624, 486);
    int cur = m->cur[P_SAVE];
    char name[64], date[64];
    for (int i = 0; i < SLOTS_PER_PAGE; i++) {
        int slot = m->savePage * SLOTS_PER_PAGE + i, y = 28 + 48 * i;
        button(g, 18, y, 290, 46);
        if (i == cur) highlight(g, 18, y, 290, 46);
        snprintf(name, sizeof name, "%s%d", T(g)->text.saveName ? T(g)->text.saveName : "", slot + 1);
        text(g, 24, y + 8, 22, name, kWhite);
        if (slot_info(m, slot, date, sizeof date)) {
            char d[16], tm[16];
            snprintf(d, sizeof d, "%.10s", date);
            snprintf(tm, sizeof tm, "%.8s", date + 11);
            text_right(g, 296, y + 2, 16, d, kWhite);
            text_right(g, 296, y + 21, 16, tm, kWhite);
        }
    }
    if (m->pageIcons)
        for (int i = 0; i < SAVE_PAGES; i++)
            blit(g->fb, g->w, g->h, m->pageIcons, i == m->savePage ? 0 : 16, 0, 16, 16, 126 + i * 16, 430, 16, 16, 255);
    quit_button(g, 18, 446, 290, cur == SLOTS_PER_PAGE);
    const SaveSlot *sel = cur < SLOTS_PER_PAGE ? slot_info(m, m->savePage * SLOTS_PER_PAGE + cur, date, sizeof date) : NULL;
    if (!sel) return;
    text(g, 355, 46, 21, date, kWhite);
    char *where = map_name(g, sel->map);
    text(g, 355, 82, 21, where, kWhite);
    free(where);
    for (int i = 0; i < sel->partyCount; i++) {
        const SlotHero *h = &sel->party[i];
        Member mb = {.hero = h->id, .level = h->level, .hp = h->hp, .mp = h->mp, .maxHp = h->maxHp, .maxMp = h->maxMp};
        member_box(g, m, &mb, 341 + (i % 2) * 300, 108 + (i / 2) * 190, -7);
    }
}

static void dialog(SG *g, const char *question, int sel) {
    panel(g, 160, 192, 640, 160);
    text_center(g, 480, 220, 22, question, kWhite);
    for (int i = 0; i < 2; i++) {
        int x = 170 + i * 311;
        button(g, x, 294, 308, 48);
        if (i == sel) highlight(g, x, 294, 308, 48);
        text(g, x + 10, 302, 21, T(g)->text.yesNo[i], kWhite);
    }
}

static void draw_config(SG *g, Menu *m) {
    panel(g, 16, 17, 936, 486);
    const GameText *t = &T(g)->text;
    const char *labels[5] = {t->config[CFG_BGM], t->config[CFG_SE], t->config[CFG_MSG_SPEED], t->config[CFG_CURSOR], t->config[CFG_RESET]};
    int cur = m->cur[P_CONFIG];
    for (int i = 0; i < 5; i++) {
        int y = 28 + 66 * i;
        button(g, 27, y, 318, 48);
        if (i == cur) highlight(g, 27, y, 318, 48);
        text(g, 31, y + 9, 21, labels[i], kWhite);
        if (i < 4) fill_rect(g->fb, g->w, g->h, 27, y + 56, 914, 1, 0x40ffffffu);
        if (i == cur && i < 4 && m->arrowSelected) {
            blit(g->fb, g->w, g->h, m->arrow, 0, 0, 64, 64, 346, y - 8, 64, 64, 255);
        }
        if (i < 2) {
            int v = i == 0 ? m->bgm : m->se;
            textf_right(g, 662, y + 2, 17, kWhite, "%d", v);
            fill_rect(g->fb, g->w, g->h, 430, y + 24, 428, 18, 0xffc0c0c0u);
            fill_rect(g->fb, g->w, g->h, 432, y + 26, 424, 14, 0xff000000u);
            fill_rect(g->fb, g->w, g->h, 432, y + 26, 424 * v / 100, 14, 0xff8a1e1eu);
        } else if (i == 2) {
            text_center(g, 645, y + 7, 23, t->config[CFG_INSTANT + m->speed], kWhite);
        } else if (i == 3) {
            text_center(g, 645, y + 7, 23, t->config[m->remember ? CFG_REMEMBER : CFG_FORGET], kWhite);
        }
    }
    quit_button(g, 27, 446, 318, cur == CONFIG_QUIT);
}

enum { SHOP_ROWS = 6 };

static void money_box(SG *g) {
    panel(g, 728, 3, 218, 48);
    char buf[32];
    snprintf(buf, sizeof buf, "%d G", P(g)->money);
    text_right(g, 936, 12, 22, buf, kWhite);
}

static int shop_slot(const ItemDef *it) {
    switch (it ? it->kind : 0) {
    case ITEM_WEAPON: return SLOT_WEAPON;
    case ITEM_SHIELD: return SLOT_SHIELD;
    case ITEM_HEAD: return SLOT_HEAD;
    case ITEM_BODY: return SLOT_BODY;
    case ITEM_ACCESSORY: return SLOT_ACC1;
    default: return -1;
    }
}

static const char *kind_label(SG *g, const ItemDef *it) {
    switch (it->kind) {
    case ITEM_WEAPON: return MT(g, MT_WEAPON);
    case ITEM_SHIELD: return MT(g, MT_SHIELD);
    case ITEM_HEAD: return MT(g, MT_HEAD);
    case ITEM_BODY: return MT(g, MT_BODY);
    case ITEM_ACCESSORY: return MT(g, MT_ACCESSORY);
    default: return MT(g, MT_EXPENDABLE);
    }
}

static void compare_line(SG *g, int x, int y, const char *label, int before, int after) {
    text(g, x, y, 17, label, kWhite);
    char buf[48];
    snprintf(buf, sizeof buf, "%d \xe2\x86\x92 ", before);
    text(g, x, y + 22, 17, buf, kWhite);
    char v[16];
    snprintf(v, sizeof v, "%d", after);
    text(g, x + font_width(g->font, 17, buf), y + 22, 17, v, after < before ? kDown : kUp);
}

static void shop_detail(SG *g, Menu *m, const ItemDef *it) {
    panel(g, 414, 73, 532, 397);
    if (!it) return;
    text(g, 423, 88, 22, kind_label(g, it), kWhite);
    char buf[64];
    snprintf(buf, sizeof buf, "%s : %d", MT(g, MT_OWNED), party_item_count(P(g), it->id));
    text(g, 690, 88, 22, buf, kWhite);
    if (it->kind == ITEM_WEAPON) snprintf(buf, sizeof buf, "%s : %d", MT(g, MT_ATTACK), it->attack);
    else if (it->kind >= ITEM_SHIELD && it->kind <= ITEM_ACCESSORY) snprintf(buf, sizeof buf, "%s : %d", MT(g, MT_DEFENSE), it->eq[EQ_DEFENSE]);
    else buf[0] = 0;
    text(g, 423, 123, 22, buf, kWhite);
    int slot = shop_slot(it);
    if (slot < 0) return;
    Tables *t = T(g);
    Party *p = P(g);
    for (int i = 0, shown = 0; i < p->count && shown < 2; i++) {
        const Member *mb = &p->members[i];
        if (!member_can_equip(t, mb, it)) continue;
        const HeroDef *h = hero_def(t, mb->hero);
        int y = 193 + shown * 140;
        Image *face = h ? face_image(g, m, h->face) : NULL;
        if (face && face->w >= 201 && face->h >= 253) blit(g->fb, g->w, g->h, face, 76, 118, 125, 135, 423, y, 125, 135, 255);
        if (h) text_center(g, 645, y + 7, 17, h->name, kWhite);
        Member after = *mb;
        after.equip[slot] = it->id;
        if (it->kind == ITEM_WEAPON) {
            compare_line(g, 552, y + 38, MT(g, MT_ATTACK), member_attack(t, mb), member_attack(t, &after));
            compare_line(g, 552, y + 82, t->text.config[CFG_ELEMENT_ATTACK], member_element_attack(t, mb), member_element_attack(t, &after));
        } else {
            compare_line(g, 552, y + 38, MT(g, MT_DEFENSE), member_defense(t, mb), member_defense(t, &after));
        }
        shown++;
    }
}

static void draw_shop(SG *g, Menu *m) {
    money_box(g);
    if (m->page == P_SHOP) {
        interp_draw_last_window(g);
        panel(g, 428, 193, 103, 158);
        int labels[3] = {SHOP_BUY, SHOP_SELL, SHOP_QUIT};
        for (int i = 0; i < 3; i++) {
            int y = 205 + 46 * i;
            button(g, 437, y, 86, 44);
            if (i == m->cur[P_SHOP]) highlight(g, 437, y, 86, 44);
            text(g, 446, y + 8, 22, T(g)->text.shop[labels[i]], kWhite);
        }
        return;
    }
    List l;
    bool sell = m->page == P_SHOP_SELL;
    if (sell) item_list(g, &l, 0);
    else shop_list(g, m, &l);
    int page = m->page, cur = m->cur[page];
    panel(g, 13, 73, 397, 397);
    for (int i = m->top[page]; i < l.count && i < m->top[page] + SHOP_ROWS; i++) {
        const ItemDef *it = item_def(T(g), l.ids[i]);
        if (!it) continue;
        int y = 84 + (i - m->top[page]) * 50;
        button(g, 23, y, 377, 48);
        if (i == cur) highlight(g, 23, y, 377, 48);
        icon(g, it->icon, it->iconX, it->iconY, 34, y + 8);
        u32 c = !sell && P(g)->money < m->shopPrices[i] ? kGray : kWhite;
        text(g, 68, y + 11, 21, it->name, c);
        char right[32];
        snprintf(right, sizeof right, "%d", sell ? it->price / 2 : m->shopPrices[i]);
        text_right(g, 393, y + 14, 17, right, c);
    }
    quit_button(g, 23, 414, 377, cur == l.count);
    const ItemDef *it = cur < l.count ? item_def(T(g), l.ids[cur]) : NULL;
    shop_detail(g, m, it);
    panel(g, 13, 473, 933, 62);
    text_fit(g, 480, 490, 22, 900, m->note[0] ? m->note : it ? it->desc : "", kWhite);
    if (m->qty && it) {
        panel(g, 240, 206, 480, 132);
        icon(g, it->icon, it->iconX, it->iconY, 265, 233);
        text(g, 302, 229, 24, it->name, kWhite);
        char q[16], price[32];
        snprintf(q, sizeof q, "x%d", m->qty);
        highlight(g, 615, 224, 77, 47);
        text_right(g, 678, 231, 24, q, kWhite);
        snprintf(price, sizeof price, "%dG", (sell ? it->price / 2 : m->shopPrices[cur]) * m->qty);
        text_right(g, 678, 277, 24, price, kWhite);
    }
}

static void draw_inn(SG *g, Menu *m) {
    char q[128];
    snprintf(q, sizeof q, "%dG", m->innPrice);
    panel(g, 160, 192, 640, 160);
    text_center(g, 480, 222, 21, m->note[0] ? m->note : q, kWhite);
    for (int i = 0; i < 2; i++) {
        int x = 170 + i * 311;
        button(g, x, 294, 308, 48);
        if (i == m->cur[P_INN]) highlight(g, x, 294, 308, 48);
        text(g, x + 10, 306, 20, T(g)->text.shop[i == 0 ? SHOP_STAY : SHOP_LEAVE], kWhite);
    }
    char money[64];
    snprintf(money, sizeof money, "%s : %dG", T(g)->text.money ? T(g)->text.money : "", P(g)->money);
    panel(g, 700, 120, 252, 50);
    text_right(g, 936, 133, 17, money, kWhite);
}

static void draw(SG *g, Menu *m) {
    switch (m->page) {
    case P_MAIN:
    case P_QUIT:
        main_column(g, m, m->cur[P_MAIN]);
        panel(g, 328, 16, 624, 422);
        member_grid(g, m, -1);
        bottom_info(g);
        if (m->page == P_QUIT) dialog(g, MT(g, MT_QUIT), m->cur[P_QUIT]);
        guide(g, m, false);
        break;
    case P_ITEMS:
    case P_ITEM_TARGET: {
        main_column(g, m, m->cur[P_MAIN]);
        List l;
        item_list(g, &l, 0);
        if (m->page == P_ITEM_TARGET) {
            panel(g, 328, 16, 624, 422);
            member_grid(g, m, m->cur[P_ITEM_TARGET]);
            bottom_text(g, MT(g, MT_WHO_USES));
        } else {
            draw_item_page(g, m, P_ITEMS, &l, false, false);
            int cur = m->cur[P_ITEMS];
            char buf[256] = "";
            const ItemDef *it = cur < l.count ? item_def(T(g), l.ids[cur]) : NULL;
            if (it) snprintf(buf, sizeof buf, "%s", it->desc);
            bottom_text(g, buf);
        }
        guide(g, m, false);
        break;
    }
    case P_SKILL_WHO:
    case P_EQUIP_WHO:
    case P_STATUS_WHO: {
        main_column(g, m, m->cur[P_MAIN]);
        panel(g, 328, 16, 624, 486);
        member_grid(g, m, m->cur[m->page]);
        quit_button(g, 490, 444, 300, m->cur[m->page] == P(g)->count);
        guide(g, m, false);
        break;
    }
    case P_SKILLS:
        draw_skills(g, m);
        guide(g, m, false);
        break;
    case P_SKILL_TARGET:
        member_side(g, m, &P(g)->members[m->member]);
        panel(g, 328, 16, 624, 422);
        member_grid(g, m, m->cur[P_SKILL_TARGET]);
        bottom_text(g, MT(g, MT_WHO_USES));
        guide(g, m, false);
        break;
    case P_EQUIP:
    case P_EQUIP_PICK:
        draw_equip(g, m, m->page == P_EQUIP_PICK);
        guide(g, m, false);
        break;
    case P_STATUS:
        draw_status(g, m);
        guide(g, m, true);
        break;
    case P_SAVE:
    case P_SAVE_CONFIRM:
        draw_save(g, m);
        if (m->page == P_SAVE_CONFIRM) dialog(g, MT(g, MT_OVERWRITE), m->cur[P_SAVE_CONFIRM]);
        else if (m->note[0]) bottom_text(g, m->note);
        guide(g, m, false);
        break;
    case P_CONFIG:
        draw_config(g, m);
        guide(g, m, false);
        break;
    case P_SHOP:
    case P_SHOP_BUY:
    case P_SHOP_SELL:
        draw_shop(g, m);
        break;
    case P_INN:
        draw_inn(g, m);
        guide(g, m, false);
        break;
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void close_menu(SG *g, Menu *m) {
    m->closing = true;
    m->animMs = 0;
    if (!m->remember) memset(m->cur, 0, sizeof m->cur);
}

static void go(Menu *m, int page) {
    m->page = page;
    m->note[0] = 0;
}

static bool move_grid(SG *g, Menu *m, int page, int count, bool withQuit) {
    int last = withQuit ? count : count - 1;
    int c = m->cur[page], old = c;
    if (sg_pressed(g, KEY_DOWN)) c = c >= count ? c : (c + 2 <= count - 1 ? c + 2 : (withQuit ? count : c));
    if (sg_pressed(g, KEY_UP)) c = c == count ? (count > 0 ? count - 1 : 0) : (c >= 2 ? c - 2 : c);
    if (sg_pressed(g, KEY_RIGHT) && c < count - 1 && c % 2 == 0) c++;
    if (sg_pressed(g, KEY_LEFT) && c < count && c % 2 == 1) c--;
    c = clampi(c, 0, last < 0 ? 0 : last);
    m->cur[page] = c;
    if (c < count) scroll_to(m, page, c);
    if (c != old) sgb_sound(g, SND_CURSOR);
    return c != old;
}

static bool move_column(SG *g, Menu *m, int page, int count) {
    int c = m->cur[page], old = c;
    if (sg_pressed(g, KEY_DOWN)) c = (c + 1) % count;
    if (sg_pressed(g, KEY_UP)) c = (c + count - 1) % count;
    m->cur[page] = c;
    if (c != old) sgb_sound(g, SND_CURSOR);
    return c != old;
}

static void heal(SG *g, Member *mb, int hp, int mp, int hpPct, int mpPct) {
    Tables *t = T(g);
    int maxHp = member_max_hp(t, mb), maxMp = member_max_mp(t, mb);
    mb->hp = clampi(mb->hp + hp + maxHp * hpPct / 100, 0, maxHp);
    mb->mp = clampi(mb->mp + mp + maxMp * mpPct / 100, 0, maxMp);
}

static void use_item(SG *g, Menu *m, const ItemDef *it, Member *mb) {
    if (mb->hp <= 0 && !it->revive) return;
    if (it->revive && mb->hp <= 0) mb->hp = 1;
    heal(g, mb, it->hp, it->mp, it->hpPercent, it->mpPercent);
    if (it->cure[0] && mb->state == 1) mb->state = 0;
    party_give_item(P(g), it->id, -1);
    sgb_sound(g, SND_ITEM);
    sg_log(g, "menu: used %s on %s", it->name, hero_def(T(g), mb->hero) ? hero_def(T(g), mb->hero)->name : "?");
}

static void use_skill(SG *g, Menu *m, const SkillDef *s, Member *user, int target) {
    Party *p = P(g);
    if (user->mp < s->mpCost) return;
    user->mp -= s->mpCost;
    for (int i = 0; i < p->count; i++) {
        if (s->target == TARGET_ALLY && i != target) continue;
        Member *mb = &p->members[i];
        if (mb->hp <= 0) continue;
        heal(g, mb, s->power, 0, 0, 0);
    }
    sgb_sound(g, SND_ITEM);
    sg_log(g, "menu: skill %s", s->name);
}

static void update_main(SG *g, Menu *m) {
    move_column(g, m, P_MAIN, MAIN_COUNT);
    if (sg_pressed(g, KEY_CANCEL)) {
        sgb_sound(g, SND_CANCEL);
        close_menu(g, m);
        return;
    }
    if (!sg_pressed(g, KEY_OK)) return;
    sgb_sound(g, SND_DECIDE);
    switch (m->cur[P_MAIN]) {
    case MAIN_ITEMS: go(m, P_ITEMS); break;
    case MAIN_SKILLS: go(m, P_SKILL_WHO); break;
    case MAIN_EQUIP: go(m, P_EQUIP_WHO); break;
    case MAIN_STATUS: go(m, P_STATUS_WHO); break;
    case MAIN_SAVE:
        sg_save_slots(g, m->slots, SAVE_SLOTS);
        go(m, P_SAVE);
        break;
    case MAIN_QUIT:
        m->cur[P_QUIT] = 1;
        go(m, P_QUIT);
        break;
    case MAIN_CONFIG: go(m, P_CONFIG); break;
    case MAIN_CLOSE: close_menu(g, m); break;
    }
}

static bool back(SG *g, Menu *m, int page) {
    if (!sg_pressed(g, KEY_CANCEL)) return false;
    sgb_sound(g, SND_CANCEL);
    go(m, page);
    return true;
}

static void update_items(SG *g, Menu *m) {
    List l;
    item_list(g, &l, 0);
    int n = l.count ? l.count : 1;
    move_grid(g, m, P_ITEMS, n, true);
    if (back(g, m, P_MAIN)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    int c = m->cur[P_ITEMS];
    if (c >= n) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_MAIN);
        return;
    }
    const ItemDef *it = c < l.count ? item_def(T(g), l.ids[c]) : NULL;
    if (!item_field_usable(it) || it->kind != ITEM_EXPENDABLE) {
        sgb_sound(g, SND_CANCEL);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    go(m, P_ITEM_TARGET);
}

static void update_item_target(SG *g, Menu *m) {
    Party *p = P(g);
    move_grid(g, m, P_ITEM_TARGET, p->count, false);
    if (back(g, m, P_ITEMS)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    List l;
    item_list(g, &l, 0);
    int c = m->cur[P_ITEMS];
    const ItemDef *it = c < l.count ? item_def(T(g), l.ids[c]) : NULL;
    if (!it || m->cur[P_ITEM_TARGET] >= p->count) return;
    use_item(g, m, it, &p->members[m->cur[P_ITEM_TARGET]]);
    if (party_item_count(p, it->id) <= 0) {
        go(m, P_ITEMS);
        item_list(g, &l, 0);
        m->cur[P_ITEMS] = clampi(m->cur[P_ITEMS], 0, l.count);
    }
}

static void update_who(SG *g, Menu *m, int page, int next) {
    Party *p = P(g);
    int c = m->cur[page], old = c;
    if (sg_pressed(g, KEY_RIGHT) && c < p->count - 1 && c % 2 == 0) c++;
    if (sg_pressed(g, KEY_LEFT) && c < p->count && c % 2 == 1) c--;
    if (sg_pressed(g, KEY_DOWN)) c = c + 2 < p->count ? c + 2 : p->count;
    if (sg_pressed(g, KEY_UP)) c = c == p->count ? (p->count > 2 ? 2 : 0) : (c >= 2 ? c - 2 : c);
    m->cur[page] = clampi(c, 0, p->count);
    if (m->cur[page] != old) sgb_sound(g, SND_CURSOR);
    if (back(g, m, P_MAIN)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (m->cur[page] >= p->count) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_MAIN);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    m->member = m->cur[page];
    m->cur[next] = 0;
    m->top[next] = 0;
    go(m, next);
}

static void update_skills(SG *g, Menu *m) {
    Member *mb = &P(g)->members[m->member];
    List l;
    skill_list(g, mb, &l);
    int n = l.count ? l.count : 1;
    move_grid(g, m, P_SKILLS, n, true);
    if (back(g, m, P_SKILL_WHO)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    int c = m->cur[P_SKILLS];
    if (c >= n) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_SKILL_WHO);
        return;
    }
    const SkillDef *s = c < l.count ? skill_def(T(g), l.ids[c]) : NULL;
    if (!s || !s->fieldUse || mb->mp < s->mpCost || mb->hp <= 0) {
        sgb_sound(g, SND_CANCEL);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    if (s->target == TARGET_ALLIES) use_skill(g, m, s, mb, -1);
    else {
        m->cur[P_SKILL_TARGET] = 0;
        go(m, P_SKILL_TARGET);
    }
}

static void update_skill_target(SG *g, Menu *m) {
    Party *p = P(g);
    move_grid(g, m, P_SKILL_TARGET, p->count, false);
    if (back(g, m, P_SKILLS)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    Member *mb = &p->members[m->member];
    List l;
    skill_list(g, mb, &l);
    int c = m->cur[P_SKILLS];
    const SkillDef *s = c < l.count ? skill_def(T(g), l.ids[c]) : NULL;
    if (s) use_skill(g, m, s, mb, m->cur[P_SKILL_TARGET]);
    if (!s || mb->mp < s->mpCost) go(m, P_SKILLS);
}

static void update_equip(SG *g, Menu *m) {
    int c = m->cur[P_EQUIP], old = c;
    if (sg_pressed(g, KEY_DOWN)) c = (c + 1) % (SLOT_COUNT + 1);
    if (sg_pressed(g, KEY_UP)) c = (c + SLOT_COUNT) % (SLOT_COUNT + 1);
    m->cur[P_EQUIP] = c;
    if (c != old) sgb_sound(g, SND_CURSOR);
    if (back(g, m, P_EQUIP_WHO)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (c == SLOT_COUNT) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_EQUIP_WHO);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    m->cur[P_EQUIP_PICK] = 0;
    m->top[P_EQUIP_PICK] = 0;
    go(m, P_EQUIP_PICK);
}

static void update_equip_pick(SG *g, Menu *m) {
    List l;
    equip_candidates(g, m, &l);
    move_grid(g, m, P_EQUIP_PICK, l.count, true);
    if (back(g, m, P_EQUIP)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    int c = m->cur[P_EQUIP_PICK];
    if (c >= l.count) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_EQUIP);
        return;
    }
    Party *p = P(g);
    Member *mb = &p->members[m->member];
    int slot = m->cur[P_EQUIP];
    Guid old = mb->equip[slot];
    if (!guid_zero(old)) party_give_item(p, old, 1);
    mb->equip[slot] = l.ids[c];
    if (!guid_zero(l.ids[c])) party_give_item(p, l.ids[c], -1);
    int maxHp = member_max_hp(T(g), mb), maxMp = member_max_mp(T(g), mb);
    if (mb->hp > maxHp) mb->hp = maxHp;
    if (mb->mp > maxMp) mb->mp = maxMp;
    sgb_sound(g, SND_DECIDE);
    go(m, P_EQUIP);
}

static void update_status(SG *g, Menu *m) {
    Party *p = P(g);
    int old = m->member;
    if (sg_pressed(g, KEY_RIGHT)) m->member = (m->member + 1) % p->count;
    if (sg_pressed(g, KEY_LEFT)) m->member = (m->member + p->count - 1) % p->count;
    if (m->member != old) sgb_sound(g, SND_CURSOR);
    if (back(g, m, P_STATUS_WHO)) return;
    if (sg_pressed(g, KEY_OK)) {
        sgb_sound(g, SND_DECIDE);
        go(m, P_STATUS_WHO);
    }
}

static int save_cursor(SG *g, Menu *m) {
    int c = m->cur[P_SAVE], old = c, oldPage = m->savePage;
    if (sg_pressed(g, KEY_DOWN)) c = (c + 1) % (SLOTS_PER_PAGE + 1);
    if (sg_pressed(g, KEY_UP)) c = (c + SLOTS_PER_PAGE) % (SLOTS_PER_PAGE + 1);
    if (sg_pressed(g, KEY_RIGHT)) m->savePage = (m->savePage + 1) % SAVE_PAGES;
    if (sg_pressed(g, KEY_LEFT)) m->savePage = (m->savePage + SAVE_PAGES - 1) % SAVE_PAGES;
    m->cur[P_SAVE] = c;
    if (c != old || m->savePage != oldPage) {
        sgb_sound(g, SND_CURSOR);
        m->note[0] = 0;
    }
    return c;
}

static void update_load(SG *g, Menu *m) {
    int c = save_cursor(g, m);
    if (back(g, m, P_MAIN)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (c == SLOTS_PER_PAGE) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_MAIN);
        return;
    }
    char date[64];
    int slot = m->savePage * SLOTS_PER_PAGE + c;
    if (!slot_info(m, slot, date, sizeof date)) {
        sgb_sound(g, SND_CANCEL);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    if (!sg_save_read(g, slot)) snprintf(m->note, sizeof m->note, "%s", MT(g, MT_NONE));
}

static void update_save(SG *g, Menu *m) {
    if (m->mode == MENU_TITLE) {
        update_load(g, m);
        return;
    }
    int c = save_cursor(g, m);
    if (back(g, m, P_MAIN)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (c == SLOTS_PER_PAGE) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_MAIN);
        return;
    }
    char date[64];
    sgb_sound(g, SND_DECIDE);
    if (slot_info(m, m->savePage * SLOTS_PER_PAGE + c, date, sizeof date)) {
        m->cur[P_SAVE_CONFIRM] = 0;
        go(m, P_SAVE_CONFIRM);
    } else if (save_slot(g, m, m->savePage * SLOTS_PER_PAGE + c)) {
        snprintf(m->note, sizeof m->note, "%s", MT(g, MT_SAVED));
    }
}

static void update_confirm(SG *g, Menu *m, int page, int backTo) {
    int c = m->cur[page], old = c;
    if (sg_pressed(g, KEY_LEFT) || sg_pressed(g, KEY_RIGHT)) c = 1 - c;
    m->cur[page] = c;
    if (c != old) sgb_sound(g, SND_CURSOR);
    if (back(g, m, backTo)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (c == 1) {
        sgb_sound(g, SND_CANCEL);
        go(m, backTo);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    if (page == P_QUIT) {
        m->quit = true;
        sg_log(g, "menu: quit game");
        return;
    }
    go(m, backTo);
    if (save_slot(g, m, m->savePage * SLOTS_PER_PAGE + m->cur[P_SAVE])) snprintf(m->note, sizeof m->note, "%s", MT(g, MT_SAVED));
}

static void update_config(SG *g, Menu *m) {
    move_column(g, m, P_CONFIG, CONFIG_ROWS);
    int c = m->cur[P_CONFIG];
    int d = sg_pressed(g, KEY_RIGHT) ? 1 : sg_pressed(g, KEY_LEFT) ? -1 : 0;
    if (d) {
        if (c == CONFIG_BGM) m->bgm = clampi(m->bgm + d * 10, 0, 100);
        else if (c == CONFIG_SE) m->se = clampi(m->se + d * 10, 0, 100);
        else if (c == CONFIG_SPEED) m->speed = clampi(m->speed + d, 0, 3);
        else if (c == CONFIG_CURSOR) m->remember = !m->remember;
        if (c < CONFIG_RESET) sgb_sound(g, SND_CURSOR);
    }
    if (back(g, m, P_MAIN)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (c == CONFIG_RESET) {
        m->bgm = m->se = 100;
        m->speed = 1;
        m->remember = 1;
        sgb_sound(g, SND_DECIDE);
    } else if (c == CONFIG_QUIT) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_MAIN);
    }
}

static void shop_scroll(Menu *m, int page) {
    int c = m->cur[page];
    if (c < m->top[page]) m->top[page] = c;
    if (c >= m->top[page] + SHOP_ROWS) m->top[page] = c - SHOP_ROWS + 1;
}

static void update_shop(SG *g, Menu *m) {
    Party *p = P(g);
    List l;
    int page = m->page;
    bool sell = page == P_SHOP_SELL;
    if (sell) item_list(g, &l, 0);
    else shop_list(g, m, &l);
    int c = m->cur[page];
    const ItemDef *it = c < l.count ? item_def(T(g), l.ids[c]) : NULL;
    if (m->qty) {
        int price = it ? (sell ? it->price / 2 : m->shopPrices[c]) : 0;
        int most = sell ? l.counts[c] : (price > 0 ? p->money / price : 99);
        int have = it ? party_item_count(p, it->id) : 0;
        if (!sell && most > 99 - have) most = 99 - have;
        if (most < 1) most = 1;
        int q = m->qty;
        if (sg_pressed(g, KEY_RIGHT) || sg_pressed(g, KEY_UP)) q = q < most ? q + 1 : q;
        if (sg_pressed(g, KEY_LEFT) || sg_pressed(g, KEY_DOWN)) q = q > 1 ? q - 1 : q;
        if (q != m->qty) sgb_sound(g, SND_CURSOR);
        m->qty = q;
        if (sg_pressed(g, KEY_CANCEL)) {
            sgb_sound(g, SND_CANCEL);
            m->qty = 0;
            return;
        }
        if (!sg_pressed(g, KEY_OK) || !it) return;
        m->qty = 0;
        if (!sell) {
            if (p->money < price * q || have + q > 99) {
                sgb_sound(g, SND_CANCEL);
                snprintf(m->note, sizeof m->note, "%s", T(g)->text.shop[SHOP_NO_MONEY]);
                return;
            }
            p->money -= price * q;
            party_give_item(p, it->id, q);
            m->result = true;
            sgb_sound(g, SND_DECIDE);
            const char *f = T(g)->text.shop[SHOP_BOUGHT];
            const char *at = f ? strstr(f, "{0}") : NULL;
            if (at) snprintf(m->note, sizeof m->note, "%.*s%s%s", (int)(at - f), f, it->name, at + 3);
        } else {
            p->money += price * q;
            party_give_item(p, it->id, -q);
            sgb_sound(g, SND_DECIDE);
            const char *f = T(g)->text.shop[SHOP_SOLD];
            const char *at = f ? strstr(f, "{0}") : NULL;
            if (at) snprintf(m->note, sizeof m->note, "%.*s%s%s", (int)(at - f), f, it->name, at + 3);
            item_list(g, &l, 0);
            m->cur[page] = clampi(m->cur[page], 0, l.count);
        }
        return;
    }
    if (move_column(g, m, page, l.count + 1)) m->note[0] = 0;
    shop_scroll(m, page);
    if (back(g, m, P_SHOP)) return;
    if (!sg_pressed(g, KEY_OK)) return;
    if (c >= l.count) {
        sgb_sound(g, SND_CANCEL);
        go(m, P_SHOP);
        return;
    }
    if (!it) return;
    if (sell && it->price <= 0) {
        sgb_sound(g, SND_CANCEL);
        return;
    }
    if (!sell && p->money < m->shopPrices[c]) {
        sgb_sound(g, SND_CANCEL);
        snprintf(m->note, sizeof m->note, "%s", T(g)->text.shop[SHOP_NO_MONEY]);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    m->note[0] = 0;
    m->qty = 1;
}

static void update_shop_main(SG *g, Menu *m) {
    if (m->page != P_SHOP) {
        update_shop(g, m);
        return;
    }
    move_column(g, m, P_SHOP, 3);
    if (sg_pressed(g, KEY_CANCEL)) {
        sgb_sound(g, SND_CANCEL);
        close_menu(g, m);
        return;
    }
    if (!sg_pressed(g, KEY_OK)) return;
    int c = m->cur[P_SHOP];
    if (c == 2) {
        sgb_sound(g, SND_CANCEL);
        close_menu(g, m);
        return;
    }
    sgb_sound(g, SND_DECIDE);
    int page = c == 0 ? P_SHOP_BUY : P_SHOP_SELL;
    m->cur[page] = 0;
    m->top[page] = 0;
    m->qty = 0;
    go(m, page);
}

static void update_inn(SG *g, Menu *m) {
    Party *p = P(g);
    int c = m->cur[P_INN], old = c;
    if (sg_pressed(g, KEY_LEFT) || sg_pressed(g, KEY_RIGHT)) c = 1 - c;
    m->cur[P_INN] = c;
    if (c != old) sgb_sound(g, SND_CURSOR);
    bool cancel = sg_pressed(g, KEY_CANCEL);
    bool ok = sg_pressed(g, KEY_OK);
    if (m->note[0] && (ok || cancel)) {
        close_menu(g, m);
        return;
    }
    if (cancel || (ok && c == 1)) {
        sgb_sound(g, SND_CANCEL);
        close_menu(g, m);
        return;
    }
    if (!ok) return;
    if (p->money < m->innPrice) {
        sgb_sound(g, SND_CANCEL);
        snprintf(m->note, sizeof m->note, "%s", T(g)->text.shop[SHOP_NO_MONEY]);
        return;
    }
    p->money -= m->innPrice;
    party_recover(g);
    m->result = true;
    sgb_sound(g, SND_STATUS_UP);
    sg_log(g, "menu: inn, party healed for %dG", m->innPrice);
    close_menu(g, m);
}

static void begin(SG *g, Menu *m, int mode, int page) {
    load_system(g, m);
    m->mode = mode;
    m->closing = false;
    m->animMs = 0;
    m->note[0] = 0;
    go(m, page);
    if (!m->remember) memset(m->cur, 0, sizeof m->cur);
    sg_log(g, "menu: open %s", menu_page(g));
}

void menu_open(SG *g) {
    Menu *m = menu_get(g);
    if (m) begin(g, m, MENU_MAIN, P_MAIN);
}

void title_menu_open(SG *g, bool load) {
    Menu *m = menu_get(g);
    if (!m) return;
    begin(g, m, MENU_TITLE, load ? P_SAVE : P_CONFIG);
    if (load) {
        sg_save_slots(g, m->slots, SAVE_SLOTS);
        m->savePage = 0;
        m->cur[P_SAVE] = 0;
    }
}

void shop_open(SG *g, const Guid *items, const i32 *prices, int count) {
    Menu *m = menu_get(g);
    if (!m) return;
    m->shopCount = 0;
    for (int i = 0; i < count && m->shopCount < 32; i++) {
        const ItemDef *it = item_def(T(g), items[i]);
        if (!it) continue;
        m->shopItems[m->shopCount] = items[i];
        m->shopPrices[m->shopCount++] = prices ? prices[i] : it->price;
    }
    m->cur[P_SHOP] = 0;
    m->result = false;
    begin(g, m, MENU_SHOP, P_SHOP);
}

void inn_open(SG *g, int price) {
    Menu *m = menu_get(g);
    if (!m) return;
    m->innPrice = price;
    m->cur[P_INN] = 0;
    m->result = false;
    begin(g, m, MENU_INN, P_INN);
}

void menu_close(SG *g) {
    Menu *m = g->b.menu;
    if (!m || m->mode == MENU_CLOSED) return;
    m->mode = MENU_CLOSED;
    m->closing = false;
    sg_log(g, "menu: closed for a load");
}

bool menu_active(SG *g) {
    SgB *b = sgb(g);
    return b && b->menu && b->menu->mode != MENU_CLOSED;
}

bool menu_result(SG *g) { return g->b.menu && g->b.menu->result; }

bool sgb_quit(SG *g) {
    SgB *b = sgb(g);
    return b && b->menu && b->menu->quit;
}

const char *menu_page(SG *g) {
    static const char *names[P_COUNT] = {"menu",        "items",  "item-target", "skill-who", "skills", "skill-target", "equip-who",
                                         "equip",       "equip-pick", "status-who", "status", "save",   "save-confirm", "quit",
                                         "config",      "shop",   "shop-buy",    "shop-sell", "inn"};
    SgB *b = sgb(g);
    if (!b || !b->menu || b->menu->mode == MENU_CLOSED) return NULL;
    return names[b->menu->page];
}

static void update(SG *g, Menu *m) {
    switch (m->page) {
    case P_MAIN: update_main(g, m); break;
    case P_ITEMS: update_items(g, m); break;
    case P_ITEM_TARGET: update_item_target(g, m); break;
    case P_SKILL_WHO: update_who(g, m, P_SKILL_WHO, P_SKILLS); break;
    case P_SKILLS: update_skills(g, m); break;
    case P_SKILL_TARGET: update_skill_target(g, m); break;
    case P_EQUIP_WHO: update_who(g, m, P_EQUIP_WHO, P_EQUIP); break;
    case P_EQUIP: update_equip(g, m); break;
    case P_EQUIP_PICK: update_equip_pick(g, m); break;
    case P_STATUS_WHO: update_who(g, m, P_STATUS_WHO, P_STATUS); break;
    case P_STATUS: update_status(g, m); break;
    case P_SAVE: update_save(g, m); break;
    case P_SAVE_CONFIRM: update_confirm(g, m, P_SAVE_CONFIRM, P_SAVE); break;
    case P_QUIT: update_confirm(g, m, P_QUIT, P_MAIN); break;
    case P_CONFIG: update_config(g, m); break;
    case P_SHOP:
    case P_SHOP_BUY:
    case P_SHOP_SELL: update_shop_main(g, m); break;
    case P_INN: update_inn(g, m); break;
    }
}

static void scaled_overlay(SG *g, u32 *under, float scale, int alpha) {
    int w = g->w, h = g->h;
    u32 *menu = sg_alloc(sizeof(u32) * (size_t)w * (size_t)h);
    memcpy(menu, g->fb, sizeof(u32) * (size_t)w * (size_t)h);
    memcpy(g->fb, under, sizeof(u32) * (size_t)w * (size_t)h);
    Image im = {w, h, menu};
    int dw = (int)((float)w * scale), dh = (int)((float)h * scale);
    for (int i = 0; i < w * h; i++) {
        u32 a = menu[i], b = under[i];
        menu[i] = a == b ? 0 : a | 0xff000000u;
    }
    blit(g->fb, w, h, &im, 0, 0, w, h, (w - dw) / 2, (h - dh) / 2, dw, dh, alpha);
    free(menu);
}

bool menu_frame(SG *g, u32 ms) {
    SgB *b = sgb(g);
    if (!b) return false;
    b->party.playMs += ms;
    Menu *m = menu_get(g);
    if (!m) return false;
    if (m->mode == MENU_CLOSED) {
        if (!g->interp.active && !g->pendingTransfer && g->fadeLevel <= 0 && sg_pressed(g, KEY_CANCEL)) {
            sgb_sound(g, SND_DECIDE);
            menu_open(g);
        } else {
            return false;
        }
    }
    m->animMs += ms;
    if (!m->closing && m->animMs >= (u32)kOpenMs) update(g, m);
    field_draw(g);
    if (m->mode == MENU_CLOSED) return true;
    u32 *under = NULL;
    bool anim = m->animMs < (u32)kOpenMs;
    if (anim) {
        under = sg_alloc(sizeof(u32) * (size_t)g->w * (size_t)g->h);
        memcpy(under, g->fb, sizeof(u32) * (size_t)g->w * (size_t)g->h);
    }
    draw(g, m);
    if (anim) {
        float t = (float)m->animMs / (float)kOpenMs;
        if (m->closing) t = 1.0f - t;
        scaled_overlay(g, under, 0.85f + 0.15f * t, (int)(255.0f * t));
        free(under);
    }
    if (m->closing && m->animMs >= (u32)kOpenMs) {
        m->mode = MENU_CLOSED;
        m->closing = false;
        field_draw(g);
        sg_log(g, "menu: closed");
    }
    return true;
}

bool title_menu_frame(SG *g, u32 ms) {
    Menu *m = g->b.menu;
    if (!m || m->mode != MENU_TITLE) return false;
    m->animMs += ms;
    update(g, m);
    if (m->mode == MENU_TITLE && m->page == P_MAIN) m->mode = MENU_CLOSED;
    if (m->mode != MENU_TITLE) return g->scene != SCENE_TITLE;
    fill_rect(g->fb, g->w, g->h, 0, 0, g->w, g->h, 0x80000000u);
    draw(g, m);
    return true;
}
