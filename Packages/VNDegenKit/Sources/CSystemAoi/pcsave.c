#include "sa_private.h"

#define FIXED_SKILL "固定／"

enum {
    IMAGE_SIZE = 1128076,
    VM_AT = 1119076,
    PC_C_VARS = 1000,
    PC_F_VARS = 5000,
    TOWN_AT = 1010852,
    CELL_SIZE = 20,
    STOCK_AT = 1004728,
    STOCK_ENTRIES = 256,
    MEMBER_AT = 86008,
    MEMBER_SIZE = 1296,
    MEMBERS_MAX = 64,
    FORM_AT = 998000,
    FORM_COUNT = 7,
    FORM_TOP = 9,
    FORM_BOTTOM = 14,
    FORM_RESERVE = 23,
    RESERVE_TILES = 5,
    MAPS_AT = 6172,
    MAP_RECORD = 532,
    MAP_HEAD = 16,
    MAP_LAYERS = 20,
    MAP_LAYER = 128,
    MAP_DUNGEONS = 15,
    MAP_FLOORS = 10,
    MAP_SIDE = 15,
    MEMBER_LEVEL = 12,
    MEMBER_EXP = 16,
    MEMBER_HP = 28,
    MEMBER_MP = 32,
    MEMBER_SP = 80,
    MEMBER_TRAINED = 84,
    MEMBER_EQUIP = 1212,
    MEMBER_BYTES = 24,
    MEMBER_STATS = 124,
    MEMBER_COPY = 544,
    BLOCK_CRITICAL = 176,
    BLOCK_AILMENT = 184,
    BLOCK_ABILITIES = 188,
    BLOCK_SLOTS = 456,
    MEMBER_SKILLS = 1220,
    MEMBER_BONUS = 10,
    GRID_AT = 1010848,
    GRID_AFTER = 1012648,
    SCENE_AT = 993208,
    SCENE_STEP = 1200,
    FREE_AT = 1008208,
    FREE_COUNT = 30,
    PLACE_VAR = 4,
    PLACE_TOWN = 6,
    SEED_VAR = 6,
    DUNGEON_VAR = 70,
};

static int at(const u8 *img, size_t off) { return (int)rd32(img + off); }

static void put(u8 *img, size_t off, int v) { wr32(img + off, (u32)v); }

static Bench member_at(const u8 *img, int k) {
    size_t m = MEMBER_AT + (size_t)k * MEMBER_SIZE;
    Bench b = {0};
    b.unit = at(img, m);
    b.level = at(img, m + MEMBER_LEVEL);
    b.exp = at(img, m + MEMBER_EXP);
    b.hp = at(img, m + MEMBER_HP);
    b.mp = at(img, m + MEMBER_MP);
    b.sp = at(img, m + MEMBER_SP);
    for (int i = 0; i < 10; i++) b.trained[i] = at(img, m + MEMBER_TRAINED + (size_t)i * 4);
    b.equip = at(img, m + MEMBER_EQUIP);
    return b;
}

static int form_slot(int cell) {
    if (cell >= FORM_TOP && cell < FORM_TOP + 3) return PARTY_FRONT + cell - FORM_TOP;
    if (cell >= FORM_BOTTOM && cell < FORM_BOTTOM + 3) return cell - FORM_BOTTOM;
    return -1;
}

static void put_slot(SA *s, int slot, const Bench *b) {
    Party *p = &s->party;
    sa_party_place(p, &s->vfs, slot, b->unit);
    p->level[slot] = b->level;
    p->exp[slot] = b->exp;
    p->hp[slot] = b->hp;
    p->mp[slot] = b->mp;
    p->sp[slot] = b->sp;
    memcpy(p->trained[slot], b->trained, sizeof b->trained);
    p->equip[slot] = b->equip;
}

static size_t map_record(int dungeon, int floor) { return MAPS_AT + (size_t)((dungeon - 1) * MAP_FLOORS + floor - 1) * MAP_RECORD; }

