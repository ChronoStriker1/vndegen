#include "sa_private.h"

enum {
    OPT_SIZE = 30784,
    OPT_UNITS_END = 25972,
    OPT_FLOORS_END = 27172,
    OPT_ITEMS_END = 28332,
    UNIT_BYTES = 20,
    FLOOR_BYTES = 8,
    ITEM_BYTES = 4,
    MAP_HEADER = 48,
    CELL_BYTES = 104,
    CELL_FLOOR = 5,
    FLOOR_MAP = 5,
    UNIT_KEY = 1,
    LEAD_UNIT = 1,
    FILE_VERSION = 1,
};

static const char magic[4] = {'S', 'A', 'R', 'C'};

static int floor_key(int dungeon, int floor) { return dungeon << 16 | floor << 8; }

static UnitRecord *find_unit(Records *r, int id) {
    for (int i = 0; i < r->unitCount; i++)
        if (r->unit[i].id == id) return &r->unit[i];
    return NULL;
}

static UnitRecord *add_unit(Records *r, int id) {
    UnitRecord *u = find_unit(r, id);
    if (u || id <= 0 || r->unitCount >= REC_UNITS) return u;
    int at = r->unitCount;
    while (at > 0 && r->unit[at - 1].id > id) at--;
    memmove(r->unit + at + 1, r->unit + at, sizeof *r->unit * (size_t)(r->unitCount - at));
    r->unitCount++;
    memset(&r->unit[at], 0, sizeof r->unit[at]);
    r->unit[at].id = id;
    r->dirty = true;
    return &r->unit[at];
}

static int find_floor(const Records *r, int key) {
    for (int i = 0; i < r->floorCount; i++)
        if (r->floorKey[i] == key) return i;
    return -1;
}

static void set_floor(Records *r, int key, int pct) {
    int i = find_floor(r, key);
    if (i >= 0) {
        if (pct > r->floorPct[i]) {
            r->floorPct[i] = pct;
            r->dirty = true;
        }
        return;
    }
    if (r->floorCount >= REC_FLOORS) return;
    int at = r->floorCount;
    while (at > 0 && r->floorKey[at - 1] > key) at--;
    memmove(r->floorKey + at + 1, r->floorKey + at, sizeof(int) * (size_t)(r->floorCount - at));
    memmove(r->floorPct + at + 1, r->floorPct + at, sizeof(int) * (size_t)(r->floorCount - at));
    r->floorKey[at] = key;
    r->floorPct[at] = pct;
    r->floorCount++;
    r->dirty = true;
}

static void add_item(Records *r, int id) {
    if (id <= 0 || r->itemCount >= REC_ITEMS) return;
    int at = r->itemCount;
    for (int i = 0; i < r->itemCount; i++)
        if (r->item[i] == id) return;
    while (at > 0 && r->item[at - 1] > id) at--;
    memmove(r->item + at + 1, r->item + at, sizeof(int) * (size_t)(r->itemCount - at));
    r->item[at] = id;
    r->itemCount++;
    r->dirty = true;
}

static void merge_unit(Records *r, const UnitRecord *u) {
    bool was = r->dirty;
    UnitRecord *m = add_unit(r, u->id);
    if (!m) return;
    if (u->level > m->level) m->level = u->level;
    if (u->kills > m->kills) m->kills = u->kills;
    for (int k = 0; k < 2; k++)
        if (!m->extra[k]) m->extra[k] = u->extra[k];
    r->dirty = was;
}

static char *opt_path(const SA *s) { return sa_fmt("%s/Bunny3.sav/Bunny3.opt", s->saveDir); }

static char *own_path(const SA *s) { return sa_fmt("%s/vndegen/records.dat", s->saveDir); }

static u8 *read_opt(const SA *s) {
    char *path = opt_path(s);
    size_t n, size = 0;
    u8 *file = sa_read_file(path, &n);
    free(path);
    u8 *img = file ? vnsystemaoi_unpack(file, n, &size) : NULL;
    free(file);
    if (img && size != OPT_SIZE) {
        free(img);
        img = NULL;
    }
    return img;
}

