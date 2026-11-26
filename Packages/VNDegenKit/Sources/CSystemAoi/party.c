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
    ANCHOR_DX = 1,
    BARS_GAP = 4,
    SMALL_BARS_DX = 13,
    SMALL_BARS_DY = 38,
    LABEL_Y = 695,
    COUNT_RIGHT = 44,
    SGP_RIGHT = 55,
    SGP_SIZE = 18,
    SGP_TEXT_Y = 699,
    LIMIT_X = 66,
    DIGIT_Y = 700,
};

static void half_into(const Image *im, int sx, int sy, int w, int h, Image *out, int ox, int oy) {
    for (int y = 0; y < h / 2; y++)
        for (int x = 0; x < w / 2; x++) {
            u32 sum[4] = {0, 0, 0, 0};
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    u32 p = im->px[(size_t)(sy + y * 2 + j) * im->w + sx + x * 2 + i], a = p >> 24;
                    sum[0] += (p & 0xff) * a;
                    sum[1] += (p >> 8 & 0xff) * a;
                    sum[2] += (p >> 16 & 0xff) * a;
                    sum[3] += a;
                }
            u32 a = sum[3];
            out->px[(size_t)(oy + y) * out->w + ox + x] =
                a ? (a / 4) << 24 | (sum[2] / a) << 16 | (sum[1] / a) << 8 | sum[0] / a : 0;
        }
}

static Image *half_size(const Image *im, int sy, int h) {
    if (!im) return NULL;
    Image *out = sa_image_new(im->w / 2, h / 2);
    half_into(im, 0, sy, im->w, h, out, 0, 0);
    return out;
}

static Image *half_sheet(const Image *im, const SpriteSheet *in, SpriteSheet *out) {
    int cw = in->cellW / 2, ch = in->cellH / 2, rows = (in->frames + in->cols - 1) / in->cols;
    *out = (SpriteSheet){in->cols, cw, ch, in->frames, in->anchorX / 2, in->anchorY / 2};
    Image *half = sa_image_new(cw * in->cols, ch * rows);
    for (int f = 0; f < in->frames; f++)
        half_into(im, f % in->cols * in->cellW, f / in->cols * in->cellH, in->cellW, in->cellH, half, f % in->cols * cw, f / in->cols * ch);
    return half;
}

static void paste(Image *dst, const Image *src, int x, int y) {
    if (!src) return;
    for (int j = 0; j < src->h && y + j < dst->h; j++)
        for (int i = 0; i < src->w && x + i < dst->w; i++) {
            u32 c = src->px[(size_t)j * src->w + i];
            if (c >> 24) dst->px[(size_t)(y + j) * dst->w + x + i] = c;
        }
}

static void build_small_bars(Party *p) {
    if (!p->gaugeFrame) return;
    int w = p->gaugeFrame->w, h = p->gaugeFrame->h + BARS_GAP + 1;
    Image *bars = sa_image_new(w, h);
    paste(bars, p->gaugeFrame, 0, 0);
    paste(bars, p->gaugeFrame, 0, BARS_GAP);
    p->smallBarEmpty = half_size(bars, 0, h);
    paste(bars, p->hpGauge, 1, 1);
    paste(bars, p->mpGauge, 1, BARS_GAP + 1);
    p->smallBarFull = half_size(bars, 0, h);
    sa_image_free(bars);
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
    p->sgp = -1;
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
    build_small_bars(p);
    p->label = sa_load_image(vfs, "mai_f00.agf");
    p->sgpLabel = sa_load_image(vfs, "mai_f01.agf");
    p->digits = sa_load_image(vfs, "dgn_f00.agf");
    return true;
}

void sa_party_free(Party *p) {
    for (int i = 0; i < PARTY_SLOTS; i++) sa_image_free(p->sprite[i]);
    for (int i = 0; i < PARTY_RESERVE; i++) sa_image_free(p->reserveSprite[i]);
    for (int i = 0; i < PARTY_SLOTS + PARTY_BENCH; i++) sa_image_free(p->frontSprite[i]);
    sa_image_free(p->platform);
    sa_image_free(p->tile);
    sa_image_free(p->smallTile);
    sa_image_free(p->gaugeFrame);
    sa_image_free(p->hpGauge);
    sa_image_free(p->mpGauge);
    sa_image_free(p->smallBarEmpty);
    sa_image_free(p->smallBarFull);
    sa_image_free(p->label);
    sa_image_free(p->sgpLabel);
    sa_image_free(p->digits);
    sa_table_free(&p->units);
    sa_table_free(&p->abilities);
    sa_table_free(&p->positions);
    memset(p, 0, sizeof *p);
}

