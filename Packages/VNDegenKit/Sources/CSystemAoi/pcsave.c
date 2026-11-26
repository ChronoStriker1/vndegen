#include "sa_private.h"

#include <strings.h>

#define FIXED_SKILL "固定／"
#define PLAIN_MAGIC "無"

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
    EDITS_AT = 44,
    EDIT_INTS = 6,
    MAPS_AT = 6172,
    MAP_RECORD = 532,
    MAP_HEAD = 16,
    MAP_LAYERS = 20,
    MAP_LAYER = 128,
    MAP_DUNGEONS = 15,
    MAP_FLOORS = 10,
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
    BLOCK_AILMENT = 176,
    BLOCK_CRITICAL = 184,
    BLOCK_ABILITIES = 188,
    BLOCK_SLOTS = 456,
    MEMBER_SKILLS = 1220,
    MEMBER_BONUS = 10,
    FIXED_MARK = 3,
    BLOCK_RESIST = 33,
    BLOCK_ELEMENT_ATTACK = 14,
    ELEMENT_ATTACK = 30,
    ELEMENT_STEP = 10,
    ELEMENTS = 6,
    BLOCK_PLAIN_ATTACK = 13,
    BLOCK_RESISTS = 11,
    BLOCK_ELEMENT_FIRST = 4,
    BLOCK_MAGIC = 36,
    BLOCK_THUNDER = 43,
    BLOCK_GOV_KIND = 65,
    GOV_KINDS = 13,
    BLOCK_TEAM_EXP = 113,
    DARKUS_COUNTER = 1284,
    MEMBER_CARRY = 1288,
    DARKUS_COUNTER_VAR = 710,
    FIELD_TAG_BASE = 8,
    TEAM_EXP = 0,
    TEAM_MAGIC,
    TEAM_KINDS,
    GRID_AT = 1010848,
    GRID_AFTER = 1012648,
    CACHE_AT = 1012656,
    CACHE_RECORD = 56,
    CACHE_NAME = 48,
    CACHE_MAX = 400,
    CACHE_COUNT_AT = 1035056,
    SCENE_AT = 993208,
    SCENE_STEP = 1200,
    ORDER_RECORDS = 4,
    FREE_AT = 1008208,
    FREE_COUNT = 30,
    TOWN_POSTS = 9,
    BLOCK_GOVERN = 8,
    POSTED = 3,
    PLACE_VAR = 4,
    PLACE_TOWN = 6,
    SEED_VAR = 6,
    DUNGEON_VAR = 70,
    PLACE_DUNGEON = 62,
    POS_VAR = 73,
    LAST_AT = 998016,
    ENTRY_AT = 85972,
    GROUP_AT = 998288,
    GROUP_SIZE = 280,
    GROUPS_MAX = EXP_GROUPS,
    GROUP_CELLS = 6,
    GROUP_RESERVE = 21,
    GROUP_TARGETS = 53,
    GROUP_FLAGS = 63,
    GROUP_CLOCK = 65,
    GROUP_FOUGHT = 66,
    GROUP_EXTRA = 40,
    ENEMY_AT = 215608,
    ENEMY_BASE = 100,
    ENEMIES_MAX = 30,
    STAND_AT = 1009560,
    STAND_COUNT = 13,
    MAZE_AT = 1009652,
    MAZE_FLOOR_SIZE = 168,
    MAZE_NET_AT = 1010836,
    BOX_SLOTS_AT = 1035060,
    BOX_SLOTS = 50,
    BOX_NAME_SIZE = 48,
    MISSIONS_AT = 1008328,
    PLACE_FIELD = 1,
    PLACE_RESERVE = 2,
    PLACE_NONE = 4,
    DARKUS = 1,
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

static size_t map_record(int index) { return MAPS_AT + (size_t)index * MAP_RECORD; }

static void read_edits(SA *s, const u8 *img) {
    Dungeon *d = &s->dungeon;
    d->editCount = 0;
    for (size_t o = EDITS_AT; o + EDIT_INTS * 4 <= MAPS_AT && at(img, o) && d->editCount < DUN_EDITS; o += EDIT_INTS * 4) {
        DunEdit *e = &d->edit[d->editCount++];
        int v[EDIT_INTS];
        for (int k = 0; k < EDIT_INTS; k++) v[k] = at(img, o + (size_t)k * 4);
        *e = (DunEdit){v[0], v[1], v[2], v[3], v[4], v[5]};
    }
}

static void write_edits(SA *s, u8 *img) {
    const Dungeon *d = &s->dungeon;
    for (int i = 0; i < d->editCount; i++) {
        const DunEdit *e = &d->edit[i];
        const int v[EDIT_INTS] = {e->area, e->x, e->y, e->off, e->value, e->mode};
        for (int k = 0; k < EDIT_INTS; k++) put(img, EDITS_AT + ((size_t)i * EDIT_INTS + (size_t)k) * 4, v[k]);
    }
}

