#include "sa.h"

enum {
    PLATFORM_X = -62,
    PLATFORM_Y = 504,
    FRONT_X = 108,
    FRONT_Y = 451,
    BACK_X = 27,
    BACK_Y = 492,
    STEP_X = 57,
    STEP_Y = 29,
    TILE_CX = 74,
    TILE_CY = 110,
    TILE_H = 150,
    RESERVE_X = -16,
    RESERVE_Y = 588,
    FEET = 17,
    LABEL_Y = 695,
    COUNT_RIGHT = 44,
    LIMIT_X = 66,
    DIGIT_Y = 700,
};

static Image *half_size(const Image *im, int sy, int h) {
    if (!im) return NULL;
    Image *out = sa_image_new(im->w / 2, h / 2);
    for (int y = 0; y < out->h; y++)
        for (int x = 0; x < out->w; x++) {
            u32 sum[4] = {0, 0, 0, 0};
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    u32 p = im->px[(size_t)(sy + y * 2 + j) * im->w + x * 2 + i], a = p >> 24;
                    sum[0] += (p & 0xff) * a;
                    sum[1] += (p >> 8 & 0xff) * a;
                    sum[2] += (p >> 16 & 0xff) * a;
                    sum[3] += a;
                }
            u32 a = sum[3];
            out->px[(size_t)y * out->w + x] =
                a ? (a / 4) << 24 | (sum[2] / a) << 16 | (sum[1] / a) << 8 | sum[0] / a : 0;
        }
    return out;
}

static Image *crop(const Image *im, int sy, int h) {
    if (!im || sy + h > im->h) return NULL;
    Image *out = sa_image_new(im->w, h);
    memcpy(out->px, im->px + (size_t)sy * im->w, sizeof(u32) * (size_t)im->w * (size_t)h);
    return out;
}

bool sa_party_init(Party *p, Vfs *vfs) {
    memset(p, 0, sizeof *p);
    p->vfs = vfs;
    sa_table_load(&p->positions, vfs, "Position.obj");
    if (!sa_table_load(&p->units, vfs, "unit.obj") || !sa_table_load(&p->abilities, vfs, "ability.obj")) return false;
    p->platform = sa_load_image(vfs, "m996bt1a.agf");
    Image *tiles = sa_load_image(vfs, "mfloor01.agf");
    p->tile = tiles;
    p->smallTile = half_size(tiles, 0, TILE_H);
    p->gaugeFrame = sa_load_image(vfs, "bat_o00.agf");
    Image *hp = sa_load_image(vfs, "bat_o01.agf");
    p->hpGauge = crop(hp, 0, 3);
    sa_image_free(hp);
    p->mpGauge = sa_load_image(vfs, "bat_o03.agf");
    p->label = sa_load_image(vfs, "mai_f00.agf");
    p->digits = sa_load_image(vfs, "dgn_f00.agf");
    return true;
}

void sa_party_free(Party *p) {
    for (int i = 0; i < PARTY_SLOTS; i++) sa_image_free(p->sprite[i]);
    for (int i = 0; i < PARTY_RESERVE; i++) sa_image_free(p->reserveSprite[i]);
    sa_image_free(p->platform);
    sa_image_free(p->tile);
    sa_image_free(p->smallTile);
    sa_image_free(p->gaugeFrame);
    sa_image_free(p->hpGauge);
    sa_image_free(p->mpGauge);
    sa_image_free(p->label);
    sa_image_free(p->digits);
    sa_table_free(&p->units);
    sa_table_free(&p->abilities);
    sa_table_free(&p->positions);
    memset(p, 0, sizeof *p);
}

static bool solid(const Image *im, int x, int y) { return (im->px[(size_t)y * im->w + x] >> 24) > 128; }

static bool clear_row(const Image *im, int y) {
    for (int x = 0; x < im->w; x++)
        if (solid(im, x, y)) return false;
    return true;
}

static bool clear_column(const Image *im, int x, int h) {
    for (int y = 0; y < h; y++)
        if (solid(im, x, y)) return false;
    return true;
}