static int count_back(const u8 *img, int end, int bytes, int limit) {
    int n = 0;
    while (n < limit && end - (n + 1) * bytes >= 0 && rd32(img + end - (n + 1) * bytes)) n++;
    return n;
}

static void read_image(Records *r, const u8 *img) {
    bool was = r->dirty;
    int n = count_back(img, OPT_UNITS_END, UNIT_BYTES, REC_UNITS);
    for (int i = n - 1; i >= 0; i--) {
        const u8 *p = img + OPT_UNITS_END - (i + 1) * UNIT_BYTES;
        UnitRecord u = {(int)rd32(p), (int)rd32(p + 4), (int)rd32(p + 8), {(int)rd32(p + 12), (int)rd32(p + 16)}};
        merge_unit(r, &u);
    }
    n = count_back(img, OPT_FLOORS_END, FLOOR_BYTES, REC_FLOORS);
    for (int i = n - 1; i >= 0; i--) {
        const u8 *p = img + OPT_FLOORS_END - (i + 1) * FLOOR_BYTES;
        set_floor(r, (int)rd32(p), (int)rd32(p + 4));
    }
    n = count_back(img, OPT_ITEMS_END, ITEM_BYTES, REC_ITEMS);
    for (int i = n - 1; i >= 0; i--) add_item(r, (int)rd32(img + OPT_ITEMS_END - (i + 1) * ITEM_BYTES));
    r->dirty = was;
}

static void write_image(const Records *r, u8 *img) {
    int old = count_back(img, OPT_UNITS_END, UNIT_BYTES, REC_UNITS);
    int keep = old > r->unitCount ? old : r->unitCount;
    memset(img + OPT_UNITS_END - keep * UNIT_BYTES, 0, (size_t)keep * UNIT_BYTES);
    for (int i = 0; i < r->unitCount; i++) {
        u8 *p = img + OPT_UNITS_END - (r->unitCount - i) * UNIT_BYTES;
        const UnitRecord *u = &r->unit[i];
        wr32(p, (u32)u->id);
        wr32(p + 4, (u32)u->level);
        wr32(p + 8, (u32)u->kills);
        wr32(p + 12, (u32)u->extra[0]);
        wr32(p + 16, (u32)u->extra[1]);
    }
    old = count_back(img, OPT_FLOORS_END, FLOOR_BYTES, REC_FLOORS);
    keep = old > r->floorCount ? old : r->floorCount;
    memset(img + OPT_FLOORS_END - keep * FLOOR_BYTES, 0, (size_t)keep * FLOOR_BYTES);
    for (int i = 0; i < r->floorCount; i++) {
        u8 *p = img + OPT_FLOORS_END - (r->floorCount - i) * FLOOR_BYTES;
        wr32(p, (u32)r->floorKey[i]);
        wr32(p + 4, (u32)r->floorPct[i]);
    }
    old = count_back(img, OPT_ITEMS_END, ITEM_BYTES, REC_ITEMS);
    keep = old > r->itemCount ? old : r->itemCount;
    memset(img + OPT_ITEMS_END - keep * ITEM_BYTES, 0, (size_t)keep * ITEM_BYTES);
    for (int i = 0; i < r->itemCount; i++) wr32(img + OPT_ITEMS_END - (r->itemCount - i) * ITEM_BYTES, (u32)r->item[i]);
}