static void read_maps(SA *s, const u8 *img) {
    if (!s->exp.explored) return;
    for (int r = 0; r < MAP_DUNGEONS * MAP_FLOORS; r++) {
        u32 head = rd32(img + map_record(r) + MAP_HEAD);
        int d = (int)(head >> 16 & 0xff), f = (int)(head >> 8 & 0xff);
        if (!head || d < 1 || d >= EXP_DUNGEONS || f < 1 || f >= EXP_FLOORS) continue;
        for (int i = 0; i < 4; i++) s->exp.floorInfo[d][f][i] = at(img, map_record(r + 1) + (size_t)i * 4);
        size_t o = map_record(r) + MAP_LAYERS;
        for (int y = 0; y < DUN_SIDE; y++) {
            size_t k = (size_t)y * 4;
            u32 bits = rd32(img + o + k), east = rd32(img + o + MAP_LAYER + k), south = rd32(img + o + 2 * MAP_LAYER + k);
            for (int x = 0; x < DUN_SIDE; x++) {
                int c = y * DUN_SIDE + x;
                s->exp.explored[d][f][c] = bits >> x & 1;
                s->exp.passages[d][f][c] = (u8)((east >> x & 1 ? PASS_EAST : 0) | (south >> x & 1 ? PASS_SOUTH : 0));
            }
        }
    }
}

static void write_maps(SA *s, u8 *img) {
    if (!s->exp.explored) return;
    int r = 0;
    for (int d = 1; d < EXP_DUNGEONS; d++)
        for (int f = 1; f < EXP_FLOORS && r < MAP_DUNGEONS * MAP_FLOORS; f++) {
            const bool *v = s->exp.explored[d][f];
            bool any = false;
            for (int c = 0; c < DUN_CELLS && !any; c++) any = v[c];
            if (!any) continue;
            size_t o = map_record(r++);
            for (int i = 0; i < 4; i++) put(img, map_record(r) + (size_t)i * 4, s->exp.floorInfo[d][f][i]);
            put(img, o + MAP_HEAD, d << 16 | f << 8);
            for (int x = 0; x < DUN_SIDE; x++)
                for (int y = 0; y < DUN_SIDE; y++) {
                    int c = y * DUN_SIDE + x;
                    u8 pass = s->exp.passages[d][f][c];
                    bool layer[4] = {v[c], pass & PASS_EAST, pass & PASS_SOUTH, v[c]};
                    for (int l = 0; l < 4; l++) {
                        if (!layer[l]) continue;
                        size_t w = o + MAP_LAYERS + (size_t)l * MAP_LAYER + (size_t)y * 4;
                        wr32(img + w, rd32(img + w) | 1u << x);
                    }
                }
        }
}

static int pc_post(int post) { return post > TOWN_POSTS ? post + 1 : post; }

static int vn_post(int id) { return id > TOWN_POSTS + 1 ? id - 1 : id <= TOWN_POSTS ? id : 0; }

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
        b.tile = slot < 0 ? cell - FORM_RESERVE : -1;
        if (slot >= 0) put_slot(s, slot, &b);
        else if (p->benchCount < PARTY_BENCH) p->bench[p->benchCount++] = b;
        done[k] = true;
    }
    for (int k = 0; k < count; k++)
        if (!done[k] && p->benchCount < PARTY_BENCH) {
            Bench b = member_at(img, k);
            b.tile = -1;
            p->bench[p->benchCount++] = b;
        }
    memset(p->post, 0, sizeof p->post);
    for (int i = 1; i < FREE_COUNT; i++) {
        int k = at(img, FREE_AT + (size_t)i * 4), post = vn_post(i);
        if (k >= 0 && k < count && post > 0 && post < PARTY_POSTS) p->post[post] = p->roster[k];
    }
    if (!sa_party_members(p) && p->benchCount) {
        Bench b = p->bench[0];
        memmove(p->bench, p->bench + 1, sizeof(Bench) * (size_t)--p->benchCount);
        put_slot(s, PARTY_FRONT + 1, &b);
    }
}

static void read_dungeon(SA *s, const u8 *img);
static void read_orders(SA *s, const u8 *img, int members);

static void write_maze(SA *s, u8 *img) {
    put(img, MAZE_AT, s->vm.c[C_MAZE_INTRUDERS]);
    for (int k = 0; k < 7; k++)
        for (int a = 0; a < 7; a++)
            for (int i = 0; i < 6; i++) put(img, MAZE_AT + 4 + (size_t)k * MAZE_FLOOR_SIZE + (size_t)(a * 6 + i) * 4, s->maze.event[k][a][i]);
    for (int i = 0; i < 3; i++) put(img, MAZE_NET_AT + (size_t)i * 4, s->vm.c[C_MAZE_FOOD + i]);
}

static void write_box_slots(SA *s, u8 *img) {
    for (int i = 0; i < BOX_SLOTS; i++) {
        char name[24];
        if (s->vm.boxSlot[i]) snprintf(name, sizeof name, "%s", s->vm.boxSlot[i]);
        else snprintf(name, sizeof name, "EV%02d.box", i);
        u8 *o = img + BOX_SLOTS_AT + (size_t)i * BOX_NAME_SIZE;
        for (int k = 0; name[k] && k < BOX_NAME_SIZE / 2 - 1; k++) o[k * 2] = (u8)name[k];
    }
}

static int box_order(const void *a, const void *b) {
    return strcasecmp((*(const LoadedBox *const *)a)->file, (*(const LoadedBox *const *)b)->file);
}

static void write_cache(SA *s, u8 *img) {
    const LoadedBox *list[SA_BOXES];
    int n = 0;
    for (int i = 0; i < s->vm.boxCount; i++)
        if (s->vm.boxes[i].box.count > 1) list[n++] = &s->vm.boxes[i];
    qsort(list, (size_t)n, sizeof list[0], box_order);
    for (int i = 0; i < n && i < CACHE_MAX; i++) {
        u8 *o = img + CACHE_AT + (size_t)i * CACHE_RECORD;
        for (int k = 0; list[i]->file[k] && k < CACHE_NAME / 2 - 1; k++) o[k * 2] = (u8)list[i]->file[k];
        o[CACHE_NAME] = 1;
        o[CACHE_NAME + 2] = (u8)list[i]->box.count;
        o[CACHE_NAME + 3] = (u8)(list[i]->box.count >> 8);
    }
    put(img, CACHE_COUNT_AT, n < CACHE_MAX ? n : CACHE_MAX);
}