static void read_maps(SA *s, const u8 *img) {
    if (!s->exp.explored) return;
    for (int d = 1; d < EXP_DUNGEONS && d <= MAP_DUNGEONS; d++)
        for (int f = 1; f < EXP_FLOORS && f <= MAP_FLOORS; f++) {
            for (int i = 0; i < 4; i++) s->exp.floorInfo[d][f][i] = at(img, map_record(d, f) + (size_t)i * 4);
            size_t o = map_record(d, f) + MAP_LAYERS;
            for (int x = 0; x < MAP_SIDE; x++) {
                size_t k = (size_t)(MAP_SIDE - 1 - x) * 4;
                u32 bits = rd32(img + o + k), down = rd32(img + o + MAP_LAYER + k), west = rd32(img + o + 2 * MAP_LAYER + k);
                for (int y = 0; y < MAP_SIDE; y++) {
                    int c = y * MAP_SIDE + x;
                    s->exp.explored[d][f][c] = bits >> y & 1;
                    s->exp.passages[d][f][c] = (u8)((down >> y & 1 ? PASS_DOWN : 0) | (west >> y & 1 ? PASS_WEST : 0));
                }
            }
        }
}

static void write_maps(SA *s, u8 *img) {
    if (!s->exp.explored) return;
    for (int d = 1; d < EXP_DUNGEONS && d <= MAP_DUNGEONS; d++)
        for (int f = 1; f < EXP_FLOORS && f <= MAP_FLOORS; f++) {
            const bool *v = s->exp.explored[d][f];
            size_t o = map_record(d, f);
            for (int i = 0; i < 4; i++) put(img, o + (size_t)i * 4, s->exp.floorInfo[d][f][i]);
            bool any = false;
            for (int c = 0; c < DUN_CELLS && !any; c++) any = v[c];
            if (!any) continue;
            put(img, o + MAP_HEAD, d << 16 | f << 8);
            for (int x = 0; x < MAP_SIDE; x++)
                for (int y = 0; y < MAP_SIDE; y++) {
                    int c = y * MAP_SIDE + x;
                    u8 pass = s->exp.passages[d][f][c];
                    bool layer[4] = {v[c], pass & PASS_DOWN, pass & PASS_WEST, v[c]};
                    for (int l = 0; l < 4; l++) {
                        if (!layer[l]) continue;
                        size_t w = o + MAP_LAYERS + (size_t)l * MAP_LAYER + (size_t)(MAP_SIDE - 1 - x) * 4;
                        wr32(img + w, rd32(img + w) | 1u << y);
                    }
                }
        }
}

static void read_members(SA *s, const u8 *img) {
    Party *p = &s->party;
    for (int i = 0; i < PARTY_SLOTS; i++) sa_party_place(p, &s->vfs, i, 0);
    p->benchCount = p->rosterCount = 0;
    int count = 0;
    while (count < MEMBERS_MAX && count < PARTY_ROSTER && at(img, MEMBER_AT + (size_t)count * MEMBER_SIZE)) count++;
    bool done[MEMBERS_MAX] = {0};
    for (int k = 0; k < count; k++) p->roster[p->rosterCount++] = at(img, MEMBER_AT + (size_t)k * MEMBER_SIZE);
    for (int cell = 0; cell < FORM_RESERVE + RESERVE_TILES; cell++) {
        int k = at(img, FORM_AT + (size_t)cell * 4), slot = form_slot(cell);
        if (k < 0 || k >= count || done[k] || (slot < 0 && cell < FORM_RESERVE)) continue;
        Bench b = member_at(img, k);
        if (slot >= 0) put_slot(s, slot, &b);
        else if (p->benchCount < PARTY_BENCH) p->bench[p->benchCount++] = b;
        done[k] = true;
    }
    for (int k = 0; k < count; k++)
        if (!done[k] && p->benchCount < PARTY_BENCH) p->bench[p->benchCount++] = member_at(img, k);
    if (!sa_party_members(p) && p->benchCount) {
        Bench b = p->bench[0];
        memmove(p->bench, p->bench + 1, sizeof(Bench) * (size_t)--p->benchCount);
        put_slot(s, PARTY_FRONT + 1, &b);
    }
}