static void find_layout(const Image *im, int *cols, int *rows) {
    int y = 0;
    while (y < im->h && clear_row(im, y)) y++;
    while (y < im->h && !clear_row(im, y)) y++;
    int ch = im->h;
    for (int d = y; d <= im->h; d++)
        if (im->h % d == 0 && (d == im->h || clear_row(im, d) || clear_row(im, d - 1))) {
            ch = d;
            break;
        }
    int x = 0;
    while (x < im->w && clear_column(im, x, ch)) x++;
    while (x < im->w && !clear_column(im, x, ch)) x++;
    int cw = im->w;
    for (int d = x; d <= im->w; d++)
        if (im->w % d == 0 && (d == im->w || clear_column(im, d, ch) || clear_column(im, d - 1, ch))) {
            cw = d;
            break;
        }
    *cols = im->w / cw;
    *rows = im->h / ch;
}

void sa_sprite_layout(const Image *im, int *cols, int *rows) {
    static struct {
        const Image *im;
        int w, h, cols, rows;
    } cache[32];
    static int next;
    for (int i = 0; i < 32; i++)
        if (cache[i].im == im && cache[i].w == im->w && cache[i].h == im->h) {
            *cols = cache[i].cols;
            *rows = cache[i].rows;
            return;
        }
    find_layout(im, cols, rows);
    cache[next].im = im;
    cache[next].w = im->w;
    cache[next].h = im->h;
    cache[next].cols = *cols;
    cache[next].rows = *rows;
    next = (next + 1) % 32;
}

int sa_sprite_filled_cells(const Image *im) {
    int cols, rows;
    sa_sprite_layout(im, &cols, &rows);
    int cw = im->w / cols, ch = im->h / rows, n = 0;
    for (int c = 0; c < cols * rows; c++) {
        bool any = false;
        for (int y = 0; y < ch && !any; y++)
            for (int x = 0; x < cw && !any; x++) any = im->px[(size_t)(c / cols * ch + y) * im->w + c % cols * cw + x] >> 24;
        if (!any) break;
        n++;
    }
    return n;
}

int sa_party_unit_row(const Party *p, int id) {
    for (int r = 0; r < p->units.rows; r++)
        if (sa_table_int(&p->units, r, 0) == id) return r;
    return -1;
}

void sa_party_place(Party *p, Vfs *vfs, int slot, int id) {
    if (slot < 0 || slot >= PARTY_SLOTS) return;
    sa_image_free(p->sprite[slot]);
    p->sprite[slot] = NULL;
    p->unit[slot] = id;
    int r = sa_party_unit_row(p, id);
    if (r < 0) {
        p->unit[slot] = 0;
        return;
    }
    p->hp[slot] = sa_table_int(&p->units, r, 23);
    p->mp[slot] = sa_table_int(&p->units, r, 24);
    p->level[slot] = 1;
    p->exp[slot] = 0;
    p->order[slot] = (Order){ORDER_FREE, -1, AIM_FREE};
    p->equip[slot] = 0;
    p->sp[slot] = sa_table_int(&p->units, r, 37);
    memset(p->trained[slot], 0, sizeof p->trained[slot]);
    char name[24];
    snprintf(name, sizeof name, "u%04da0.agf", sa_table_int(&p->units, r, 11));
    p->sprite[slot] = sa_load_image(vfs, name);
    p->cells[slot] = p->sprite[slot] ? sa_sprite_filled_cells(p->sprite[slot]) : 0;
}

void sa_party_new_game(Party *p, Vfs *vfs) {
    p->items = 0;
    p->benchCount = 0;
    p->rosterCount = 1;
    p->roster[0] = 1;
    memset(p->post, 0, sizeof p->post);
    for (int i = 0; i < PARTY_SLOTS; i++) sa_party_place(p, vfs, i, 0);
    sa_party_place(p, vfs, PARTY_FRONT + 1, 1);
}

bool sa_party_tick(Party *p, u32 ms) {
    u32 before = p->ms / PARTY_CELL_MS;
    p->ms += ms;
    if (p->ms / PARTY_CELL_MS == before) return false;
    for (int s = 0; s < PARTY_SLOTS; s++)
        if (p->unit[s] && p->cells[s] > 1) return true;
    return false;
}

static void slot_tile(int slot, int boardX, int boardY, int *x, int *y) {
    int k = slot % 3;
    bool front = slot >= PARTY_FRONT;
    *x = boardX - PLATFORM_X + (front ? FRONT_X : BACK_X) + k * STEP_X;
    *y = boardY - PLATFORM_Y + (front ? FRONT_Y : BACK_Y) + k * STEP_Y;
}

