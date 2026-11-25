#include "sa_private.h"

enum {
    MAIN_X = 112,
    MAIN_Y = 86,
    MAIN_DX = 117,
    MAIN_DY = 125,
    BENCH_X = 13,
    BENCH_Y = 366,
    BENCH_DX = 108,
    BENCH_SLOTS = 5,
    TILE_W = 108,
    TILE_H = 118,
    FEET = 96,
    LIST_X = 567,
    LIST_Y = 21,
    ROW_Y = 77,
    ROW_H = 26,
    ROW_W = 400,
    ROWS = 23,
    TAB_Y = 27,
    BADGE_X = 575,
    BADGE_DY = 4,
    NAME_X = 668,
    TEXT_DY = 5,
    STAT_X = 826,
    STAT_W = 34,
    RANK_RIGHT = 316,
    RANK_Y = 26,
    CARD_X = 997,
    CARD_Y = 21,
    BACK_X = 1117,
    BACK_Y = 675,
    BTN_W = 152,
    BTN_H = 44,
    HOVER_BACK = 1,
    HOVER_MAIN = 100,
    HOVER_BENCH = 200,
    HOVER_ROW = 300,
    HOVER_UNIT = 400,
    HOVER_POST = 500,
    HOVER_TAB = 600,
    HOVER_FIRE = 800,
    FIRE_X = 445,
    FIRE_Y = 536,
    HOVER_DISMISS = 700,
    TAB_X = 321,
    TAB_DX = 79,
    TAB_W = 76,
    TAB_H = 20,
    ORG_POSTS = 9,
    MAZE_TILES = 11,
    LABELS = 20,
    ICON_W = 24,
    ICON_H = 20,
    ICON_DY = 10,
    MAZE_ORG_X = 9,
    MAZE_ORG_Y = 421,
    BOOSTED = 0x0068b7,
    POST_W = 98,
    POST_H = 64,
    DISMISS_X = 372,
    DISMISS_Y = 675,
    DISMISS_W = 180,
    DISMISS_H = 27,
    INFO_X = 9,
    INFO_RIGHT = 222,
    INFO_Y = 31,
    INFO_DY = 20,
};

enum { TAB_LIST, TAB_ORG, TAB_FORM };

static const struct {
    int post, x, y;
} orgTile[ORG_POSTS] = {{1, 299, 112}, {2, 412, 168}, {3, 165, 187}, {7, 5, 214}, {4, 249, 229},
                        {8, 89, 255}, {5, 333, 271}, {9, 173, 298}, {6, 417, 313}};

static const struct {
    int post, x, y;
} mazeTile[MAZE_TILES] = {{10, 298, 382}, {11, 418, 442}, {12, 15, 459}, {19, 320, 498}, {13, 99, 501}, {14, 183, 543}, {16, 12, 556}, {20, 442, 557},
                          {15, 267, 585}, {17, 96, 598}, {18, 180, 640}};

enum { ART_TOP, ART_BOTTOM, ART_DISMISS, ART_TILE, ART_LIST, ART_ROWS, ART_BADGE, ART_BACK, ART_BG, ART_PLATFORM, ART_RANK, ART_FORM, ART_TAB0, ART_ORG = ART_TAB0 + 6, ART_BTN_LIST, ART_BTN_ORG, ART_POST_TILE, ART_FIRE, ART_MAZE_ORG, ART_MAZE_TILE, ART_ICON, ART_RAIL_SUPPORT, ART_RAIL_DEPUTY, ART_RAIL_RAID, ART_LABEL0, ART_COUNT = ART_LABEL0 + LABELS };