bool sa_pcsave_read(SA *s, const char *path) {
    size_t n, size;
    u8 *file = sa_read_file(path, &n);
    u8 *img = file ? vnsystemaoi_unpack(file, n, &size) : NULL;
    free(file);
    if (!img || size != IMAGE_SIZE) {
        sa_log(s, "pc save %s: %s", path, img ? "unexpected size" : "unreadable");
        free(img);
        return false;
    }
    memset(s->vm.c, 0, sizeof s->vm.c);
    memset(s->vm.f, 0, sizeof s->vm.f);
    for (int i = 0; i < PC_C_VARS; i++) s->vm.c[i] = at(img, VM_AT + (size_t)i * 4);
    memcpy(s->vm.f, img + VM_AT + PC_C_VARS * 4, PC_F_VARS);
    for (int c = 0; c < TOWN_CELLS; c++) {
        size_t o = TOWN_AT + (size_t)c * CELL_SIZE;
        Facility *f = &s->town.cell[c];
        f->type = at(img, o);
        f->level = f->type ? at(img, o + 8) : 0;
        f->exp = f->type ? at(img, o + 12) : 0;
    }
    s->town.stage = s->vm.c[C_TOWN_STAGE];
    read_members(s, img);
    read_maps(s, img);
    s->exp.active = s->vm.c[DUNGEON_VAR] > 0 && s->vm.c[DUNGEON_VAR + 1] > 0;
    s->exp.dungeon = s->vm.c[DUNGEON_VAR];
    s->exp.floor = s->vm.c[DUNGEON_VAR + 1];
    Party *p = &s->party;
    p->items = 0;
    for (int i = 0; i < STOCK_ENTRIES; i++) {
        size_t o = STOCK_AT + (size_t)i * 12;
        int id = at(img, o), count = at(img, o + 4) + at(img, o + 8);
        if (!id) break;
        if (count > 0) sa_party_add_item(p, id, count);
    }
    free(img);
    sa_log(s, "pc save %s: turn %d, read", path, s->vm.c[C_TURN]);
    return true;
}

static const struct {
    int off, value;
} sceneTail[] = {{998008, 1}, {998284, -1}};

static const struct {
    int off, value;
} engineVars[] = {
    {0, 0x7fffffff}, {7, 100}, {8, 100}, {36, 1}, {37, 1}, {190, -1}, {191, -1}, {192, -1}, {193, -1}, {194, -1},
};

static int skill_id(SA *s, const char *name) {
    const Table *k = &s->battle.skills;
    for (int i = 0; i < k->rows; i++)
        if (!strcmp(sa_table_text(k, i, 1), name)) return sa_table_int(k, i, 0);
    return 0;
}

static int ability_id(SA *s, const char *name) {
    const Table *a = &s->party.abilities;
    for (int i = 0; i < a->rows; i++)
        if (!strcmp(sa_table_text(a, i, 1), name)) return sa_table_int(a, i, 0);
    return 0;
}

static int resist(const char *ability, const char *key) {
    const char *p = strstr(ability, key);
    return p ? atoi(p + strlen(key)) : 0;
}

static int item_bonus(SA *s, int id, const char *key) {
    const Table *items = &s->equip.items;
    for (int i = 0; i < items->rows; i++) {
        if (sa_table_int(items, i, 0) != id) continue;
        int bonus = 0;
        for (int col = 15; col <= 22; col++) bonus += resist(sa_table_text(items, i, col), key);
        return bonus;
    }
    return 0;
}