void sa_party_tile_center(int slot, int boardX, int boardY, int *x, int *y) {
    slot_tile(slot, boardX, boardY, x, y);
    *x += TILE_CX;
    *y += TILE_CY;
}

void sa_party_draw_tile(Party *p, u32 *fb, int cx, int cy, int frame) {
    if (p->tile) sa_blit_rect(fb, p->tile, 0, frame * TILE_H, p->tile->w, TILE_H, cx - TILE_CX, cy - TILE_CY);
}

void sa_party_draw_tiles(Party *p, u32 *fb, int boardX, int boardY, const int *frames) {
    for (int k = 0; k < PARTY_RESERVE; k++)
        sa_blit(fb, p->smallTile, boardX - PLATFORM_X + RESERVE_X + k * STEP_X / 2, boardY - PLATFORM_Y + RESERVE_Y + k * STEP_Y / 2);
    for (int s = 0; s < PARTY_SLOTS; s++) {
        int x, y;
        slot_tile(s, boardX, boardY, &x, &y);
        sa_party_draw_tile(p, fb, x + TILE_CX, y + TILE_CY, frames ? frames[s] : 0);
    }
}

static void gauge(u32 *fb, const Image *im, int value, int max, int x, int y) {
    if (!im || max <= 0) return;
    int w = value <= 0 ? 0 : value >= max ? 50 : (value * 50 + max - 1) / max;
    sa_blit_rect(fb, im, 0, 0, w, im->h, x, y);
}

int sa_party_rank(const Party *p) {
    int sum = 0;
    for (int i = 0; i < PARTY_SLOTS; i++) {
        int r = p->unit[i] ? sa_party_unit_row(p, p->unit[i]) : -1;
        if (r >= 0) sum += sa_table_int(&p->units, r, 15);
    }
    return sum;
}

void sa_party_draw_bars(Party *p, u32 *fb, int x, int y, int hp, int maxHp, int mp, int maxMp) {
    sa_blit(fb, p->gaugeFrame, x - 3, y - 12);
    sa_blit(fb, p->gaugeFrame, x - 3, y - 8);
    gauge(fb, p->hpGauge, hp, maxHp, x - 2, y - 11);
    gauge(fb, p->mpGauge, mp, maxMp, x - 2, y - 7);
}

void sa_party_draw_diorama(Party *p, u32 *fb, int rankLimit, const Image *platform) {
    sa_blit(fb, platform ? platform : p->platform, PLATFORM_X, PLATFORM_Y);
    sa_party_draw_tiles(p, fb, PLATFORM_X, PLATFORM_Y, NULL);
    for (int k = 0; k < 3; k++)
        for (int row = 0; row < 2; row++) {
            int s = row ? PARTY_FRONT + k : k;
            const Image *im = p->sprite[s];
            if (!p->unit[s] || !im) continue;
            int cols, rows;
            sa_sprite_layout(im, &cols, &rows);
            int cw = im->w / cols, ch = im->h / rows;
            int cell = p->cells[s] > 0 ? (int)(p->ms / PARTY_CELL_MS % (u32)p->cells[s]) : 0;
            int cx, cy;
            sa_party_tile_center(s, PLATFORM_X, PLATFORM_Y, &cx, &cy);
            int x = cx - cw / 2, y = cy + FEET - ch;
            sa_blit_rect(fb, im, cell % cols * cw, cell / cols * ch, cw, ch, x, y);
            sa_party_draw_bars(p, fb, cx - 22, y, p->hp[s], sa_party_max_hp(p, s), p->mp[s], sa_party_max_mp(p, s));
        }
    for (int t = 0; t < PARTY_RESERVE; t++) {
        int b = sa_party_reserve_at(p, t), unit = b >= 0 ? p->bench[b].unit : 0;
        if (p->reserveUnit[t] != unit) {
            sa_image_free(p->reserveSprite[t]);
            p->reserveSprite[t] = NULL;
            p->reserveUnit[t] = unit;
            if (unit && p->vfs) {
                char name[24];
                snprintf(name, sizeof name, "u%04da0.agf", sa_table_int(&p->units, sa_party_unit_row(p, unit), 11));
                Image *full = sa_load_image(p->vfs, name);
                if (full) {
                    int cols, rows;
                    sa_sprite_layout(full, &cols, &rows);
                    Image *cell = sa_image_new(full->w / cols, full->h / rows);
                    for (int y = 0; y < cell->h; y++) memcpy(cell->px + (size_t)y * cell->w, full->px + (size_t)y * full->w, sizeof(u32) * (size_t)cell->w);
                    p->reserveSprite[t] = half_size(cell, 0, cell->h);
                    sa_image_free(cell);
                    sa_image_free(full);
                }
            }
        }
        const Image *im = p->reserveSprite[t];
        if (!im) continue;
        int cx = RESERVE_X + t * STEP_X / 2 + p->smallTile->w / 2;
        int cy = RESERVE_Y + t * STEP_Y / 2 + p->smallTile->h / 2;
        sa_blit(fb, im, cx - im->w / 2, cy + FEET / 2 - im->h);
    }
    sa_blit(fb, p->label, 1, LABEL_Y);
    char n[16];
    snprintf(n, sizeof n, "%d", sa_party_rank(p));
    int len = (int)strlen(n);
    for (int i = 0; i < len; i++) sa_blit_rect(fb, p->digits, (n[i] - '0') * 10, 0, 10, 13, COUNT_RIGHT - (len - i) * 10, DIGIT_Y);
    snprintf(n, sizeof n, "%d", rankLimit);
    for (int i = 0; n[i]; i++) sa_blit_rect(fb, p->digits, (n[i] - '0') * 10, 0, 10, 13, LIMIT_X + i * 10, DIGIT_Y);
}