static const char *const artNames[ART_COUNT] = {"ums_a00.agf", "ums_a01.agf", "ums_a02.agf", "ums_z00.agf", "uts_c00.agf", "uts_c01.agf",
                                                "uts_h98.agf", "uts_g00.agf", "bg009a0.agf", "m997bt1a.agf", "uts_b03.agf", "uts_b02.agf", "uts_d01.agf", "uts_d02.agf",
                                                "uts_d03.agf", "uts_d04.agf", "uts_d05.agf", "uts_d06.agf", "uts_a01.agf", "uts_b00.agf", "uts_b01.agf",
                                                "uts_w00.agf", "uts_x01.agf", "uts_a03.agf", "uts_v00.agf", "uts_i02.agf", "uts_y00.agf", "uts_y01.agf", "uts_y02.agf", "uts_h00.agf", "uts_h01.agf",
                                                "uts_h02.agf", "uts_h03.agf", "uts_h04.agf", "uts_h05.agf", "uts_h06.agf", "uts_h07.agf", "uts_h08.agf",
                                                "uts_h09.agf", "uts_h10.agf", "uts_h11.agf", "uts_h12.agf", "uts_h13.agf", "uts_h14.agf", "uts_h15.agf",
                                                "uts_h16.agf", "uts_h17.agf", "uts_h18.agf", "uts_h19.agf"};

static const int mainSlot[6] = {PARTY_FRONT, PARTY_FRONT + 1, PARTY_FRONT + 2, 0, 1, 2};

#define NO_EDGE 0xffffffffu

void sa_deploy_init(Deploy *d, Vfs *vfs) {
    memset(d, 0, sizeof *d);
    for (int i = 0; i < ART_COUNT; i++) d->art[i] = sa_load_image(vfs, artNames[i]);
    d->hover = d->picked = -1;
}

void sa_deploy_free(Deploy *d) {
    for (int i = 0; i < ART_COUNT; i++) sa_image_free(d->art[i]);
    for (int i = 0; i < PARTY_BENCH; i++) sa_image_free(d->benchSprite[i]);
    memset(d, 0, sizeof *d);
}

void sa_deploy_open(SA *s) {
    Deploy *d = &s->deploy;
    d->hover = d->picked = -1;
    d->tab = TAB_FORM;
    s->mode = MODE_DEPLOY;
    s->dirty = true;
}

static bool in(int x, int y, int rx, int ry, int w, int h) { return x >= rx && x < rx + w && y >= ry && y < ry + h; }

static int main_x(int i) { return MAIN_X + i % 3 * MAIN_DX; }

static int main_y(int i) { return MAIN_Y + i / 3 * MAIN_DY; }

static int rows(const SA *s, int *keys) {
    int n = 0;
    for (int i = 0; i < 6; i++)
        if (s->party.unit[mainSlot[i]]) keys[n++] = HOVER_MAIN + i;
    for (int i = 0; i < s->party.benchCount; i++) keys[n++] = HOVER_UNIT + i;
    return n;
}

static bool posts_open(const SA *s) { return s->vm.f[F_POSTS] != 0; }

static bool maze_tile_open(const SA *s, int i) { return sa_maze_post_open(&s->maze, &s->vm, mazeTile[i].post); }

static bool on_tile(const Image *im, int fx, int fy, int x0, int y0, int x, int y) {
    if (!im || x < x0 || y < y0 || x >= x0 + POST_W || y >= y0 + POST_H) return false;
    return im->px[(size_t)(fy + y - y0) * im->w + fx + x - x0] >> 24 > 128;
}