static void read_own(Records *r, const SA *s) {
    char *path = own_path(s);
    size_t n;
    u8 *d = sa_read_file(path, &n);
    free(path);
    if (!d || n < 20 || memcmp(d, magic, 4) || rd32(d + 4) != FILE_VERSION) {
        free(d);
        return;
    }
    int units = (int)rd32(d + 8), floors = (int)rd32(d + 12), items = (int)rd32(d + 16);
    size_t need = 20 + (size_t)units * UNIT_BYTES + (size_t)floors * FLOOR_BYTES + (size_t)items * ITEM_BYTES;
    if (units < 0 || floors < 0 || items < 0 || units > REC_UNITS || floors > REC_FLOORS || items > REC_ITEMS || n < need) {
        free(d);
        return;
    }
    bool was = r->dirty;
    const u8 *p = d + 20;
    for (int i = 0; i < units; i++, p += UNIT_BYTES) {
        UnitRecord u = {(int)rd32(p), (int)rd32(p + 4), (int)rd32(p + 8), {(int)rd32(p + 12), (int)rd32(p + 16)}};
        merge_unit(r, &u);
    }
    for (int i = 0; i < floors; i++, p += FLOOR_BYTES) set_floor(r, (int)rd32(p), (int)rd32(p + 4));
    for (int i = 0; i < items; i++, p += ITEM_BYTES) add_item(r, (int)rd32(p));
    r->dirty = was;
    free(d);
}

static void write_own(const Records *r, const SA *s) {
    size_t n = 20 + (size_t)r->unitCount * UNIT_BYTES + (size_t)r->floorCount * FLOOR_BYTES + (size_t)r->itemCount * ITEM_BYTES;
    u8 *d = sa_alloc(n), *p = d + 20;
    memcpy(d, magic, 4);
    wr32(d + 4, FILE_VERSION);
    wr32(d + 8, (u32)r->unitCount);
    wr32(d + 12, (u32)r->floorCount);
    wr32(d + 16, (u32)r->itemCount);
    for (int i = 0; i < r->unitCount; i++, p += UNIT_BYTES) {
        const UnitRecord *u = &r->unit[i];
        wr32(p, (u32)u->id);
        wr32(p + 4, (u32)u->level);
        wr32(p + 8, (u32)u->kills);
        wr32(p + 12, (u32)u->extra[0]);
        wr32(p + 16, (u32)u->extra[1]);
    }
    for (int i = 0; i < r->floorCount; i++, p += FLOOR_BYTES) {
        wr32(p, (u32)r->floorKey[i]);
        wr32(p + 4, (u32)r->floorPct[i]);
    }
    for (int i = 0; i < r->itemCount; i++, p += ITEM_BYTES) wr32(p, (u32)r->item[i]);
    char *dir = sa_fmt("%s/vndegen", s->saveDir), *path = own_path(s);
    sa_mkdirs(dir);
    sa_write_file(path, d, n);
    free(dir);
    free(path);
    free(d);
}

void sa_records_load(SA *s) {
    Records *r = &s->extra.records;
    memset(r, 0, sizeof *r);
    for (int d = 0; d < EXP_DUNGEONS; d++)
        for (int f = 0; f < EXP_FLOORS; f++) r->floorCells[d][f] = -1;
    read_own(r, s);
    u8 *img = read_opt(s);
    if (img) read_image(r, img);
    free(img);
}

int sa_records_floor_cells(SA *s, int dungeon, int floor) {
    Records *r = &s->extra.records;
    if (dungeon <= 0 || dungeon >= EXP_DUNGEONS || floor <= 0 || floor >= EXP_FLOORS) return 0;
    if (r->floorCells[dungeon][floor] >= 0) return r->floorCells[dungeon][floor];
    r->floorCells[dungeon][floor] = 0;
    const Table *t = &s->exp.floors;
    for (int row = 0; row < t->rows; row++) {
        if (sa_table_int(t, row, 0) != dungeon || sa_table_int(t, row, 1) != floor) continue;
        size_t n;
        u8 *m = sa_vfs_read(&s->vfs, sa_table_text(t, row, FLOOR_MAP), &n);
        int w = m && n >= MAP_HEADER ? (int)rd32(m + 4) : 0, h = m && n >= MAP_HEADER ? (int)rd32(m + 8) : 0, cells = 0;
        if (w > 0 && h > 0 && n >= MAP_HEADER + (size_t)(w * h) * CELL_BYTES)
            for (int c = 0; c < w * h; c++) cells += (int)rd32(m + MAP_HEADER + (size_t)c * CELL_BYTES + CELL_FLOOR * 4) != 0xffffffffu;
        free(m);
        r->floorCells[dungeon][floor] = cells;
        break;
    }
    return r->floorCells[dungeon][floor];
}