int sa_party_unit_row(const Party *p, int id) {
    for (int r = 0; r < p->units.rows; r++)
        if (sa_table_int(&p->units, r, 0) == id) return r;
    return -1;
}

bool sa_party_unit_knows(const Party *p, int row, int level, int k) { return sa_table_int(&p->units, row, 40 + k * 3) <= level; }

int sa_party_skill_level(const Party *p, int unit, int level) {
    for (int i = 0; i < p->rosterCount; i++)
        if (p->roster[i] == unit && p->skillLevel[i] > level) return p->skillLevel[i];
    return level;
}

void sa_party_set_skill_level(Party *p, int unit, int level) {
    for (int i = 0; i < p->rosterCount; i++)
        if (p->roster[i] == unit) p->skillLevel[i] = level;
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
    p->sprite[slot] = sa_sheet_load(vfs, name, &p->sheet[slot]);
}

void sa_party_new_game(Party *p, Vfs *vfs) {
    p->items = 0;
    p->benchCount = 0;
    p->rosterCount = 1;
    p->roster[0] = 1;
    memset(p->skillLevel, 0, sizeof p->skillLevel);
    memset(p->post, 0, sizeof p->post);
    for (int i = 0; i < PARTY_SLOTS; i++) sa_party_place(p, vfs, i, 0);
    sa_party_place(p, vfs, PARTY_FRONT + 1, 1);
}