static void read_cache(SA *s, const u8 *img) {
    int n = at(img, CACHE_COUNT_AT);
    for (int i = 0; i < n && i < CACHE_MAX; i++) {
        const u8 *o = img + CACHE_AT + (size_t)i * CACHE_RECORD;
        char name[CACHE_NAME / 2];
        int k = 0;
        for (; k < CACHE_NAME / 2 - 1 && o[k * 2]; k++) name[k] = (char)o[k * 2];
        name[k] = 0;
        if (k) sa_vm_register_hooks(&s->vm, &s->vfs, name);
    }
}

static void read_maze(SA *s, const u8 *img) {
    for (int k = 0; k < 7; k++)
        for (int a = 0; a < 7; a++)
            for (int i = 0; i < 6; i++) s->maze.event[k][a][i] = at(img, MAZE_AT + 4 + (size_t)k * MAZE_FLOOR_SIZE + (size_t)(a * 6 + i) * 4);
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
        f->built = f->type ? at(img, o + 4) : 0;
    }
    s->town.stage = s->vm.c[C_TOWN_STAGE];
    read_members(s, img);
    read_orders(s, img, s->party.rosterCount);
    read_maps(s, img);
    read_edits(s, img);
    s->exp.active = s->vm.c[DUNGEON_VAR] > 0 && s->vm.c[DUNGEON_VAR + 1] > 0;
    s->exp.dungeon = s->vm.c[DUNGEON_VAR];
    s->exp.floor = s->vm.c[DUNGEON_VAR + 1];
    read_dungeon(s, img);
    read_maze(s, img);
    read_cache(s, img);
    s->missions.logCount = 0;
    for (int i = 0; i < MISSION_LOG && at(img, MISSIONS_AT + (size_t)i * 12); i++, s->missions.logCount++)
        for (int k = 0; k < 3; k++) s->missions.log[i][k] = at(img, MISSIONS_AT + (size_t)i * 12 + (size_t)k * 4);
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