static void write_member(SA *s, u8 *img, int k, const Bench *b, bool placed) {
    const Party *p = &s->party;
    const Table *u = &p->units;
    int r = sa_party_unit_row(p, b->unit);
    size_t m = MEMBER_AT + (size_t)k * MEMBER_SIZE;
    put(img, m, b->unit);
    put(img, m + 4, k);
    put(img, m + 8, k);
    put(img, m + MEMBER_LEVEL, b->level);
    put(img, m + MEMBER_EXP, b->exp);
    put(img, m + MEMBER_BYTES, (k ? k : 2) << 16 | (placed ? 1 : 4) << 8);
    put(img, m + MEMBER_HP, b->hp);
    put(img, m + MEMBER_MP, b->mp);
    put(img, m + MEMBER_SP, b->sp);
    for (int i = 0; i < 10; i++) put(img, m + MEMBER_TRAINED + (size_t)i * 4, b->trained[i]);
    static const char *const bonusKeys[] = {"斬補正", "打補正", "射補正", "魔補正"};
    for (int copy = 0; copy < 2; copy++) {
        size_t c = m + MEMBER_STATS + (size_t)copy * MEMBER_COPY;
        for (int i = 0; i < 10; i++) put(img, c + (size_t)i * 4, sa_table_int(u, r, 23 + i) + b->trained[i]);
        if (copy && b->equip)
            for (int i = 0; i < 4; i++) put(img, c + (size_t)(MEMBER_BONUS + i) * 4, item_bonus(s, b->equip, bonusKeys[i]));
        put(img, c + BLOCK_CRITICAL, resist(sa_table_text(u, r, 33), "必殺耐性+"));
        put(img, c + BLOCK_AILMENT, resist(sa_table_text(u, r, 33), "異常耐性+"));
        for (int i = 0; i < 3; i++) put(img, c + BLOCK_ABILITIES + (size_t)i * 4, ability_id(s, sa_table_text(u, r, 34 + i)));
        put(img, c + BLOCK_SLOTS, sa_table_int(u, r, 38));
    }
    if (b->equip) {
        put(img, m + MEMBER_EQUIP, b->equip);
        put(img, m + MEMBER_EQUIP + 4, -1);
    }
    size_t q = m + MEMBER_SKILLS;
    int ids[8], known = 0;
    for (int i = 0; i < 8; i++) {
        const char *name = sa_table_text(u, r, 41 + i * 3);
        int id = *name && strcmp(name, "*") ? skill_id(s, name) : 0;
        if (id) ids[known++] = id;
    }
    bool fixed = known && !strncmp(sa_table_text(u, r, 41), FIXED_SKILL, strlen(FIXED_SKILL));
    for (int i = 0; i < (fixed ? 1 : known); i++, q += 8) {
        put(img, q, ids[i]);
        put(img, q + 4, fixed ? known - 1 : 1);
    }
}

static void write_members(SA *s, u8 *img) {
    const Party *p = &s->party;
    for (int cell = 8; cell < FORM_RESERVE + RESERVE_TILES + 5; cell++)
        put(img, FORM_AT + (size_t)cell * 4, form_slot(cell) >= 0 || (cell >= FORM_RESERVE && cell < FORM_RESERVE + RESERVE_TILES) ? -1 : -2);
    int k = 0;
    for (int i = 0; i < p->rosterCount && k < MEMBERS_MAX; i++) {
        int id = p->roster[i];
        Bench b = {0};
        int cell = -1;
        for (int sl = 0; sl < PARTY_SLOTS && cell < 0; sl++) {
            if (p->unit[sl] != id) continue;
            b.unit = id, b.hp = p->hp[sl], b.mp = p->mp[sl], b.level = p->level[sl], b.exp = p->exp[sl], b.equip = p->equip[sl], b.sp = p->sp[sl];
            memcpy(b.trained, p->trained[sl], sizeof b.trained);
            cell = sl >= PARTY_FRONT ? FORM_TOP + sl - PARTY_FRONT : FORM_BOTTOM + sl;
        }
        for (int j = 0; j < p->benchCount && !b.unit; j++)
            if (p->bench[j].unit == id) b = p->bench[j];
        if (!b.unit) continue;
        write_member(s, img, k, &b, cell >= 0);
        if (cell >= 0) put(img, FORM_AT + (size_t)cell * 4, k);
        k++;
    }
    bool more = k > 1;
    put(img, FORM_AT + 5 * 4, more ? 1 : -1);
    put(img, FORM_AT + 6 * 4, more ? 0 : -1);
    put(img, FORM_AT + FORM_COUNT * 4, more ? k : 0);
}