static int hit(SA *s, int x, int y) {
    Deploy *d = &s->deploy;
    if (in(x, y, BACK_X, BACK_Y, BTN_W, BTN_H)) return HOVER_BACK;
    if (posts_open(s))
        for (int t = TAB_ORG; t <= TAB_FORM; t++)
            if (in(x, y, TAB_X + t * TAB_DX, TAB_Y - 1, TAB_W, TAB_H)) return HOVER_TAB + t;
    if (d->tab == TAB_ORG) {
        if (in(x, y, DISMISS_X, DISMISS_Y, DISMISS_W, DISMISS_H)) return HOVER_DISMISS;
        for (int i = 0; i < ORG_POSTS; i++)
            if (on_tile(d->art[ART_POST_TILE], 0, 0, orgTile[i].x, orgTile[i].y, x, y)) return HOVER_POST + orgTile[i].post;
        for (int i = 0; i < MAZE_TILES; i++)
            if (maze_tile_open(s, i) && on_tile(d->art[ART_MAZE_TILE], 0, 0, mazeTile[i].x, mazeTile[i].y, x, y)) return HOVER_POST + mazeTile[i].post;
        int keys[PARTY_SLOTS + PARTY_BENCH], n = rows(s, keys);
        for (int i = 0; i < n && i < ROWS; i++)
            if (in(x, y, LIST_X, ROW_Y + i * ROW_H, ROW_W, ROW_H)) return HOVER_ROW + i;
        return -1;
    }
    if (in(x, y, FIRE_X, FIRE_Y, TILE_W, TILE_H)) return HOVER_FIRE;
    for (int i = 0; i < 6; i++)
        if (in(x, y, main_x(i), main_y(i), TILE_W, TILE_H)) return HOVER_MAIN + i;
    for (int i = 0; i < BENCH_SLOTS; i++)
        if (in(x, y, BENCH_X + i * BENCH_DX, BENCH_Y, TILE_W, TILE_H)) return HOVER_BENCH + i;
    int keys[PARTY_SLOTS + PARTY_BENCH], n = rows(s, keys);
    for (int i = 0; i < n && i < ROWS; i++)
        if (in(x, y, LIST_X, ROW_Y + i * ROW_H, ROW_W, ROW_H)) return HOVER_ROW + i;
    return -1;
}

static int resolve(SA *s, int h) {
    if (h >= HOVER_ROW && h < HOVER_UNIT) {
        int keys[PARTY_SLOTS + PARTY_BENCH], n = rows(s, keys);
        return h - HOVER_ROW < n ? keys[h - HOVER_ROW] : -1;
    }
    return h;
}

static int rank_with(const SA *s, int slot, int unit) {
    int sum = 0;
    for (int i = 0; i < PARTY_SLOTS; i++) {
        int u = i == slot ? unit : s->party.unit[i];
        int r = u ? sa_party_unit_row(&s->party, u) : -1;
        if (r >= 0) sum += sa_table_int(&s->party.units, r, 15);
    }
    return sum;
}

static bool is_main(int k) { return k >= HOVER_MAIN && k < HOVER_MAIN + 6; }

static bool is_post(int k) { return k > HOVER_POST && k < HOVER_POST + PARTY_POSTS; }

static bool is_tile(int k) { return k >= HOVER_BENCH && k < HOVER_BENCH + BENCH_SLOTS; }

static int bench_index(const SA *s, int k) {
    if (is_tile(k)) return sa_party_reserve_at(&s->party, k - HOVER_BENCH);
    if (k >= HOVER_UNIT && k - HOVER_UNIT < s->party.benchCount) return k - HOVER_UNIT;
    return -1;
}

static int unit_of(SA *s, int k) {
    Party *p = &s->party;
    if (is_main(k)) return p->unit[mainSlot[k - HOVER_MAIN]];
    if (is_post(k)) return p->post[k - HOVER_POST];
    int b = bench_index(s, k);
    return b >= 0 ? p->bench[b].unit : 0;
}

static void apply_post(SA *s, int from, int to) {
    Party *p = &s->party;
    int unit = unit_of(s, from);
    if (to == HOVER_DISMISS) {
        if (is_post(from)) p->post[from - HOVER_POST] = 0;
        return;
    }
    if (!unit || !is_post(to)) return;
    if (is_main(from)) {
        if (!sa_party_bench(p, mainSlot[from - HOVER_MAIN])) return;
        p->bench[p->benchCount - 1].tile = -1;
    } else if (!is_post(from)) {
        int b = bench_index(s, from);
        if (b >= 0) p->bench[b].tile = -1;
    }
    if (is_post(from)) {
        int other = p->post[to - HOVER_POST];
        p->post[to - HOVER_POST] = unit;
        p->post[from - HOVER_POST] = other;
        return;
    }
    sa_party_set_post(p, to - HOVER_POST, unit);
}