static int skill_row(SA *s, const char *name) {
    const Table *k = &s->battle.skills;
    for (int i = 0; i < k->rows; i++)
        if (!strcmp(sa_table_text(k, i, 1), name)) return i;
    return -1;
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

static const char *const resistKeys[] = {"斬耐性", "打耐性", "射耐性", "魔耐性", "光耐性", "闇耐性", "火耐性", "水耐性", "土耐性", "風耐性", "雷耐性"};
static const char *const elements[] = {"光", "闇", "火", "水", "土", "風"};
static const int lightDark[2][BLOCK_RESISTS] = {{[4] = 20, [5] = -50}, {[4] = -50, [5] = 20}};

static int element_affinity(int e, int i) {
    if (e < 2) return lightDark[e][i];
    int k = i - BLOCK_ELEMENT_FIRST - 2;
    if (i == BLOCK_ELEMENT_FIRST || i == BLOCK_ELEMENT_FIRST + 1 || i == BLOCK_THUNDER - BLOCK_RESIST) return -20;
    if (k < 0 || k > 3) return 0;
    int self = e - 2;
    return k == self ? 10 : k == (self + 1) % 4 ? 50 : k == (self + 3) % 4 ? -50 : 0;
}

static int element_of(const char *name) {
    int e = 0;
    while (e < ELEMENTS && strcmp(name, elements[e])) e++;
    return e;
}

static int attack_slot(const char *attribute) {
    int e = element_of(attribute);
    return e < ELEMENTS ? BLOCK_ELEMENT_ATTACK + e : strcmp(attribute, PLAIN_MAGIC) ? -1 : BLOCK_PLAIN_ATTACK;
}

static void write_resists(SA *s, u8 *img, size_t c, int r, const int *skills, int n) {
    const Table *u = &s->party.units, *k = &s->battle.skills;
    const char *text = sa_table_text(u, r, 33);
    int e = element_of(sa_table_text(u, r, 14)), known[BLOCK_ELEMENT_ATTACK + ELEMENTS] = {0};
    if (e < ELEMENTS) put(img, c + (size_t)(BLOCK_ELEMENT_ATTACK + e) * 4, ELEMENT_ATTACK);
    for (int i = 0; i < n; i++) {
        int slot = attack_slot(sa_table_text(k, skills[i], 14));
        if (slot >= 0 && known[slot]++) put(img, c + (size_t)slot * 4, at(img, c + (size_t)slot * 4) + ELEMENT_STEP);
    }
    for (int i = 0; i < BLOCK_RESISTS; i++) {
        int v = resist(text, resistKeys[i]) + (e < ELEMENTS ? element_affinity(e, i) : 0);
        for (int j = 0; j < n; j++) v += resist(sa_table_text(k, skills[j], 6), resistKeys[i]);
        put(img, c + (size_t)(BLOCK_RESIST + i) * 4, v);
    }
}

static int unit_skills(SA *s, int r, const Bench *b, bool enemy, int *rows, int *marks) {
    const Table *u = &s->party.units;
    int n = 0;
    for (int k = 0; k < 8; k++) {
        const char *name = sa_table_text(u, r, 41 + k * 3);
        int row = *name && strcmp(name, "*") ? skill_row(s, name) : -1;
        if (row < 0 || !sa_party_unit_knows(&s->party, r, enemy ? b->level : sa_party_skill_level(&s->party, b->unit, b->level), k)) continue;
        rows[n] = row;
        marks[n++] = strncmp(name, FIXED_SKILL, strlen(FIXED_SKILL)) ? 1 : FIXED_MARK;
    }
    return n;
}

static void write_unit(SA *s, u8 *img, size_t m, const Bench *b, bool enemy, const int *bonus) {
    const Party *p = &s->party;
    const Table *u = &p->units;
    int r = sa_party_unit_row(p, b->unit);
    int skills[8], marks[8], n = unit_skills(s, r, b, enemy, skills, marks);
    put(img, m, b->unit);
    put(img, m + MEMBER_LEVEL, b->level);
    put(img, m + MEMBER_EXP, b->exp);
    put(img, m + MEMBER_HP, b->hp);
    put(img, m + MEMBER_MP, b->mp);
    put(img, m + MEMBER_SP, b->sp);
    for (int i = 0; i < 10; i++) put(img, m + MEMBER_TRAINED + (size_t)i * 4, b->trained[i]);
    static const char *const bonusKeys[] = {"斬補正", "打補正", "射補正", "魔補正"};
    for (int copy = 0; copy < 2; copy++) {
        size_t c = m + MEMBER_STATS + (size_t)copy * MEMBER_COPY;
        int v[10];
        sa_battle_unit_stats(&s->battle, r, b->level > 0 ? b->level : 1, v);
        for (int i = 0; i < 10; i++) {
            int value = v[i] + b->trained[i] + (copy && bonus ? bonus[i] : 0);
            put(img, c + (size_t)i * 4, copy && i >= BLOCK_GOVERN ? sa_party_govern_cap(value) : value);
        }
        write_resists(s, img, c, r, skills, n);
        if (copy && b->equip)
            for (int i = 0; i < 4; i++) put(img, c + (size_t)(MEMBER_BONUS + i) * 4, at(img, c + (size_t)(MEMBER_BONUS + i) * 4) + item_bonus(s, b->equip, bonusKeys[i]));
        put(img, c + BLOCK_CRITICAL, resist(sa_table_text(u, r, 33), "必殺耐性"));
        put(img, c + BLOCK_AILMENT, resist(sa_table_text(u, r, 33), "異常耐性"));
        for (int i = 0; i < 3; i++) put(img, c + BLOCK_ABILITIES + (size_t)i * 4, ability_id(s, sa_table_text(u, r, 34 + i)));
        put(img, c + BLOCK_SLOTS, sa_table_int(u, r, 38));
    }
    if (b->equip) {
        put(img, m + MEMBER_EQUIP, b->equip);
        put(img, m + MEMBER_EQUIP + 4, -1);
    }
    for (int i = 0; i < n; i++) {
        put(img, m + MEMBER_SKILLS + (size_t)i * 8, sa_table_int(&s->battle.skills, skills[i], 0));
        put(img, m + MEMBER_SKILLS + (size_t)i * 8 + 4, marks[i]);
    }
}

static const struct {
    const char *name;
    unsigned char slot[3], col[3];
} activeSlots[] = {
    {"罠発見", {52}, {4}},       {"罠無効化", {53}, {4}},   {"遠視", {57}, {4}},
    {"地図", {59}, {4}},         {"サブメンバー", {92, 93}, {4, 5}},
    {"邪魔回復", {94}, {4}},     {"身代わり", {95}, {4}},   {"囮", {98}, {4}},
    {"援護射撃", {103, 104}, {4, 5}},
    {"自爆", {105}, {4}},        {"生命力", {106}, {4}},    {"凶暴者", {107, 108, 112}, {4, 4, 5}},
    {"勤勉", {BLOCK_TEAM_EXP}, {4}},     {"先制攻撃", {118}, {4}},  {"前向精神", {83}, {4}},
    {"魔法無効化", {97}, {4}},   {"いなし", {101, 102}, {4, 5}},
};

static const struct {
    const char *name;
    int slot;
} govSlots[] = {{"政治家", 63}, {"大将軍", 64}, {"人気者", 78}, {"資金管理", 79}, {"資源管理", 80}, {"食料管理", 81}};

static const int govKindPost[GOV_KINDS] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 19};

static const char *const areaResists[] = {"斬撃属性耐性／", "打撃属性耐性／", "射撃属性耐性／"};

static int ability_col(SA *s, const char *name, int col) {
    const Table *a = &s->party.abilities;
    for (int i = 0; i < a->rows; i++)
        if (!strcmp(sa_table_text(a, i, 1), name)) return sa_table_int(a, i, col);
    return 0;
}

static void add(u8 *img, size_t off, int v) { put(img, off, at(img, off) + v); }

static const char *const teamAbilities[TEAM_KINDS] = {"学", "魔法全体防御"};

static void team_bonus(SA *s, int n, const int *rows, const int *cols, int *team) {
    for (int k = 0; k < TEAM_KINDS; k++) {
        team[k] = 0;
        for (int i = 0; i < n; i++)
            if (!strcmp(sa_table_text(&s->party.units, rows[i], cols[i]), teamAbilities[k])) team[k] += ability_col(s, teamAbilities[k], 4);
    }
}