void sa_records_sync(SA *s) {
    Records *r = &s->extra.records;
    if (!s->inGame) return;
    UnitRecord *lead = add_unit(r, LEAD_UNIT);
    if (lead && lead->level < 1) {
        lead->level = 1;
        r->dirty = true;
    }
    Party *p = &s->party;
    for (int i = 0; i < PARTY_SLOTS; i++) {
        UnitRecord *u = p->unit[i] ? find_unit(r, p->unit[i]) : NULL;
        if (u && p->level[i] > u->level) {
            u->level = p->level[i];
            r->dirty = true;
        }
    }
    for (int i = 0; i < p->benchCount; i++) {
        UnitRecord *u = find_unit(r, p->bench[i].unit);
        if (u && p->bench[i].level > u->level) {
            u->level = p->bench[i].level;
            r->dirty = true;
        }
    }
    if (!s->exp.explored) return;
    for (int d = 1; d < EXP_DUNGEONS; d++)
        for (int f = 1; f < EXP_FLOORS; f++) {
            int seen = 0;
            for (int c = 0; c < DUN_CELLS; c++) seen += s->exp.explored[d][f][c];
            if (!seen) continue;
            int cells = sa_records_floor_cells(s, d, f);
            int pct = cells ? seen * 100 / cells : 0;
            set_floor(r, floor_key(d, f), pct > 100 ? 100 : pct);
        }
}

void sa_records_flush(SA *s) {
    Records *r = &s->extra.records;
    sa_records_sync(s);
    if (!r->dirty) return;
    u8 *img = read_opt(s);
    if (img) {
        read_image(r, img);
        write_image(r, img);
        size_t n;
        u8 *packed = vnsystemaoi_pack(img, OPT_SIZE, &n);
        char *path = opt_path(s);
        if (packed) sa_write_file(path, packed, n);
        free(path);
        free(packed);
        free(img);
    }
    write_own(r, s);
    r->dirty = false;
}

void sa_records_join(SA *s, const char *key) {
    const Table *u = &s->party.units;
    for (int i = 0; i < u->rows; i++) {
        if (strcmp(sa_table_text(u, i, UNIT_KEY), key)) continue;
        UnitRecord *rec = add_unit(&s->extra.records, sa_table_int(u, i, 0));
        if (rec && rec->level < 1) {
            rec->level = 1;
            s->extra.records.dirty = true;
        }
        return;
    }
}

void sa_records_item(SA *s, int id) { add_item(&s->extra.records, id); }

void sa_records_battle(SA *s) {
    Records *r = &s->extra.records;
    const Battle *b = &s->battle;
    u32 mark[3] = {b->rng, (u32)b->count, (u32)b->round};
    if (!memcmp(mark, r->battleMark, sizeof mark)) return;
    memcpy(r->battleMark, mark, sizeof mark);
    for (int i = 0; i < b->count; i++) {
        const Fighter *f = &b->f[i];
        if (!f->enemy || f->hp > 0) continue;
        UnitRecord *u = add_unit(r, f->id);
        if (u) {
            u->kills++;
            r->dirty = true;
        }
    }
    if (b->result != BATTLE_WON || !b->gainItem[0]) return;
    for (int i = 0; i < b->items.rows; i++)
        if (!strcmp(sa_table_text(&b->items, i, 1), b->gainItem)) {
            add_item(r, sa_table_int(&b->items, i, 0));
            return;
        }
}

const UnitRecord *sa_records_unit(const SA *s, int id) { return find_unit((Records *)&s->extra.records, id); }

int sa_records_floor(const SA *s, int dungeon, int floor) {
    const Records *r = &s->extra.records;
    int i = find_floor(r, floor_key(dungeon, floor));
    return i < 0 ? -1 : r->floorPct[i];
}

bool sa_records_has_item(const SA *s, int id) {
    const Records *r = &s->extra.records;
    for (int i = 0; i < r->itemCount; i++)
        if (r->item[i] == id) return true;
    return false;
}