static int period(const int *prof, int w, double limit) {
    long total = 0;
    for (int x = 0; x < w; x++) total += prof[x];
    if (!total) return 0;
    int best = 0;
    double bestErr = 0;
    for (int cw = 24; cw <= w / 2; cw++) {
        int cols = w / cw;
        if (w - cols * cw >= cols) continue;
        long err = 0, sum = 0;
        for (int x = 0; x + cw < cols * cw; x++) {
            err += abs(prof[x] - prof[x + cw]);
            sum += prof[x];
        }
        if (!sum) continue;
        double e = (double)err / (double)sum;
        if (!best || e < bestErr - 0.02) best = cw, bestErr = e;
    }
    return best && bestErr < limit ? best : 0;
}

int sa_sprite_split(const Image *im, SpriteFrame *out, int max) {
    int *rows = sa_alloc(sizeof(int) * (size_t)(im->h + 1));
    for (int y = 0; y < im->h; y++) {
        rows[y] = 0;
        for (int x = 0; x < im->w; x++) rows[y] += (im->px[(size_t)y * im->w + x] >> 24) > 128;
    }
    int ch = period(rows, im->h, 0.5);
    free(rows);
    int bands = ch ? im->h / ch : im->h % 2 == 0 ? 2 : 1, bh = im->h / bands, n = 0;
    int *prof = sa_alloc(sizeof(int) * (size_t)(im->w + 1));
    for (int x = 0; x < im->w; x++) {
        prof[x] = 0;
        for (int y = 0; y < bh; y++) prof[x] += (im->px[(size_t)y * im->w + x] >> 24) > 128;
    }
    int cw = period(prof, im->w, 0.35);
    for (int b = 0; b < bands; b++) {
        int runs[SPRITE_FRAMES][2], count = 0, start = -1;
        for (int x = 0; x <= im->w; x++) {
            bool solid = false;
            for (int y = b * bh; x < im->w && y < (b + 1) * bh && !solid; y++) solid = (im->px[(size_t)y * im->w + x] >> 24) > 128;
            if (solid && start < 0) start = x;
            if (!solid && start >= 0) {
                if (count < SPRITE_FRAMES) {
                    runs[count][0] = start;
                    runs[count++][1] = x;
                }
                start = -1;
            }
        }
        int widest = 0;
        for (int i = 0; i < count; i++) widest = runs[i][1] - runs[i][0] > widest ? runs[i][1] - runs[i][0] : widest;
        if (cw && widest > cw) {
            for (int c = 0; c < im->w / cw && n < max; c++) {
                bool any = false;
                for (int y = b * bh; y < (b + 1) * bh && !any; y++)
                    for (int x = c * cw; x < (c + 1) * cw && !any; x++) any = (im->px[(size_t)y * im->w + x] >> 24) > 128;
                if (any) out[n++] = (SpriteFrame){c * cw, b * bh, cw, bh};
            }
            continue;
        }
        for (int i = 0; i < count && n < max; i++) {
            int left = i ? (runs[i - 1][1] + runs[i][0]) / 2 : 0;
            int right = i + 1 < count ? (runs[i][1] + runs[i + 1][0]) / 2 : im->w;
            out[n++] = (SpriteFrame){left, b * bh, right - left, bh};
        }
    }
    free(prof);
    return n;
}