static void area_resists(SA *s, int n, const int *rows, const int *gr, const int *gc, int (*out)[3]) {
    for (int i = 0; i < n; i++) memset(out[i], 0, sizeof out[i]);
    for (int i = 0; i < n; i++) {
        const char *name = sa_table_text(&s->party.units, rows[i], 34);
        for (int k = 0; k < 3; k++) {
            if (strncmp(name, areaResists[k], strlen(areaResists[k]))) continue;
            const char *range = name + strlen(areaResists[k]);
            int value = ability_col(s, name, 4);
            for (int j = 0; j < n; j++)
                if (!strcmp(range, "全") || (!strcmp(range, "前") && gr[j] == 0) || (!strcmp(range, "後") && gr[j] == 2) ||
                    (!strcmp(range, "縦") && gc[j] == gc[i]) || (!strcmp(range, "横") && gr[j] == gr[i]))
                    out[j][k] += value;
        }
    }
}

static void write_abilities(SA *s, u8 *img, size_t m, int unit, int place, const int *team, const int *resists) {
    const Party *p = &s->party;
    int r = sa_party_unit_row(p, unit);
    size_t c = m + MEMBER_STATS + MEMBER_COPY;
    if (place == PLACE_FIELD || place == PLACE_RESERVE) {
        const char *name = sa_table_text(&p->units, r, place == PLACE_FIELD ? 34 : 35);
        for (size_t i = 0; i < sizeof activeSlots / sizeof activeSlots[0]; i++) {
            if (strcmp(name, activeSlots[i].name)) continue;
            for (int k = 0; k < 3 && activeSlots[i].slot[k]; k++) add(img, c + (size_t)activeSlots[i].slot[k] * 4, ability_col(s, name, activeSlots[i].col[k]));
        }
        int magic = team[TEAM_MAGIC] + (strcmp(name, "魔法耐性") ? 0 : ability_col(s, name, 4));
        for (int i = BLOCK_MAGIC; i <= BLOCK_THUNDER; i++) add(img, c + (size_t)i * 4, magic);
        add(img, c + BLOCK_TEAM_EXP * 4, team[TEAM_EXP]);
        for (int k = 0; resists && k < 3; k++) add(img, c + (size_t)(BLOCK_RESIST + k) * 4, resists[k]);
        return;
    }
    const char *gov = sa_table_text(&p->units, r, 36);
    for (size_t i = 0; i < sizeof govSlots / sizeof govSlots[0]; i++)
        if (!strcmp(gov, govSlots[i].name)) put(img, c + (size_t)govSlots[i].slot * 4, ability_col(s, gov, 4));
    for (int k = 0; k < GOV_KINDS; k++) put(img, c + (size_t)(BLOCK_GOV_KIND + k) * 4, sa_party_post_effect(p, unit, govKindPost[k]));
}

static void write_member(SA *s, u8 *img, int k, const Bench *b, int cell, const int *bonus, const int *resists, const int *team, const u8 *carry) {
    size_t m = MEMBER_AT + (size_t)k * MEMBER_SIZE;
    write_unit(s, img, m, b, false, bonus);
    const u8 *rec = carry ? sa_carry_unit(carry, b->unit) : NULL;
    if (rec) memcpy(img + m + MEMBER_CARRY, rec + CARRY_FIELD, 8);
    put(img, m + 4, k);
    put(img, m + 8, k);
    if (b->unit == DARKUS) put(img, m + DARKUS_COUNTER, s->vm.c[DARKUS_COUNTER_VAR]);
    int post = sa_party_post_of(&s->party, b->unit);
    int place = post ? POSTED : cell < 0 ? PLACE_NONE : cell >= FORM_RESERVE ? PLACE_RESERVE : PLACE_FIELD;
    write_abilities(s, img, m, b->unit, place, team, resists);
    if (post) {
        int id = pc_post(post), gov, lead;
        size_t c = m + MEMBER_STATS + MEMBER_COPY;
        put(img, m + MEMBER_BYTES, id << 16 | POSTED << 8);
        put(img, FREE_AT + (size_t)id * 4, k);
        sa_party_post_stats(&s->party, b->unit, post, &gov, &lead);
        put(img, c + BLOCK_GOVERN * 4, sa_party_govern_cap(gov + b->trained[BLOCK_GOVERN]));
        put(img, c + (BLOCK_GOVERN + 1) * 4, sa_party_govern_cap(lead + b->trained[BLOCK_GOVERN + 1]));
    } else if (place == PLACE_FIELD)
        put(img, m + MEMBER_BYTES, (cell - FIELD_TAG_BASE) << 16 | PLACE_FIELD << 8);
    else if (place == PLACE_RESERVE)
        put(img, m + MEMBER_BYTES, (cell - FORM_RESERVE) << 16 | PLACE_RESERVE << 8);
    else
        put(img, m + MEMBER_BYTES, (k ? k : 2) << 16 | PLACE_NONE << 8);
}

