#include "sa_private.h"

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
    MEMBER_RESIST = 300,
    MEMBER_SLOTS = 580,
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
    PLACE_DUNGEON = 62,
    SEED_VAR = 6,
};

static int at(const u8 *img, size_t off) { return (int)rd32(img + off); }

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
    Party *p = &s->party;
    for (int i = 0; i < PARTY_SLOTS; i++) sa_party_place(p, &s->vfs, i, 0);
    p->benchCount = 0;
    int slot = PARTY_FRONT + 1;
    sa_party_place(p, &s->vfs, slot, at(img, MEMBER_AT));
    p->level[slot] = at(img, MEMBER_AT + MEMBER_LEVEL);
    p->exp[slot] = at(img, MEMBER_AT + MEMBER_EXP);
    p->hp[slot] = at(img, MEMBER_AT + MEMBER_HP);
    p->mp[slot] = at(img, MEMBER_AT + MEMBER_MP);
    p->sp[slot] = at(img, MEMBER_AT + MEMBER_SP);
    for (int k = 0; k < 10; k++) p->trained[slot][k] = at(img, MEMBER_AT + MEMBER_TRAINED + (size_t)k * 4);
    p->equip[slot] = at(img, MEMBER_AT + MEMBER_EQUIP);
    p->rosterCount = 1;
    p->roster[0] = p->unit[slot];
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

static void put(u8 *img, size_t off, int v) { wr32(img + off, (u32)v); }

static const struct {
    int off, value;
} sceneTail[] = {
    {998008, 1},   {998020, -1},  {998024, -1},  {998032, -2},  {998036, -1},  {998044, -1},  {998048, -2},
    {998052, -2},  {998056, -1},  {998060, -1},  {998064, -1},  {998068, -2},  {998072, -2},  {998076, -2},
    {998080, -2},  {998084, -2},  {998088, -2},  {998092, -1},  {998096, -1},  {998100, -1},  {998104, -1},
    {998108, -1},  {998112, -2},  {998116, -2},  {998120, -2},  {998124, -2},  {998128, -2},  {998284, -1},
};

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

static void write_member(SA *s, u8 *img, int slot) {
    const Party *p = &s->party;
    const Table *u = &p->units;
    int r = sa_party_unit_row(p, p->unit[slot]);
    size_t m = MEMBER_AT;
    put(img, m, p->unit[slot]);
    put(img, m + MEMBER_LEVEL, p->level[slot]);
    put(img, m + MEMBER_EXP, p->exp[slot]);
    put(img, m + MEMBER_BYTES, 0x00020100);
    put(img, m + MEMBER_HP, p->hp[slot]);
    put(img, m + MEMBER_MP, p->mp[slot]);
    put(img, m + MEMBER_SP, p->sp[slot]);
    for (int k = 0; k < 10; k++) put(img, m + MEMBER_TRAINED + (size_t)k * 4, p->trained[slot][k]);
    static const char *const bonusKeys[] = {"斬補正", "打補正", "射補正", "魔補正"};
    for (int copy = 0; copy < 2; copy++) {
        size_t c = m + MEMBER_STATS + (size_t)copy * MEMBER_COPY;
        for (int k = 0; k < 10; k++) put(img, c + (size_t)k * 4, sa_table_int(u, r, 23 + k) + p->trained[slot][k]);
        if (copy && p->equip[slot])
            for (int k = 0; k < 4; k++) put(img, c + (size_t)(MEMBER_BONUS + k) * 4, item_bonus(s, p->equip[slot], bonusKeys[k]));
        size_t q = m + MEMBER_RESIST + (size_t)copy * MEMBER_COPY;
        put(img, q, resist(sa_table_text(u, r, 33), "必殺耐性+"));
        put(img, q + 8, resist(sa_table_text(u, r, 33), "異常耐性+"));
        for (int k = 0; k < 3; k++) put(img, q + 12 + (size_t)k * 4, 2);
        put(img, m + MEMBER_SLOTS + (size_t)copy * MEMBER_COPY, sa_table_int(u, r, 38));
    }
    if (p->equip[slot]) {
        put(img, m + MEMBER_EQUIP, p->equip[slot]);
        put(img, m + MEMBER_EQUIP + 4, -1);
    }
    size_t k = m + MEMBER_SKILLS;
    for (int i = 0; i < 8; i++) {
        const char *name = sa_table_text(u, r, 41 + i * 3);
        int id = *name && strcmp(name, "*") ? skill_id(s, name) : 0;
        if (!id) continue;
        put(img, k, id);
        put(img, k + 4, 1);
        k += 8;
    }
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
    write_member(s, img, PARTY_FRONT + 1);
    for (int i = 0; i < PC_C_VARS; i++) put(img, VM_AT + (size_t)i * 4, s->vm.c[i]);
    memcpy(img + VM_AT + PC_C_VARS * 4, s->vm.f, PC_F_VARS);
    for (size_t i = 0; i < sizeof engineVars / sizeof engineVars[0]; i++) put(img, VM_AT + (size_t)engineVars[i].off * 4, engineVars[i].value);
    put(img, VM_AT + PLACE_VAR * 4, s->exp.active ? PLACE_DUNGEON : PLACE_TOWN);
    put(img, VM_AT + (PLACE_VAR + 1) * 4, s->exp.active ? 2 : 1);
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
    if (rd32(c + (PLACE_VAR + 1) * 4) == 2) sa_exp_names(s, (int)rd32(c + 70 * 4), (int)rd32(c + 71 * 4), &dungeon, &floor);
    snprintf(out->place[0], sizeof out->place[0], "%s", dungeon ? dungeon : "街");
    snprintf(out->place[1], sizeof out->place[1], "%s", floor ? floor : "");
    int weapon = at(img, MEMBER_AT + MEMBER_EQUIP);
    for (int i = 0; weapon && i < s->equip.items.rows; i++)
        if (sa_table_int(&s->equip.items, i, 0) == weapon) snprintf(out->weapon, sizeof out->weapon, "%s", sa_table_text(&s->equip.items, i, 1));
    free(img);
    return true;
}