bool sa_pcsave_write(SA *s, const char *path) {
    u8 *img = sa_alloc(IMAGE_SIZE);
    memset(img, 0, IMAGE_SIZE);
    for (int k = 0; k < 4; k++) {
        size_t o = SCENE_AT + (size_t)k * SCENE_STEP;
        put(img, o, -1);
        put(img, o + 4, 1);
        put(img, o + 8, -1);
    }
    for (size_t i = 0; i < sizeof sceneTail / sizeof sceneTail[0]; i++) put(img, (size_t)sceneTail[i].off, sceneTail[i].value);
    for (int i = 0; i < FREE_COUNT; i++) put(img, FREE_AT + (size_t)i * 4, -1);
    const Party *p = &s->party;
    for (int i = 0, n = 0; i < p->items && n < STOCK_ENTRIES; i++) {
        int used = 0;
        for (int sl = 0; sl < PARTY_SLOTS; sl++)
            if (p->unit[sl] && p->equip[sl] == p->itemId[i]) used++;
        size_t o = STOCK_AT + (size_t)n++ * 12;
        put(img, o, p->itemId[i]);
        put(img, o + 4, p->itemCount[i] - used);
        put(img, o + 8, used);
    }
    const Town *t = &s->town;
    for (int c = 0; c < TOWN_CELLS; c++) {
        size_t o = GRID_AT + (size_t)c * CELL_SIZE;
        const Facility *f = &t->cell[c];
        bool open = c < t->mass.rows && sa_table_int(&t->mass, c, 1) <= t->stage;
        put(img, o, open || f->type ? 0 : -1);
        put(img, o + 4, f->type);
        put(img, o + 8, f->type ? 0 : -1);
        put(img, o + 12, f->type ? f->level : 0);
        put(img, o + 16, f->type ? f->exp : 0);
    }
    put(img, GRID_AFTER, 1);
    write_members(s, img);
    write_maps(s, img);
    for (int i = 0; i < PC_C_VARS; i++) put(img, VM_AT + (size_t)i * 4, s->vm.c[i]);
    memcpy(img + VM_AT + PC_C_VARS * 4, s->vm.f, PC_F_VARS);
    for (size_t i = 0; i < sizeof engineVars / sizeof engineVars[0]; i++) put(img, VM_AT + (size_t)engineVars[i].off * 4, engineVars[i].value);
    put(img, VM_AT + PLACE_VAR * 4, PLACE_TOWN);
    put(img, VM_AT + (PLACE_VAR + 1) * 4, 1);
    if (!s->vm.c[SEED_VAR]) put(img, VM_AT + SEED_VAR * 4, (int)(sa_vm_random(&s->vm) & 0x7fffffff));
    size_t n;
    u8 *packed = vnsystemaoi_pack(img, IMAGE_SIZE, &n);
    bool ok = packed && sa_write_file(path, packed, n);
    sa_log(s, "pc save %s: %s", path, ok ? "written" : "failed");
    free(packed);
    free(img);
    return ok;
}

bool sa_pcsave_summary(SA *s, const char *path, SaveSummary *out) {
    memset(out, 0, sizeof *out);
    size_t n, size;
    u8 *file = sa_read_file(path, &n);
    u8 *img = file ? vnsystemaoi_unpack(file, n, &size) : NULL;
    free(file);
    if (!img || size != IMAGE_SIZE) {
        free(img);
        return false;
    }
    const u8 *c = img + VM_AT;
    out->used = true;
    out->time = rd32(c + 2 * 4);
    out->count = (int)rd32(c + 3 * 4);
    out->play = (int)rd32(c + 11 * 4);
    out->turn = (int)rd32(c + C_TURN * 4);
    for (int i = 0; i < 4; i++) out->town[i] = (int)rd32(c + (C_RESIDENTS + i) * 4);
    out->level = at(img, MEMBER_AT + MEMBER_LEVEL);
    const char *dungeon = NULL, *floor = NULL;
    if ((int)rd32(c + DUNGEON_VAR * 4) > 0) sa_exp_names(s, (int)rd32(c + DUNGEON_VAR * 4), (int)rd32(c + (DUNGEON_VAR + 1) * 4), &dungeon, &floor);
    snprintf(out->place[0], sizeof out->place[0], "%s", dungeon ? dungeon : "街");
    snprintf(out->place[1], sizeof out->place[1], "%s", floor ? floor : "");
    int weapon = at(img, MEMBER_AT + MEMBER_EQUIP);
    for (int i = 0; weapon && i < s->equip.items.rows; i++)
        if (sa_table_int(&s->equip.items, i, 0) == weapon) snprintf(out->weapon, sizeof out->weapon, "%s", sa_table_text(&s->equip.items, i, 1));
    free(img);
    return true;
}