static void party_bonus(SA *s, int (*delta)[10], int (*resists)[3], int *team) {
    const Party *p = &s->party;
    int rows[PARTY_SLOTS + PARTY_RESERVE], cols[PARTY_SLOTS + PARTY_RESERVE], gr[PARTY_SLOTS], gc[PARTY_SLOTS], slot[PARTY_SLOTS], n = 0;
    for (int sl = 0; sl < PARTY_SLOTS; sl++) {
        memset(delta[sl], 0, sizeof delta[sl]);
        memset(resists[sl], 0, sizeof resists[sl]);
        if (!p->unit[sl]) continue;
        rows[n] = sa_party_unit_row(p, p->unit[sl]);
        cols[n] = 34;
        gr[n] = sl >= PARTY_FRONT ? 0 : 2;
        gc[n] = sl % PARTY_FRONT;
        slot[n++] = sl;
    }
    int d[PARTY_SLOTS][10], res[PARTY_SLOTS][3];
    sa_battle_position_bonus(&s->battle, n, rows, gr, gc, d);
    area_resists(s, n, rows, gr, gc, res);
    for (int i = 0; i < n; i++) {
        memcpy(delta[slot[i]], d[i], sizeof d[i]);
        memcpy(resists[slot[i]], res[i], sizeof res[i]);
    }
    int all = n;
    for (int i = 0; i < p->benchCount && all < PARTY_SLOTS + PARTY_RESERVE; i++)
        if (p->bench[i].tile >= 0 && p->bench[i].tile < RESERVE_TILES && !sa_party_post_of(p, p->bench[i].unit)) {
            rows[all] = sa_party_unit_row(p, p->bench[i].unit);
            cols[all++] = 35;
        }
    team_bonus(s, all, rows, cols, team);
}

static int write_members(SA *s, u8 *img) {
    const Party *p = &s->party;
    int bonus[PARTY_SLOTS][10], resists[PARTY_SLOTS][3], team[TEAM_KINDS];
    party_bonus(s, bonus, resists, team);
    for (int cell = 8; cell < FORM_RESERVE + RESERVE_TILES + 5; cell++)
        put(img, FORM_AT + (size_t)cell * 4, form_slot(cell) >= 0 || (cell >= FORM_RESERVE && cell < FORM_RESERVE + RESERVE_TILES) ? -1 : -2);
    u8 *carry = s->vm.f[CARRY_FLAG] ? sa_carry_block(s) : NULL;
    int k = 0;
    for (int i = 0; i < p->rosterCount && k < MEMBERS_MAX; i++) {
        int id = p->roster[i];
        Bench b = {0};
        int cell = -1, at = -1;
        for (int sl = 0; sl < PARTY_SLOTS && cell < 0; sl++) {
            if (p->unit[sl] != id) continue;
            at = sl;
            b.unit = id, b.hp = p->hp[sl], b.mp = p->mp[sl], b.level = p->level[sl], b.exp = p->exp[sl], b.equip = p->equip[sl], b.sp = p->sp[sl];
            memcpy(b.trained, p->trained[sl], sizeof b.trained);
            cell = sl >= PARTY_FRONT ? FORM_TOP + sl - PARTY_FRONT : FORM_BOTTOM + sl;
        }
        for (int j = 0; j < p->benchCount && !b.unit; j++)
            if (p->bench[j].unit == id) b = p->bench[j];
        if (!b.unit) continue;
        if (cell < 0 && b.tile >= 0 && b.tile < RESERVE_TILES) cell = FORM_RESERVE + b.tile;
        write_member(s, img, k, &b, cell, at >= 0 ? bonus[at] : NULL, at >= 0 ? resists[at] : NULL, team, carry);
        if (cell >= 0) put(img, FORM_AT + (size_t)cell * 4, k);
        k++;
    }
    free(carry);
    return k;
}

void sa_pcsave_carry_units(SA *s, u8 *out, int count, int size) {
    u8 *img = sa_alloc(IMAGE_SIZE);
    memset(img, 0, IMAGE_SIZE);
    int n = write_members(s, img);
    for (int k = 0; k < n && k < count; k++) {
        const u8 *m = img + MEMBER_AT + (size_t)k * MEMBER_SIZE;
        u8 *o = out + (size_t)k * (size_t)size;
        memcpy(o, m, 4);
        memcpy(o + CARRY_LEVEL, m + MEMBER_LEVEL, 8);
        memcpy(o + CARRY_SP, m + MEMBER_SP, 44);
        memcpy(o + CARRY_EQUIP, m + MEMBER_EQUIP, 72);
        wr32(o + CARRY_FIELD, (rd32(m + MEMBER_BYTES) >> 8 & 0xff) == PLACE_FIELD);
        wr32(o + CARRY_USED, 1);
    }
    free(img);
}

static const int modeCodes[] = {[ORDER_FREE] = -1, [ORDER_ATTACK] = -2, [ORDER_SUPPORT] = -4, [ORDER_DEFEND] = -3,
                                [ORDER_SWAP] = -5, [ORDER_INTRUDE] = -1, [ORDER_FLEE] = -6, [ORDER_SKILL] = -1};
static const int aimCodes[AIM_COUNT] = {[AIM_FREE] = -1,  [AIM_ALL] = -1,   [AIM_ENEMY] = -2,     [AIM_ALLY] = -3,
                                        [AIM_FRONT] = -4, [AIM_MIDDLE] = -5, [AIM_BACK] = -6,     [AIM_RIGHT] = -7,
                                        [AIM_LEFT] = -9,  [AIM_CENTRE] = -8, [AIM_RIGHTMOST] = -1, [AIM_LEFTMOST] = -1};

static int code_index(const int *codes, int n, int code) {
    for (int i = 0; i < n; i++)
        if (codes[i] == code) return i;
    return 0;
}

static int member_slot(const Party *p, int unit) {
    for (int sl = 0; sl < PARTY_SLOTS; sl++)
        if (unit && p->unit[sl] == unit) return sl;
    return -1;
}