static void apply(SA *s, int from, int to) {
    Party *p = &s->party;
    if (s->deploy.tab == TAB_ORG) {
        apply_post(s, from, to);
        return;
    }
    int b = bench_index(s, from);
    if (to == HOVER_FIRE) {
        if (is_main(from) && sa_party_bench(p, mainSlot[from - HOVER_MAIN])) p->bench[p->benchCount - 1].tile = -1;
        else if (b >= 0) p->bench[b].tile = -1;
        return;
    }
    if (is_main(from) && is_main(to))
        sa_party_swap(p, mainSlot[from - HOVER_MAIN], mainSlot[to - HOVER_MAIN]);
    else if (is_main(from) && (is_tile(to) || to >= HOVER_UNIT)) {
        if (sa_party_bench(p, mainSlot[from - HOVER_MAIN]) && is_tile(to)) sa_party_set_reserve(p, p->benchCount - 1, to - HOVER_BENCH);
    } else if (b >= 0 && is_main(to)) {
        int slot = mainSlot[to - HOVER_MAIN];
        if (rank_with(s, slot, p->bench[b].unit) <= s->vm.c[C_RANK_MAX]) sa_party_unbench(p, &s->vfs, b, slot);
    } else if (b >= 0 && is_tile(to))
        sa_party_set_reserve(p, b, to - HOVER_BENCH);
}

static const Image *bench_sprite(SA *s, int i) {
    Deploy *d = &s->deploy;
    int unit = s->party.bench[i].unit;
    if (d->benchUnit[i] != unit) {
        sa_image_free(d->benchSprite[i]);
        d->benchSprite[i] = NULL;
        d->benchUnit[i] = unit;
        int r = sa_party_unit_row(&s->party, unit);
        char name[24];
        snprintf(name, sizeof name, "u%04da0.agf", sa_table_int(&s->party.units, r, 11));
        d->benchSprite[i] = sa_load_image(&s->vfs, name);
    }
    return d->benchSprite[i];
}

static void sprite(u32 *fb, const Image *im, int x, int y) {
    if (!im) return;
    int cols, rows;
    sa_sprite_layout(im, &cols, &rows);
    int cw = im->w / cols, ch = im->h / rows;
    sa_blit_rect(fb, im, 0, 0, cw, ch, x + TILE_W / 2 - cw / 2, y + FEET - ch);
}

static void tile(const Deploy *d, u32 *fb, int x, int y, bool lit) {
    sa_blit_rect(fb, d->art[ART_TILE], lit ? TILE_W : 0, 0, TILE_W, TILE_H, x, y);
}

static void draw_org(SA *s, u32 *fb);
static void draw_form(SA *s, u32 *fb);
static void draw_list(SA *s, u32 *fb);

static void draw(SA *s) {
    Deploy *d = &s->deploy;
    u32 *fb = s->fb;
    sa_blit(fb, d->art[ART_BG], 0, 0);
    if (d->tab == TAB_ORG) draw_org(s, fb);
    else draw_form(s, fb);
    sa_blit_rect(fb, d->art[ART_RANK], 0, 0, 92, 21, 230, 26);
    if (posts_open(s)) {
        const Image *btn[3] = {d->art[ART_BTN_LIST], d->art[ART_BTN_ORG], d->art[ART_FORM]};
        for (int t = 0; t < 3; t++)
            sa_blit_rect(fb, btn[t], d->tab == t ? TAB_W : 0, d->hover == HOVER_TAB + t ? TAB_H : 0, TAB_W, TAB_H, TAB_X + t * TAB_DX, TAB_Y - 1);
    } else
        sa_blit_rect(fb, d->art[ART_FORM], TAB_W, 0, TAB_W, TAB_H, TAB_X + 2 * TAB_DX, TAB_Y - 1);
    draw_list(s, fb);
}