void sa_party_add_item(Party *p, int id, int n) {
    for (int i = 0; i < p->items; i++)
        if (p->itemId[i] == id) {
            p->itemCount[i] += n;
            return;
        }
    if (p->items == PARTY_ITEMS) return;
    p->itemId[p->items] = id;
    p->itemCount[p->items++] = n;
}

int sa_party_item_count(const Party *p, int id) {
    for (int i = 0; i < p->items; i++)
        if (p->itemId[i] == id) return p->itemCount[i];
    return 0;
}

int sa_party_count_units(const Party *p, const char *key) {
    int n = 0;
    for (int s = 0; s < PARTY_SLOTS; s++) {
        if (!p->unit[s]) continue;
        int r = sa_party_unit_row(p, p->unit[s]);
        if (!key || !*key || !strcmp(sa_table_text(&p->units, r, 1), key)) n++;
    }
    return n;
}

static int find_type(const Table *lvup, const char *type) {
    for (int r = 0; r < lvup->rows; r++)
        if (!strcmp(sa_table_text(lvup, r, 1), type)) return r;
    return -1;
}

static int growth(const Table *lvup, const char *type, const char *stat, int level) {
    int r = find_type(lvup, type);
    if (r < 0 || level <= 1) return 0;
    const char *s = sa_table_text(lvup, r, 24);
    const char *p = strstr(s, stat);
    if (!p) return 0;
    int add = 0, per = 0;
    if (sscanf(p + strlen(stat), "+%d/%d", &add, &per) != 2 || per <= 0) return 0;
    return (level - 1) / per * add;
}

static int level_gain(const Table *lvup, const char *type, int col, int level) {
    int r = find_type(lvup, type);
    return r < 0 || level <= 1 ? 0 : (level - 1) * sa_table_int(lvup, r, col);
}

void sa_party_unit_stats(const Party *p, int row, int level, int *out) {
    const Table *u = &p->units;
    static const Table none;
    const Table *lvup = p->lvup ? p->lvup : &none;
    const char *type = sa_table_text(u, row, 19);
    out[0] = sa_table_int(u, row, 23) + level_gain(lvup, type, 4, level);
    out[1] = sa_table_int(u, row, 24) + level_gain(lvup, type, 5, level);
    for (int i = 0; i < 8; i++) out[2 + i] = sa_table_int(u, row, 25 + i);
    int attack = growth(lvup, type, "攻撃", level), defence = growth(lvup, type, "防御", level);
    out[2 + STAT_ATTACK] += attack;
    out[2 + STAT_MAGIC] += attack;
    out[2 + STAT_DEFENCE] += defence;
    out[2 + STAT_RESIST] += defence;
    out[2 + STAT_SPEED] += growth(lvup, type, "速度", level);
    out[2 + STAT_EVASION] += growth(lvup, type, "回避", level);
}

static int slot_value(const Party *p, int slot, int index) {
    int v[10];
    sa_party_unit_stats(p, sa_party_unit_row(p, p->unit[slot]), p->level[slot] > 0 ? p->level[slot] : 1, v);
    return v[index] + p->trained[slot][index];
}

int sa_party_max_hp(const Party *p, int slot) { return slot_value(p, slot, 0); }

int sa_party_max_mp(const Party *p, int slot) { return slot_value(p, slot, 1); }

int sa_party_stat(const Party *p, int slot, int stat) { return slot_value(p, slot, 2 + stat); }

bool sa_party_join(Party *p, Vfs *vfs, const char *key) {
    int r = -1;
    for (int i = 0; i < p->units.rows && r < 0; i++)
        if (!strcmp(sa_table_text(&p->units, i, 1), key)) r = i;
    if (r < 0) return false;
    int id = sa_table_int(&p->units, r, 0);
    for (int i = 0; i < p->rosterCount; i++)
        if (p->roster[i] == id) return true;
    if (p->rosterCount < PARTY_ROSTER) p->roster[p->rosterCount++] = id;
    if (!sa_party_members(p)) {
        sa_party_place(p, vfs, PARTY_FRONT + 1, id);
        return true;
    }
    if (p->benchCount < PARTY_BENCH) {
        Bench *b = &p->bench[p->benchCount++];
        memset(b, 0, sizeof *b);
        b->unit = id;
        b->hp = sa_table_int(&p->units, r, 23);
        b->mp = sa_table_int(&p->units, r, 24);
        b->level = 1;
        b->sp = sa_table_int(&p->units, r, 37);
        b->tile = -1;
    }
    return true;
}

