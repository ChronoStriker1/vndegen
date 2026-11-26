#include "sa_private.h"

enum {
    CELL_OBJECT = 6,
    OBJECT_CRYSTAL = 60,
    OBJECT_OPENED = 61,
    FLOOR_CHESTS = 36,
    CHEST_ENTRIES = 8,
    ITEM_NAME = 1,
    ITEM_RANK = 29,
    ITEM_TAGS = 30,
    ITEM_WEIGHT = 31,
    CRYSTAL_INFO = 1,
    CRYSTAL_OPENED = 0x81,
    CRYSTAL_SLOTS = 8,
};

static bool tagged(const char *list, const char *tag) {
    size_t n = strlen(tag);
    for (const char *p = list; p; p = strchr(p, '|') ? strchr(p, '|') + 1 : NULL)
        if (!strncmp(p, tag, n) && (p[n] == '|' || !p[n])) return true;
    return false;
}

static bool in_chest(const Table *items, int r, const char *table, int lo, int hi) {
    int rank = sa_table_int(items, r, ITEM_RANK);
    return rank >= lo && rank <= hi && tagged(sa_table_text(items, r, ITEM_TAGS), table);
}

static int pick_item(const Table *items, const char *table, int lo, int hi, u32 roll) {
    int total = 0;
    for (int r = 0; r < items->rows; r++)
        if (in_chest(items, r, table, lo, hi)) total += sa_table_int(items, r, ITEM_WEIGHT);
    if (total <= 0) return -1;
    int left = (int)(roll % (u32)total);
    for (int r = 0; r < items->rows; r++) {
        if (!in_chest(items, r, table, lo, hi)) continue;
        left -= sa_table_int(items, r, ITEM_WEIGHT);
        if (left < 0) return r;
    }
    return -1;
}

static int *crystal_word(SA *s, int k, int *shift) {
    Expedition *e = &s->exp;
    if (!e->floorInfo || k < 0 || k >= CRYSTAL_SLOTS || e->dungeon < 0 || e->dungeon >= EXP_DUNGEONS || e->floor < 0 ||
        e->floor >= EXP_FLOORS)
        return NULL;
    *shift = 8 * (k % 4);
    return &e->floorInfo[e->dungeon][e->floor][CRYSTAL_INFO + k / 4];
}

void sa_actions_enter(SA *s) {
    Dungeon *d = &s->dungeon;
    for (int k = 0; k < d->crystalCount; k++) {
        int shift, *w = crystal_word(s, k, &shift);
        if (!w || ((u32)*w >> shift & 0xff) != CRYSTAL_OPENED) continue;
        i32 *obj = &d->cell[d->crystal[k][1] * DUN_SIDE + d->crystal[k][0]][CELL_OBJECT];
        if (*obj == OBJECT_CRYSTAL) *obj = OBJECT_OPENED;
    }
}

void sa_actions_open(SA *s) {
    Dungeon *d = &s->dungeon;
    int k = sa_dungeon_crystal_index(d);
    int col = FLOOR_CHESTS + (k < CHEST_ENTRIES ? k : CHEST_ENTRIES - 1) * 4;
    const Table *floors = &s->exp.floors, *items = &s->battle.items;
    const char *table = sa_table_text(floors, s->exp.row, col);
    int lo = sa_table_int(floors, s->exp.row, col + 1), hi = sa_table_int(floors, s->exp.row, col + 2);
    int r = pick_item(items, table, lo, hi, sa_vm_random(&s->vm));
    d->cell[d->y * DUN_SIDE + d->x][CELL_OBJECT] = OBJECT_OPENED;
    int shift, *w = crystal_word(s, k, &shift);
    if (w) *w = (int)(((u32)*w & ~(0xffu << shift)) | (u32)CRYSTAL_OPENED << shift);
    if (r < 0) {
        sa_log(s, "crystal %d (%s %d-%d): nothing", k, table, lo, hi);
        return;
    }
    int id = sa_table_int(items, r, 0);
    sa_party_add_item(&s->party, id, 1);
    sa_records_item(s, id);
    char msg[160];
    snprintf(msg, sizeof msg, "%sを|手に入れた", sa_table_text(items, r, ITEM_NAME));
    sa_exp_notice(s, msg);
    sa_log(s, "crystal %d (%s %d-%d): %s", k, table, lo, hi, sa_table_text(items, r, ITEM_NAME));
}