static void draw_form(SA *s, u32 *fb) {
    Deploy *d = &s->deploy;
    Party *p = &s->party;
    sa_blit(fb, d->art[ART_TOP], 2, 21);
    sa_blit(fb, d->art[ART_BOTTOM], 2, 338);
    sa_blit(fb, d->art[ART_DISMISS], 438, 503);
    for (int i = 0; i < 6; i++) {
        int k = HOVER_MAIN + i;
        tile(d, fb, main_x(i), main_y(i), d->picked == k || d->hover == k);
        if (p->unit[mainSlot[i]] && d->picked != k) sprite(fb, p->sprite[mainSlot[i]], main_x(i), main_y(i));
    }
    for (int i = 0; i < BENCH_SLOTS; i++) {
        int k = HOVER_BENCH + i, x = BENCH_X + i * BENCH_DX;
        tile(d, fb, x, BENCH_Y, d->picked == k || d->hover == k);
        int b = sa_party_reserve_at(p, i);
        if (b >= 0 && d->picked != k && d->picked != HOVER_UNIT + b) sprite(fb, bench_sprite(s, b), x, BENCH_Y);
    }
    tile(d, fb, FIRE_X, FIRE_Y, d->hover == HOVER_FIRE);
}

static const Image *unit_sprite(SA *s, int unit) {
    for (int i = 0; i < s->party.benchCount; i++)
        if (s->party.bench[i].unit == unit) return bench_sprite(s, i);
    for (int i = 0; i < PARTY_SLOTS; i++)
        if (s->party.unit[i] == unit) return s->party.sprite[i];
    return NULL;
}

static void info_line(u32 *fb, const char *t, int y) {
    sa_text_draw(fb, SA_W, SA_H, t, -1, INFO_RIGHT - sa_text_width(t, -1, 15), y, 15, 0x000000, NO_EDGE);
}

static void post_tile(SA *s, u32 *fb, const Image *art, int post, int x, int y) {
    Deploy *d = &s->deploy;
    int k = HOVER_POST + post, fx = d->picked == k ? 0 : d->hover == k ? POST_W : 0, fy = d->picked == k ? POST_H : 0;
    sa_blit_rect(fb, art, fx, fy, POST_W, POST_H, x, y);
}

static void post_unit(SA *s, u32 *fb, int post, int x, int y) {
    Party *p = &s->party;
    int unit = p->post[post];
    if (!unit || s->deploy.picked == HOVER_POST + post) return;
    sprite(fb, unit_sprite(s, unit), x + POST_W / 2 - TILE_W / 2, y + POST_H / 2 - FEET + 6);
    int icon = sa_party_post_icon(p, unit, post);
    if (icon >= 0) sa_blit_rect(fb, s->deploy.art[ART_ICON], icon % 2 * ICON_W, icon / 2 * ICON_H, ICON_W, ICON_H, x, y + ICON_DY);
}