int sa_party_post_of(const Party *p, int unit) {
    for (int i = 1; i < PARTY_POSTS && unit; i++)
        if (p->post[i] == unit) return i;
    return 0;
}

int sa_party_post_index(const Party *p, const char *name) {
    for (int r = 1; r < p->positions.rows && r < PARTY_POSTS; r++)
        if (!strcmp(sa_table_text(&p->positions, r, 0), name)) return r;
    return 0;
}

void sa_party_set_post(Party *p, int post, int unit) {
    if (post <= 0 || post >= PARTY_POSTS) return;
    int was = sa_party_post_of(p, unit);
    if (was) p->post[was] = p->post[post];
    p->post[post] = unit;
}

int sa_party_reserve_at(const Party *p, int tile) {
    for (int i = 0; i < p->benchCount; i++)
        if (p->bench[i].tile == tile) return i;
    return -1;
}

void sa_party_set_reserve(Party *p, int index, int tile) {
    if (index < 0 || index >= p->benchCount || tile >= PARTY_RESERVE) return;
    int there = tile >= 0 ? sa_party_reserve_at(p, tile) : -1;
    if (there >= 0) p->bench[there].tile = p->bench[index].tile;
    p->bench[index].tile = tile;
}

#define SWAP(field)                                                                                                    \
    do {                                                                                                               \
        __typeof__(p->field[0]) tmp_ = p->field[a];                                                                    \
        p->field[a] = p->field[b];                                                                                     \
        p->field[b] = tmp_;                                                                                            \
    } while (0)

void sa_party_swap(Party *p, int a, int b) {
    if (a == b || a < 0 || b < 0 || a >= PARTY_SLOTS || b >= PARTY_SLOTS) return;
    SWAP(unit);
    SWAP(hp);
    SWAP(mp);
    SWAP(level);
    SWAP(exp);
    SWAP(order);
    SWAP(equip);
    SWAP(sp);
    SWAP(sprite);
    SWAP(cells);
    int t[10];
    memcpy(t, p->trained[a], sizeof t);
    memcpy(p->trained[a], p->trained[b], sizeof t);
    memcpy(p->trained[b], t, sizeof t);
}

int sa_party_members(const Party *p) {
    int n = 0;
    for (int i = 0; i < PARTY_SLOTS; i++) n += p->unit[i] != 0;
    return n;
}

bool sa_party_bench(Party *p, int slot) {
    if (slot < 0 || slot >= PARTY_SLOTS || !p->unit[slot] || p->benchCount == PARTY_BENCH || sa_party_members(p) <= 1) return false;
    Bench *b = &p->bench[p->benchCount++];
    b->unit = p->unit[slot];
    b->hp = p->hp[slot];
    b->mp = p->mp[slot];
    b->level = p->level[slot];
    b->exp = p->exp[slot];
    b->equip = p->equip[slot];
    b->sp = p->sp[slot];
    b->tile = -1;
    memcpy(b->trained, p->trained[slot], sizeof b->trained);
    sa_image_free(p->sprite[slot]);
    p->sprite[slot] = NULL;
    p->unit[slot] = 0;
    p->cells[slot] = 0;
    return true;
}

void sa_party_unbench(Party *p, Vfs *vfs, int index, int slot) {
    if (index < 0 || index >= p->benchCount || slot < 0 || slot >= PARTY_SLOTS) return;
    Bench b = p->bench[index];
    memmove(p->bench + index, p->bench + index + 1, sizeof(Bench) * (size_t)(p->benchCount - index - 1));
    p->benchCount--;
    if (p->unit[slot]) sa_party_bench(p, slot);
    sa_party_place(p, vfs, slot, b.unit);
    p->hp[slot] = b.hp;
    p->mp[slot] = b.mp;
    p->level[slot] = b.level;
    p->exp[slot] = b.exp;
    p->equip[slot] = b.equip;
    p->sp[slot] = b.sp;
    memcpy(p->trained[slot], b.trained, sizeof b.trained);
}
