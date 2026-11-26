#include "sa_private.h"

enum {
    UNITS = 100,
    UNIT_BYTES = 136,
    ITEMS_AT = UNITS * UNIT_BYTES,
    ITEM_PAIRS = 290,
    STOCK_AT = ITEMS_AT + ITEM_PAIRS * 8,
    BLOCK_BYTES = STOCK_AT + 12,
    OPT_FLAGS_AT = 28804,
    FLAG_CLEARED = 15,
    FLAG_REGISTERED = 801,
    FIRST_KEY_ITEM = 1001,
    FILE_VERSION = 1,
    HEAD = 8,
    LEAD = 1,
};

static const char magic[4] = {'S', 'A', 'C', 'Y'};

static char *own_path(const SA *s) { return sa_fmt("%s/vndegen/carry.dat", s->saveDir); }

static bool registered(const u8 *opt) { return opt[OPT_FLAGS_AT + FLAG_REGISTERED] != 0; }

u8 *sa_carry_block(const SA *s) {
    u8 *opt = sa_records_opt_read(s);
    if (opt && registered(opt)) {
        u8 *b = sa_alloc(BLOCK_BYTES);
        memcpy(b, opt, BLOCK_BYTES);
        free(opt);
        return b;
    }
    free(opt);
    char *path = own_path(s);
    size_t n;
    u8 *d = sa_read_file(path, &n);
    free(path);
    if (!d || n != HEAD + BLOCK_BYTES || memcmp(d, magic, 4) || rd32(d + 4) != FILE_VERSION) {
        free(d);
        return NULL;
    }
    memmove(d, d + HEAD, BLOCK_BYTES);
    return d;
}

void sa_carry_load_flags(SA *s) {
    u8 *opt = sa_records_opt_read(s);
    if (!opt) return;
    const int flags[] = {FLAG_CLEARED, FLAG_REGISTERED};
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++)
        if (opt[OPT_FLAGS_AT + flags[i]] && !s->vm.g[flags[i]]) {
            s->vm.g[flags[i]] = 1;
            s->vm.gDirty = true;
        }
    free(opt);
}

void sa_carry_register(SA *s) {
    u8 *b = sa_alloc(BLOCK_BYTES);
    memset(b, 0, BLOCK_BYTES);
    sa_pcsave_carry_units(s, b, UNITS, UNIT_BYTES);
    const Party *p = &s->party;
    for (int i = 0, n = 0; i < p->items && n < ITEM_PAIRS; i++) {
        if (p->itemId[i] >= FIRST_KEY_ITEM || p->itemCount[i] <= 0) continue;
        wr32(b + ITEMS_AT + n * 8, (u32)p->itemId[i]);
        wr32(b + ITEMS_AT + n++ * 8 + 4, (u32)p->itemCount[i]);
    }
    wr32(b + STOCK_AT, (u32)s->vm.c[C_FOOD]);
    wr32(b + STOCK_AT + 4, (u32)s->vm.c[C_MONEY]);
    wr32(b + STOCK_AT + 8, (u32)s->vm.c[C_RESOURCES]);
    u8 *d = sa_alloc(HEAD + BLOCK_BYTES);
    memcpy(d, magic, 4);
    wr32(d + 4, FILE_VERSION);
    memcpy(d + HEAD, b, BLOCK_BYTES);
    char *dir = sa_fmt("%s/vndegen", s->saveDir), *path = own_path(s);
    sa_mkdirs(dir);
    sa_write_file(path, d, HEAD + BLOCK_BYTES);
    free(dir);
    free(path);
    free(d);
    u8 *opt = sa_records_opt_read(s);
    if (opt) {
        memcpy(opt, b, BLOCK_BYTES);
        opt[OPT_FLAGS_AT + FLAG_REGISTERED] = 1;
        if (s->vm.g[FLAG_CLEARED]) opt[OPT_FLAGS_AT + FLAG_CLEARED] = 1;
        sa_records_opt_write(s, opt);
        free(opt);
    }
    free(b);
    sa_log(s, "carry-over registered: food %d, money %d, resources %d", s->vm.c[C_FOOD], s->vm.c[C_MONEY], s->vm.c[C_RESOURCES]);
}

static int free_count(Party *p, int id) {
    Member m[PARTY_ROSTER + PARTY_SLOTS + PARTY_BENCH];
    int n = sa_party_member_list(p, m), used = 0;
    for (int i = 0; i < n; i++) used += *m[i].equip == id;
    return sa_party_item_count(p, id) - used;
}

const u8 *sa_carry_unit(const u8 *block, int unit) {
    for (int k = 0; k < UNITS; k++)
        if (unit && (int)rd32(block + k * UNIT_BYTES) == unit) return block + k * UNIT_BYTES;
    return NULL;
}

static void apply_unit(SA *s, const u8 *b, int unit) {
    const u8 *r = sa_carry_unit(b, unit);
    if (!r) return;
    Party *p = &s->party;
    Member m[PARTY_ROSTER + PARTY_SLOTS + PARTY_BENCH];
    int n = sa_party_member_list(p, m);
    for (int i = 0; i < n; i++) {
        if (m[i].unit != unit) continue;
        *m[i].sp = (int)rd32(r + CARRY_SP);
        for (int k = 0; k < 10; k++) m[i].trained[k] = (int)rd32(r + CARRY_TRAINED + k * 4);
        int equip = (int)rd32(r + CARRY_EQUIP);
        *m[i].equip = 0;
        if (equip > 0 && free_count(p, equip) > 0) *m[i].equip = equip;
        *m[i].hp = sa_party_member_value(p, &m[i], 0);
        *m[i].mp = sa_party_member_value(p, &m[i], 1);
        s->dirty = true;
        return;
    }
}

void sa_carry_restore(SA *s) {
    u8 *b = sa_carry_block(s);
    if (!b) {
        sa_log(s, "carry-over: nothing registered");
        return;
    }
    s->vm.c[C_FOOD] = (int)rd32(b + STOCK_AT);
    s->vm.c[C_MONEY] = (int)rd32(b + STOCK_AT + 4);
    s->vm.c[C_RESOURCES] = (int)rd32(b + STOCK_AT + 8);
    for (int i = 0; i < ITEM_PAIRS; i++) {
        int id = (int)rd32(b + ITEMS_AT + i * 8), count = (int)rd32(b + ITEMS_AT + i * 8 + 4);
        if (id <= 0 || count <= 0) continue;
        sa_party_add_item(&s->party, id, count);
        sa_records_item(s, id);
    }
    s->vm.f[CARRY_FLAG] = 1;
    apply_unit(s, b, LEAD);
    free(b);
    sa_log(s, "carry-over restored: food %d, money %d, resources %d", s->vm.c[C_FOOD], s->vm.c[C_MONEY], s->vm.c[C_RESOURCES]);
}

void sa_carry_join(SA *s, const char *key) {
    if (!s->vm.f[CARRY_FLAG]) return;
    const Table *u = &s->party.units;
    int unit = 0;
    for (int i = 0; i < u->rows && !unit; i++)
        if (!strcmp(sa_table_text(u, i, 1), key)) unit = sa_table_int(u, i, 0);
    if (!unit) return;
    u8 *b = sa_carry_block(s);
    if (!b) return;
    apply_unit(s, b, unit);
    free(b);
}