bool sa_party_tick(Party *p, u32 ms) {
    u32 before = p->ms / PARTY_CELL_MS;
    p->ms += ms;
    if (p->ms / PARTY_CELL_MS == before) return false;
    for (int s = 0; s < PARTY_SLOTS; s++)
        if (p->unit[s] && p->sheet[s].frames > 1) return true;
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

void sa_party_draw_small_tile(Party *p, u32 *fb, int cx, int cy) { sa_blit(fb, p->smallTile, cx - TILE_CX / 2, cy - TILE_CY / 2); }

void sa_party_draw_tiles(Party *p, u32 *fb, int boardX, int boardY, const int *frames) {
    for (int k = 0; k < PARTY_RESERVE; k++)
        sa_blit(fb, p->smallTile, boardX - PLATFORM_X + RESERVE_X + k * STEP_X / 2, boardY - PLATFORM_Y + RESERVE_Y + k * STEP_Y / 2);
    for (int s = 0; s < PARTY_SLOTS; s++) {
        int x, y;
        slot_tile(s, boardX, boardY, &x, &y);
        sa_party_draw_tile(p, fb, x + TILE_CX, y + TILE_CY, frames ? frames[s] : 0);
    }
}

static int gauge_width(int value, int max) { return max <= 0 || value <= 0 ? 0 : value >= max ? 50 : (value * 50 + max - 1) / max; }

static void gauge(u32 *fb, const Image *im, int value, int max, int x, int y) {
    if (!im || max <= 0) return;
    sa_blit_rect(fb, im, 0, 0, gauge_width(value, max), im->h, x, y);
}

int sa_party_rank(const Party *p) {
    int sum = 0;
    for (int i = 0; i < PARTY_SLOTS; i++) {
        int r = p->unit[i] ? sa_party_unit_row(p, p->unit[i]) : -1;
        if (r >= 0) sum += sa_table_int(&p->units, r, 15);
    }
    for (int i = 0; i < p->benchCount; i++) {
        int r = p->bench[i].tile >= 0 ? sa_party_unit_row(p, p->bench[i].unit) : -1;
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

void sa_party_draw_board(Party *p, u32 *fb, const Image *platform, int ox, int oy, const int *frames) {
    sa_blit(fb, platform ? platform : p->platform, PLATFORM_X + ox, PLATFORM_Y + oy);
    sa_party_draw_tiles(p, fb, PLATFORM_X + ox, PLATFORM_Y + oy, frames);
    for (int k = 0; k < 3; k++)
        for (int row = 0; row < 2; row++) {
            int s = row ? PARTY_FRONT + k : k;
            const Image *im = p->sprite[s];
            if (!p->unit[s] || !im) continue;
            int cx, cy;
            sa_party_tile_center(s, PLATFORM_X + ox, PLATFORM_Y + oy, &cx, &cy);
            sa_sheet_draw(fb, im, &p->sheet[s], (int)(p->ms / PARTY_CELL_MS), cx + ANCHOR_DX, cy);
            sa_party_draw_bars(p, fb, cx - 22, cy - PARTY_BARS_DY, p->hp[s], sa_party_max_hp(p, s), p->mp[s], sa_party_max_mp(p, s));
        }
}

static void small_bars(Party *p, u32 *fb, const Bench *b, int x, int y) {
    if (!p->smallBarEmpty || !p->smallBarFull) return;
    int r = sa_party_unit_row(p, b->unit), v[10];
    sa_party_unit_stats(p, r, b->level > 0 ? b->level : 1, v);
    sa_blit(fb, p->smallBarEmpty, x, y);
    sa_blit_rect(fb, p->smallBarFull, 0, 0, (gauge_width(b->hp, v[0] + b->trained[0]) + 1) / 2, BARS_GAP / 2, x, y);
    sa_blit_rect(fb, p->smallBarFull, 0, BARS_GAP / 2, (gauge_width(b->mp, v[1] + b->trained[1]) + 1) / 2, p->smallBarFull->h - BARS_GAP / 2, x, y + BARS_GAP / 2);
}

void sa_party_draw_reserve(Party *p, u32 *fb, int boardX, int boardY, u32 ms, bool bars) {
    for (int t = 0; t < PARTY_RESERVE; t++) {
        int b = sa_party_reserve_at(p, t), unit = b >= 0 ? p->bench[b].unit : 0;
        if (p->reserveUnit[t] != unit) {
            sa_image_free(p->reserveSprite[t]);
            p->reserveSprite[t] = NULL;
            p->reserveUnit[t] = unit;
            if (unit && p->vfs) {
                char name[24];
                snprintf(name, sizeof name, "u%04da0.agf", sa_table_int(&p->units, sa_party_unit_row(p, unit), 11));
                SpriteSheet sheet;
                Image *full = sa_sheet_load(p->vfs, name, &sheet);
                if (full && sheet.frames > 0) p->reserveSprite[t] = half_sheet(full, &sheet, &p->reserveSheet[t]);
                sa_image_free(full);
            }
        }
        const Image *im = p->reserveSprite[t];
        if (!im) continue;
        int ax = boardX - PLATFORM_X + RESERVE_X + TILE_CX / 2 - 1 + (t * STEP_X + 1) / 2, ay = boardY - PLATFORM_Y + RESERVE_Y + TILE_CY / 2 + t * STEP_Y / 2;
        sa_sheet_draw(fb, im, &p->reserveSheet[t], (int)(ms / PARTY_CELL_MS), ax, ay);
        if (bars) small_bars(p, fb, &p->bench[sa_party_reserve_at(p, t)], ax - SMALL_BARS_DX, ay + (t * STEP_Y + 1) / 2 - t * STEP_Y / 2 - SMALL_BARS_DY);
    }
}

void sa_party_draw_diorama(Party *p, u32 *fb, int rankLimit, const Image *platform) {
    sa_party_draw_board(p, fb, platform, 0, 0, NULL);
    sa_party_draw_reserve(p, fb, PLATFORM_X, PLATFORM_Y, p->ms, false);
    char n[16];
    if (p->sgp >= 0) {
        sa_blit(fb, p->sgpLabel, 0, LABEL_Y);
        snprintf(n, sizeof n, "%d", p->sgp);
        sa_font_set_spacing(0);
        sa_text_draw(fb, SA_W, SA_H, n, -1, SGP_RIGHT - sa_text_width(n, -1, SGP_SIZE), SGP_TEXT_Y, SGP_SIZE, 0xffffff, 0x000000);
        sa_font_set_spacing(1);
        return;
    }
    sa_blit(fb, p->label, 1, LABEL_Y);
    int rank = sa_party_rank(p);
    snprintf(n, sizeof n, "%d", rank);
    int len = (int)strlen(n), row = rank > rankLimit ? 13 : 0;
    for (int i = 0; i < len; i++) sa_blit_rect(fb, p->digits, (n[i] - '0') * 10, row, 10, 13, COUNT_RIGHT - (len - i) * 10, DIGIT_Y);
    snprintf(n, sizeof n, "%d", rankLimit);
    for (int i = 0; n[i]; i++) sa_blit_rect(fb, p->digits, (n[i] - '0') * 10, 0, 10, 13, LIMIT_X + i * 10, DIGIT_Y);
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
    for (int i = 0; i < p->benchCount; i++)
        if (p->bench[i].tile >= 0 && (!key || !*key || !strcmp(sa_table_text(&p->units, sa_party_unit_row(p, p->bench[i].unit), 1), key))) n++;
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

static void fill_member(Party *p, int unit, Member *out) {
    for (int i = 0; i < PARTY_SLOTS; i++)
        if (p->unit[i] == unit) {
            *out = (Member){unit, i, -1, &p->hp[i], &p->mp[i], &p->level[i], &p->exp[i], &p->equip[i], &p->sp[i], p->trained[i]};
            return;
        }
    for (int i = 0; i < p->benchCount; i++)
        if (p->bench[i].unit == unit) {
            Bench *b = &p->bench[i];
            *out = (Member){unit, -1, i, &b->hp, &b->mp, &b->level, &b->exp, &b->equip, &b->sp, b->trained};
            return;
        }
    out->unit = 0;
}

int sa_party_member_list(Party *p, Member *out) {
    int n = 0;
    for (int r = 0; r < p->rosterCount; r++) {
        Member m;
        fill_member(p, p->roster[r], &m);
        bool dup = false;
        for (int i = 0; i < n && !dup; i++) dup = out[i].unit == m.unit;
        if (m.unit && !dup) out[n++] = m;
    }
    for (int k = 0; k < PARTY_SLOTS; k++) {
        int i = (k + PARTY_FRONT) % PARTY_SLOTS;
        bool dup = false;
        for (int j = 0; j < n && !dup; j++) dup = out[j].unit == p->unit[i];
        if (p->unit[i] && !dup) out[n++] = (Member){p->unit[i], i, -1, &p->hp[i], &p->mp[i], &p->level[i], &p->exp[i], &p->equip[i], &p->sp[i], p->trained[i]};
    }
    for (int i = 0; i < p->benchCount; i++) {
        bool dup = false;
        for (int j = 0; j < n && !dup; j++) dup = out[j].unit == p->bench[i].unit;
        Bench *b = &p->bench[i];
        if (!dup) out[n++] = (Member){b->unit, -1, i, &b->hp, &b->mp, &b->level, &b->exp, &b->equip, &b->sp, b->trained};
    }
    return n;
}

int sa_party_member_value(const Party *p, const Member *m, int index) {
    int v[10];
    sa_party_unit_stats(p, sa_party_unit_row(p, m->unit), *m->level > 0 ? *m->level : 1, v);
    return v[index] + m->trained[index];
}

const Image *sa_party_front_sprite(Party *p, int unit, const SpriteSheet **sheet) {
    int i = -1;
    for (int s = 0; s < PARTY_SLOTS && i < 0; s++)
        if (p->unit[s] == unit) i = s;
    for (int b = 0; b < p->benchCount && i < 0; b++)
        if (p->bench[b].unit == unit) i = PARTY_SLOTS + b;
    if (i < 0) return NULL;
    if (p->frontUnit[i] != unit) {
        sa_image_free(p->frontSprite[i]);
        p->frontSprite[i] = NULL;
        p->frontUnit[i] = unit;
        int r = sa_party_unit_row(p, unit);
        char name[24];
        snprintf(name, sizeof name, "u%04da2.agf", r >= 0 ? sa_table_int(&p->units, r, 11) : 0);
        if (p->vfs) p->frontSprite[i] = sa_sheet_load(p->vfs, name, &p->frontSheet[i]);
    }
    *sheet = &p->frontSheet[i];
    return p->frontSprite[i];
}

int sa_party_member_badge(const Party *p, const Member *m) {
    int post = sa_party_post_of(p, m->unit);
    if (post > 0) return post - 1;
    if (m->slot >= 0) return BADGE_FIELD;
    return p->bench[m->bench].tile >= 0 ? BADGE_RESERVE : -1;
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
    if (p->rosterCount < PARTY_ROSTER) {
        p->skillLevel[p->rosterCount] = 0;
        p->roster[p->rosterCount++] = id;
    }
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

static int ability_row(const Party *p, const char *name) {
    for (int r = 0; r < p->abilities.rows; r++)
        if (!strcmp(sa_table_text(&p->abilities, r, 2), name)) return r;
    return -1;
}

static bool ability_applies(const char *a, const char *key, int post) {
    bool town = post <= 9, floorHead = post >= 12 && post <= 18;
    static const char *const talents[] = {"農業の才", "商業の才", "工業の才", "警備の才", "教育の才", "娯楽の才", "衛生の才"};
    for (int i = 0; i < 7; i++)
        if (!strcmp(a, talents[i])) return post == 3 + i;
    return !strcmp(a, "天才") || !strcmp(a, "無能") || (!strcmp(a, "政治家") && strstr(key, "統治")) || (!strcmp(a, "大将軍") && strstr(key, "統率")) ||
           (!strcmp(a, "指揮官") && (post == 10 || post == 11)) || (!strcmp(a, "小隊長") && floorHead) || (!strcmp(a, "独立行動") && post >= 12) ||
           (!strcmp(a, "軍事向き") && !town) || (!strcmp(a, "内政向き") && town) || (!strcmp(a, "内政の才") && post >= 3 && post <= 9) ||
           (!strcmp(a, "補佐官") && (post == 2 || post == 11));
}

int sa_party_post_icon(const Party *p, int unit, int post) {
    int r = sa_party_unit_row(p, unit);
    if (r < 0 || post <= 0 || post >= p->positions.rows) return -1;
    const char *a = sa_table_text(&p->units, r, 36);
    if (!strcmp(a, "人気者")) return post <= 2 ? 2 : -1;
    if (!strcmp(a, "資金管理")) return 3;
    if (!strcmp(a, "食料管理")) return 4;
    if (!strcmp(a, "資源管理")) return 5;
    if (!ability_applies(a, sa_table_text(&p->positions, post, 2), post)) return -1;
    return !strcmp(a, "無能") ? 1 : 0;
}

int sa_party_govern_cap(int v) { return v < PARTY_GOVERN_MAX ? v : PARTY_GOVERN_MAX; }

bool sa_party_post_stats(const Party *p, int unit, int post, int *gov, int *lead) {
    int r = sa_party_unit_row(p, unit);
    *gov = r >= 0 ? sa_table_int(&p->units, r, 31) : 0;
    *lead = r >= 0 ? sa_table_int(&p->units, r, 32) : 0;
    int icon = sa_party_post_icon(p, unit, post);
    int ar = icon == 0 || icon == 1 ? ability_row(p, sa_table_text(&p->units, r, 36)) : -1;
    int v = ar >= 0 ? sa_table_int(&p->abilities, ar, 4) : 0, sign = icon == 1 ? -1 : 1;
    *gov = sa_party_govern_cap(*gov + sign * v);
    *lead = sa_party_govern_cap(*lead + sign * (v - v / 5));
    return v != 0;
}

int sa_party_post_effect(const Party *p, int unit, int post) {
    int icon = sa_party_post_icon(p, unit, post), r = sa_party_unit_row(p, unit);
    if (icon != 0 && icon != 1) return 0;
    int ar = ability_row(p, sa_table_text(&p->units, r, 36));
    if (ar < 0) return 0;
    const char *text = sa_table_text(&p->abilities, ar, 6);
    int v = sa_table_int(&p->abilities, ar, 4);
    return strstr(text, "効果を向上") ? v : strstr(text, "効果を低下") ? -v : 0;
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
    SWAP(sheet);
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
    p->sheet[slot] = (SpriteSheet){0};
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
