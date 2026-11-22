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
    RESERVE_X = -16,
    RESERVE_Y = 588,
    RESERVE_SLOTS = 5,
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
    if (!sa_table_load(&p->units, vfs, "unit.obj")) return false;
    p->platform = sa_load_image(vfs, "m996bt1a.agf");
    Image *tiles = sa_load_image(vfs, "mfloor01.agf");
    p->tile = crop(tiles, 0, 150);
    p->smallTile = half_size(tiles, 0, 150);
    sa_image_free(tiles);
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
    sa_image_free(p->platform);
    sa_image_free(p->tile);
    sa_image_free(p->smallTile);
    sa_image_free(p->gaugeFrame);
    sa_image_free(p->hpGauge);
    sa_image_free(p->mpGauge);
    sa_image_free(p->label);
    sa_image_free(p->digits);
    sa_table_free(&p->units);
    memset(p, 0, sizeof *p);
}

static void sprite_cells(const Image *im, int *cols, int *rows) {
    static const int layouts[][2] = {{11, 3}, {6, 2}, {4, 2}};
    for (size_t i = 0; i < sizeof layouts / sizeof layouts[0]; i++)
        if (im->w % layouts[i][0] == 0 && im->h % layouts[i][1] == 0) {
            *cols = layouts[i][0];
            *rows = layouts[i][1];
            return;
        }
    *cols = 1;
    *rows = 1;
}

static int filled_cells(const Image *im) {
    int cols, rows;
    sprite_cells(im, &cols, &rows);
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
    char name[24];
    snprintf(name, sizeof name, "u%04da0.agf", sa_table_int(&p->units, r, 11));
    p->sprite[slot] = sa_load_image(vfs, name);
    p->cells[slot] = p->sprite[slot] ? filled_cells(p->sprite[slot]) : 0;
}

void sa_party_new_game(Party *p, Vfs *vfs) {
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

static void slot_tile(int slot, int *x, int *y) {
    int k = slot % 3;
    bool front = slot >= PARTY_FRONT;
    *x = (front ? FRONT_X : BACK_X) + k * STEP_X;
    *y = (front ? FRONT_Y : BACK_Y) + k * STEP_Y;
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

void sa_party_draw_diorama(Party *p, u32 *fb, int rankLimit) {
    sa_blit(fb, p->platform, PLATFORM_X, PLATFORM_Y);
    for (int k = 0; k < RESERVE_SLOTS; k++)
        sa_blit(fb, p->smallTile, RESERVE_X + k * STEP_X / 2, RESERVE_Y + k * STEP_Y / 2);
    for (int s = 0; s < PARTY_SLOTS; s++) {
        int x, y;
        slot_tile(s, &x, &y);
        sa_blit(fb, p->tile, x, y);
    }
    for (int k = 0; k < 3; k++)
        for (int row = 0; row < 2; row++) {
            int s = row ? PARTY_FRONT + k : k;
            const Image *im = p->sprite[s];
            if (!p->unit[s] || !im) continue;
            int cols, rows;
            sprite_cells(im, &cols, &rows);
            int cw = im->w / cols, ch = im->h / rows;
            int cell = p->cells[s] > 0 ? (int)(p->ms / PARTY_CELL_MS % (u32)p->cells[s]) : 0;
            int tx, ty;
            slot_tile(s, &tx, &ty);
            int x = tx + TILE_CX - cw / 2, y = ty + TILE_CY + FEET - ch;
            sa_blit_rect(fb, im, cell % cols * cw, cell / cols * ch, cw, ch, x, y);
            int r = sa_party_unit_row(p, p->unit[s]);
            sa_blit(fb, p->gaugeFrame, x - 3, y - 12);
            sa_blit(fb, p->gaugeFrame, x - 3, y - 8);
            gauge(fb, p->hpGauge, p->hp[s], sa_table_int(&p->units, r, 23), x - 2, y - 11);
            gauge(fb, p->mpGauge, p->mp[s], sa_table_int(&p->units, r, 24), x - 2, y - 7);
        }
    sa_blit(fb, p->label, 1, LABEL_Y);
    char n[16];
    snprintf(n, sizeof n, "%d", sa_party_rank(p));
    int len = (int)strlen(n);
    for (int i = 0; i < len; i++) sa_blit_rect(fb, p->digits, (n[i] - '0') * 10, 0, 10, 13, COUNT_RIGHT - (len - i) * 10, DIGIT_Y);
    snprintf(n, sizeof n, "%d", rankLimit);
    for (int i = 0; n[i]; i++) sa_blit_rect(fb, p->digits, (n[i] - '0') * 10, 0, 10, 13, LIMIT_X + i * 10, DIGIT_Y);
}