static void write_orders(SA *s, u8 *img, int members) {
    const Party *p = &s->party;
    for (int k = 0; k < ORDER_RECORDS; k++)
        for (int i = 0; i < (members ? members : 1); i++) {
            size_t o = SCENE_AT + (size_t)k * SCENE_STEP + (size_t)i * 12;
            int slot = member_slot(p, at(img, MEMBER_AT + (size_t)i * MEMBER_SIZE));
            const Order *d = slot < 0 ? NULL : k ? &p->preset[k - 1][slot] : &p->order[slot];
            bool skill = d && d->mode == ORDER_SKILL && d->skill >= 0;
            put(img, o, skill ? sa_table_int(&s->battle.skills, d->skill, 0) : d ? modeCodes[d->mode] : -1);
            put(img, o + 4, 1);
            put(img, o + 8, d && d->aim >= 0 && d->aim < AIM_COUNT ? aimCodes[d->aim] : -1);
        }
}

static void read_orders(SA *s, const u8 *img, int members) {
    Party *p = &s->party;
    memset(p->order, 0, sizeof p->order);
    memset(p->preset, 0, sizeof p->preset);
    memset(p->presetSaved, 0, sizeof p->presetSaved);
    for (int k = 0; k < ORDER_RECORDS; k++)
        for (int i = 0; i < members; i++) {
            size_t o = SCENE_AT + (size_t)k * SCENE_STEP + (size_t)i * 12;
            int slot = member_slot(p, at(img, MEMBER_AT + (size_t)i * MEMBER_SIZE)), action = at(img, o), aim = at(img, o + 8);
            if (slot < 0) continue;
            Order *d = k ? &p->preset[k - 1][slot] : &p->order[slot];
            const Table *t = &s->battle.skills;
            int row = -1;
            for (int r = 0; action >= 0 && r < t->rows && row < 0; r++)
                if (sa_table_int(t, r, 0) == action) row = r;
            *d = (Order){row >= 0 ? ORDER_SKILL : code_index(modeCodes, ORDER_SKILL, action), row, code_index(aimCodes, AIM_COUNT, aim)};
            if (k && (action != -1 || aim != -1)) p->presetSaved[k - 1] = true;
        }
}

static void write_dungeon(SA *s, u8 *img) {
    const Expedition *e = &s->exp;
    const int here[4] = {e->dungeon << 16 | e->floor << 8, s->dungeon.x, s->dungeon.y, s->dungeon.dir};
    for (int i = 0; i < 4; i++) put(img, LAST_AT + (size_t)i * 4, e->active ? here[i] : e->last[i]);
    for (int i = 0; i < 4; i++) put(img, ENTRY_AT + 16 + (size_t)i * 4, e->entry[i]);
    for (int i = 0; i < STAND_COUNT; i++) put(img, STAND_AT + (size_t)i * 4, -1);
    if (!e->active) return;
    int n = 0, k = 0;
    for (int i = 0; i < e->groupCount && k < GROUPS_MAX; i++) {
        const ExpGroup *g = &e->group[i];
        if (!g->alive) continue;
        size_t o = GROUP_AT + (size_t)k++ * GROUP_SIZE;
        put(img, o, g->party);
        put(img, o + 4, g->number);
        put(img, o + 8, e->dungeon << 16 | e->floor << 8);
        put(img, o + 12, g->x);
        put(img, o + 16, g->y);
        put(img, o + 20, g->dir);
        int board = sa_battle_board_size(&s->battle, g->party);
        for (int c = 0; c < BATTLE_CELLS + 10; c++)
            put(img, o + (size_t)(GROUP_CELLS + c) * 4, board == 1 && c < BATTLE_CELLS && (c % 5 == 0 || c % 5 == 4) ? -2 : -1);
        int rows[BATTLE_CELLS], cols[BATTLE_CELLS], gr[BATTLE_CELLS], gc[BATTLE_CELLS], at[BATTLE_CELLS], count = 0, delta[BATTLE_CELLS][10], resists[BATTLE_CELLS][3];
        for (int c = 0; c < BATTLE_CELLS; c++) {
            at[c] = -1;
            int r = g->cells[c] >= 0 ? sa_party_unit_row(&s->party, g->cells[c]) : -1;
            if (r < 0) continue;
            at[c] = count;
            rows[count] = r;
            cols[count] = 34;
            gr[count] = c / 5;
            gc[count++] = c % 5;
        }
        sa_battle_position_bonus(&s->battle, count, rows, gr, gc, delta);
        area_resists(s, count, rows, gr, gc, resists);
        int team[TEAM_KINDS];
        team_bonus(s, count, rows, cols, team);
        for (int c = 0; c < BATTLE_CELLS; c++) {
            if (at[c] < 0 || n >= ENEMIES_MAX) continue;
            int r = rows[at[c]];
            Bench b = {0};
            b.unit = g->cells[c];
            b.level = sa_table_int(&s->party.units, r, 17) + s->vm.c[C_ENEMY_LEVEL];
            int v[10];
            sa_battle_unit_stats(&s->battle, r, b.level, v);
            b.hp = v[0];
            b.mp = v[1];
            size_t m = ENEMY_AT + (size_t)n * MEMBER_SIZE;
            write_unit(s, img, m, &b, true, delta[at[c]]);
            write_abilities(s, img, m, b.unit, PLACE_FIELD, team, resists[at[c]]);
            put(img, m + 4, -1);
            put(img, m + 8, ENEMY_BASE + n);
            put(img, m + 20, g->number);
            put(img, m + MEMBER_BYTES, g->number | 1 << 8 | c << 16);
            put(img, o + (size_t)(GROUP_CELLS + c) * 4, ENEMY_BASE + n);
            n++;
        }
        for (int t = 0; t < EXP_TARGETS; t++) {
            put(img, o + (size_t)(GROUP_TARGETS + t * 2) * 4, g->target[t][0]);
            put(img, o + (size_t)(GROUP_TARGETS + t * 2 + 1) * 4, g->target[t][1]);
        }
        put(img, o + GROUP_FLAGS * 4, g->flags);
        put(img, o + (GROUP_FLAGS + 1) * 4, g->flags == -5 ? -5 : -1);
        put(img, o + GROUP_CLOCK * 4, g->clock);
        put(img, o + GROUP_FOUGHT * 4, g->fought);
        put(img, o + GROUP_EXTRA * 4, g->extra);
        put(img, o + (GROUP_SIZE - 4), sa_exp_group_slot(s, g));
    }
}