static void draw_org(SA *s, u32 *fb) {
    Deploy *d = &s->deploy;
    Party *p = &s->party;
    sa_blit(fb, d->art[ART_ORG], 2, 21);
    if (s->vm.f[F_MAZE]) sa_blit(fb, d->art[ART_MAZE_ORG], MAZE_ORG_X, MAZE_ORG_Y);
    if (s->vm.f[F_MAZE]) {
        if (sa_maze_post_open(&s->maze, &s->vm, 20)) sa_blit(fb, d->art[ART_RAIL_SUPPORT], 358, 427);
        if (sa_maze_post_open(&s->maze, &s->vm, 11)) sa_blit(fb, d->art[ART_RAIL_DEPUTY], 357, 422);
        if (sa_maze_post_open(&s->maze, &s->vm, 19)) sa_blit(fb, d->art[ART_RAIL_RAID], 348, 427);
    }
    for (int i = 0; i < ORG_POSTS; i++) post_tile(s, fb, d->art[ART_POST_TILE], orgTile[i].post, orgTile[i].x, orgTile[i].y);
    for (int i = 0; i < MAZE_TILES; i++)
        if (maze_tile_open(s, i)) post_tile(s, fb, d->art[ART_MAZE_TILE], mazeTile[i].post, mazeTile[i].x, mazeTile[i].y);
    for (int i = 0; i < ORG_POSTS; i++) post_unit(s, fb, orgTile[i].post, orgTile[i].x, orgTile[i].y);
    for (int i = 0; i < MAZE_TILES; i++)
        if (maze_tile_open(s, i)) post_unit(s, fb, mazeTile[i].post, mazeTile[i].x, mazeTile[i].y);
    sa_blit_rect(fb, d->art[ART_FIRE], 0, d->hover == HOVER_DISMISS ? DISMISS_H : 0, DISMISS_W, DISMISS_H, DISMISS_X, DISMISS_Y);
    int focus = is_post(d->hover) ? d->hover : is_post(d->picked) ? d->picked : -1;
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    if (focus > 0) {
        int post = focus - HOVER_POST, unit = p->post[post], r = unit ? sa_party_unit_row(p, unit) : -1;
        info_line(fb, sa_table_text(&p->positions, post, 0), INFO_Y);
        if (r >= 0) {
            char v[16];
            int gov, lead;
            sa_party_post_stats(p, unit, post, &gov, &lead);
            info_line(fb, sa_table_text(&p->units, r, 2), INFO_Y + INFO_DY);
            snprintf(v, sizeof v, "%d", gov);
            info_line(fb, v, INFO_Y + 2 * INFO_DY);
            snprintf(v, sizeof v, "%d", lead);
            info_line(fb, v, INFO_Y + 3 * INFO_DY);
        } else {
            info_line(fb, "--", INFO_Y + 2 * INFO_DY);
            info_line(fb, "--", INFO_Y + 3 * INFO_DY);
        }
    } else {
        info_line(fb, "--", INFO_Y + 2 * INFO_DY);
        info_line(fb, "--", INFO_Y + 3 * INFO_DY);
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
}

static void draw_list(SA *s, u32 *fb) {
    Deploy *d = &s->deploy;
    Party *p = &s->party;
    char rank[24];
    snprintf(rank, sizeof rank, "%d/ %d", sa_party_rank(p), s->vm.c[C_RANK_MAX]);
    sa_text_draw(fb, SA_W, SA_H, rank, -1, RANK_RIGHT - sa_text_width(rank, -1, 15), RANK_Y + 2, 15, 0x000000, NO_EDGE);
    sa_blit(fb, d->art[ART_LIST], LIST_X, LIST_Y);
    for (int i = 0; i < 6; i++) sa_blit_rect(fb, d->art[ART_TAB0 + i], 0, 0, 60, 20, 589 + i * 62, TAB_Y);
    int keys[PARTY_SLOTS + PARTY_BENCH], n = rows(s, keys);
    sa_font_set_spacing(0);
    sa_font_set_plain(true);
    for (int i = 0; i < n && i < ROWS; i++) {
        int k = keys[i], y = ROW_Y + i * ROW_H, unit, level;
        if (is_main(k)) {
            int sl = mainSlot[k - HOVER_MAIN];
            unit = p->unit[sl];
            level = p->level[sl];
        } else {
            unit = p->bench[k - HOVER_UNIT].unit;
            level = p->bench[k - HOVER_UNIT].level;
        }
        int frame = d->picked == k ? 2 : d->hover == HOVER_ROW + i ? 3 : 0;
        sa_blit_rect(fb, d->art[ART_ROWS], 0, frame * ROW_H, ROW_W, ROW_H, LIST_X, y);
        int post = sa_party_post_of(p, unit);
        if (post > 0 && post <= LABELS) sa_blit(fb, d->art[ART_LABEL0 + post - 1], BADGE_X, y + BADGE_DY);
        else if (is_main(k) || p->bench[k - HOVER_UNIT].tile >= 0) sa_blit(fb, d->art[ART_BADGE], BADGE_X, y + BADGE_DY);
        int r = sa_party_unit_row(p, unit);
        sa_text_draw(fb, SA_W, SA_H, sa_table_text(&p->units, r, 2), -1, NAME_X, y + TEXT_DY, 18, 0x000000, NO_EDGE);
        int gov, lead;
        bool boosted = post && sa_party_post_stats(p, unit, post, &gov, &lead);
        if (!boosted) sa_party_post_stats(p, unit, 0, &gov, &lead);
        const int stats[4] = {level, sa_table_int(&p->units, r, 15), gov, lead};
        for (int j = 0; j < 4; j++) {
            char v[12];
            snprintf(v, sizeof v, "%d", stats[j]);
            sa_text_draw(fb, SA_W, SA_H, v, -1, STAT_X + (j + 1) * STAT_W - 6 - sa_text_width(v, -1, 15), y + TEXT_DY + 1, 15, j >= 2 && boosted ? BOOSTED : 0x000000, NO_EDGE);
        }
    }
    sa_font_set_plain(false);
    sa_font_set_spacing(1);
    int focus = resolve(s, d->hover);
    if (focus < 0) focus = d->picked;
    sa_card_draw(s, fb, focus >= HOVER_MAIN && focus < HOVER_BENCH ? mainSlot[focus - HOVER_MAIN] : -1, CARD_X, CARD_Y);
    if (d->tab != TAB_ORG) sa_party_draw_diorama(p, fb, s->vm.c[C_RANK_MAX], d->art[ART_PLATFORM]);
    sa_blit_rect(fb, d->art[ART_BACK], 0, d->hover == HOVER_BACK ? BTN_H : 0, BTN_W, BTN_H, BACK_X, BACK_Y);
    const Image *held = NULL;
    if (is_post(d->picked) && p->post[d->picked - HOVER_POST]) held = unit_sprite(s, p->post[d->picked - HOVER_POST]);
    else if (is_main(d->picked) && p->unit[mainSlot[d->picked - HOVER_MAIN]]) held = p->sprite[mainSlot[d->picked - HOVER_MAIN]];
    else if (bench_index(s, d->picked) >= 0) held = bench_sprite(s, bench_index(s, d->picked));
    if (held) sprite(fb, held, s->mouseX - TILE_W / 2, s->mouseY - FEET / 2);
}

bool sa_deploy_frame(SA *s, u32 ms) {
    if (s->mode != MODE_DEPLOY) return false;
    Deploy *d = &s->deploy;
    if (sa_party_tick(&s->party, ms)) s->dirty = true;
    int h = hit(s, s->mouseX, s->mouseY);
    if (h != d->hover) {
        d->hover = h;
        s->dirty = true;
    }
    if (d->picked >= 0 && (s->mouseX != d->heldX || s->mouseY != d->heldY)) {
        d->heldX = s->mouseX;
        d->heldY = s->mouseY;
        s->dirty = true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (d->picked >= 0) d->picked = -1;
        else s->mode = MODE_TOWN;
        s->dirty = true;
    }
    if (s->clicked) {
        s->clicked = false;
        int k = resolve(s, h);
        if (h == HOVER_BACK) s->mode = MODE_TOWN;
        else if (h >= HOVER_TAB && h < HOVER_TAB + 3) {
            d->tab = h - HOVER_TAB;
            d->picked = -1;
        } else if ((h == HOVER_DISMISS || h == HOVER_FIRE) && d->picked < 0) {
        } else if (k >= 0 && d->picked < 0) d->picked = k;
        else if (k >= 0) {
            apply(s, d->picked, k);
            d->picked = -1;
        } else
            d->picked = -1;
        s->dirty = true;
    }
    if (s->mode == MODE_DEPLOY && s->dirty) {
        draw(s);
        s->serial++;
        s->dirty = false;
    }
    if (s->mode == MODE_TOWN) {
        s->town.hover = -1;
        s->dirty = true;
    }
    return true;
}