static void read_dungeon(SA *s, const u8 *img) {
    Expedition *e = &s->exp;
    for (int i = 0; i < 4; i++) e->last[i] = at(img, LAST_AT + (size_t)i * 4);
    if (e->active && e->last[0] == (e->dungeon << 16 | e->floor << 8))
        for (int i = 0; i < 3; i++) s->vm.c[POS_VAR + i] = e->last[1 + i];
    for (int i = 0; i < 4; i++) e->entry[i] = at(img, ENTRY_AT + 16 + (size_t)i * 4);
    e->groupCount = 0;
    if (!e->active) return;
    for (int k = 0; k < GROUPS_MAX; k++) {
        size_t o = GROUP_AT + (size_t)k * GROUP_SIZE;
        if (!at(img, o)) break;
        ExpGroup *g = &e->group[e->groupCount++];
        memset(g, 0, sizeof *g);
        g->party = at(img, o);
        g->number = at(img, o + 4);
        g->x = at(img, o + 12);
        g->y = at(img, o + 16);
        g->dir = at(img, o + 20);
        g->alive = true;
        for (int c = 0; c < BATTLE_CELLS; c++) {
            int idx = at(img, o + (size_t)(GROUP_CELLS + c) * 4) - ENEMY_BASE;
            g->cells[c] = idx >= 0 && idx < ENEMIES_MAX ? at(img, ENEMY_AT + (size_t)idx * MEMBER_SIZE) : -1;
            if (g->cells[c] <= 0) g->cells[c] = -1;
        }
        for (int t = 0; t < EXP_TARGETS; t++) {
            g->target[t][0] = at(img, o + (size_t)(GROUP_TARGETS + t * 2) * 4);
            g->target[t][1] = at(img, o + (size_t)(GROUP_TARGETS + t * 2 + 1) * 4);
        }
        g->flags = at(img, o + GROUP_FLAGS * 4);
        g->clock = at(img, o + GROUP_CLOCK * 4);
        g->fought = at(img, o + GROUP_FOUGHT * 4);
        g->extra = at(img, o + GROUP_EXTRA * 4);
        for (int r = 0; r < s->battle.groups.rows; r++)
            if (sa_table_int(&s->battle.groups, r, 0) == g->party) g->stay = !strcmp(sa_table_text(&s->battle.groups, r, 7), "停止");
    }
}

bool sa_pcsave_write(SA *s, const char *path) {
    u8 *img = sa_alloc(IMAGE_SIZE);
    memset(img, 0, IMAGE_SIZE);
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
        put(img, o + 8, f->type ? f->built : -1);
        put(img, o + 12, f->type ? f->level : 0);
        put(img, o + 16, f->type ? f->exp : 0);
    }
    put(img, GRID_AFTER, 1);
    write_cache(s, img);
    int members = write_members(s, img);
    write_orders(s, img, members);
    write_maps(s, img);
    write_edits(s, img);
    write_dungeon(s, img);
    write_maze(s, img);
    write_box_slots(s, img);
    for (int i = 0; i < s->missions.logCount; i++)
        for (int k = 0; k < 3; k++) put(img, MISSIONS_AT + (size_t)i * 12 + (size_t)k * 4, s->missions.log[i][k]);
    for (int i = 0; i < PC_C_VARS; i++) put(img, VM_AT + (size_t)i * 4, s->vm.c[i]);
    memcpy(img + VM_AT + PC_C_VARS * 4, s->vm.f, PC_F_VARS);
    for (size_t i = 0; i < sizeof engineVars / sizeof engineVars[0]; i++)
        if (!s->vm.c[engineVars[i].off]) put(img, VM_AT + (size_t)engineVars[i].off * 4, engineVars[i].value);
    put(img, VM_AT + PLACE_VAR * 4, s->exp.active ? sa_exp_place_code(s) : PLACE_TOWN);
    if (s->exp.active)
        for (int i = 0; i < 3; i++) put(img, VM_AT + (size_t)(POS_VAR + i) * 4, at(img, LAST_AT + (size_t)(1 + i) * 4));
    put(img, VM_AT + (PLACE_VAR + 1) * 4, s->exp.active ? 2 : 1);
    if (!s->vm.c[SEED_VAR]) put(img, VM_AT + SEED_VAR * 4, (int)(sa_vm_random(&s->vm) & 0x7fffffff));
    sa_save_comment_put(s, img);
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
    sa_save_comment_get(img, out);
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
    out->steps = (int)rd32(c + C_PLACE_STEPS * 4);
    int weapon = at(img, MEMBER_AT + MEMBER_EQUIP);
    for (int i = 0; weapon && i < s->equip.items.rows; i++)
        if (sa_table_int(&s->equip.items, i, 0) == weapon) snprintf(out->weapon, sizeof out->weapon, "%s", sa_table_text(&s->equip.items, i, 1));
    free(img);
    return true;
}
