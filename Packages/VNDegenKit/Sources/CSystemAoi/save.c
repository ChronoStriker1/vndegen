#include "sa_private.h"
#include <time.h>

#define SAVE_MAGIC 0x41534e56u
#define SAVE_VERSION 13u
#define LEADER (PARTY_FRONT + 1)

typedef struct Writer {
    u8 *d;
    size_t n, cap;
} Writer;

static void put(Writer *w, const void *p, size_t n) {
    if (w->n + n > w->cap) {
        w->cap = (w->n + n) * 2 + 4096;
        w->d = sa_realloc(w->d, w->cap);
    }
    memcpy(w->d + w->n, p, n);
    w->n += n;
}

static void put32(Writer *w, u32 v) {
    u8 b[4];
    wr32(b, v);
    put(w, b, 4);
}

static void put_ints(Writer *w, const int *v, size_t count) {
    for (size_t i = 0; i < count; i++) put32(w, (u32)v[i]);
}

typedef struct Reader {
    const u8 *d;
    size_t n, p;
    bool bad;
} Reader;

static void get(Reader *r, void *out, size_t n) {
    if (r->p + n > r->n) {
        r->bad = true;
        memset(out, 0, n);
        return;
    }
    memcpy(out, r->d + r->p, n);
    r->p += n;
}

static u32 get32(Reader *r) {
    u8 b[4];
    get(r, b, 4);
    return rd32(b);
}

static void get_ints(Reader *r, int *v, size_t count) {
    for (size_t i = 0; i < count; i++) v[i] = (int)get32(r);
}

static char *slot_path(const SA *s, int slot) { return sa_fmt("%s/vndegen/slot%02d.sav", s->saveDir, slot); }

char *sa_pcsave_path(const SA *s, int slot) {
    if (slot >= SAVE_SLOTS) return sa_fmt("%s/Bunny3.sav/Bunny3A%d.dat", s->saveDir, slot - SAVE_SLOTS);
    return sa_fmt("%s/Bunny3.sav/Bunny3%02d.dat", s->saveDir, slot);
}

static void copy_name(char *out, const char *name) { snprintf(out, 48, "%s", name ? name : ""); }

void sa_save_current(SA *s, SaveSummary *out) {
    memset(out, 0, sizeof *out);
    out->used = true;
    out->time = (u32)time(NULL);
    out->turn = s->vm.c[C_TURN];
    for (int i = 0; i < 4; i++) out->town[i] = s->vm.c[C_RESIDENTS + i];
    out->level = s->party.level[LEADER] ? s->party.level[LEADER] : 1;
    const char *dungeon, *floor;
    sa_exp_place_names(s, &dungeon, &floor);
    copy_name(out->place[0], dungeon ? dungeon : "街");
    copy_name(out->place[1], floor);
    out->steps = s->vm.c[C_PLACE_STEPS];
    for (int i = 0; s->party.equip[LEADER] && i < s->equip.items.rows; i++)
        if (sa_table_int(&s->equip.items, i, 0) == s->party.equip[LEADER]) copy_name(out->weapon, sa_table_text(&s->equip.items, i, 1));
}

static void write_summary(Writer *w, SA *s, int count) {
    SaveSummary m;
    sa_save_current(s, &m);
    m.time = (u32)s->vm.c[2];
    put32(w, m.time);
    put32(w, (u32)m.turn);
    put_ints(w, m.town, 4);
    put32(w, (u32)m.level);
    put32(w, (u32)count);
    put(w, m.place, sizeof m.place);
    put(w, m.weapon, sizeof m.weapon);
    put32(w, (u32)m.steps);
}

static void read_summary(Reader *r, SaveSummary *out) {
    out->time = get32(r);
    out->turn = (int)get32(r);
    get_ints(r, out->town, 4);
    out->level = (int)get32(r);
    out->count = (int)get32(r);
    get(r, out->place, sizeof out->place);
    get(r, out->weapon, sizeof out->weapon);
    out->steps = (int)get32(r);
    out->place[0][47] = out->place[1][47] = out->weapon[47] = 0;
}

static bool write_sidecar(SA *s, int slot, int count);

bool sa_save_write(SA *s, int slot, int count) {
    u32 now = (u32)time(NULL);
    s->vm.c[2] = s->vm.c[13] = (int)now;
    s->vm.c[3] = count;
    s->vm.c[11] = (int)(s->playMs / 1000);
    char *dir = sa_fmt("%s/Bunny3.sav", s->saveDir);
    sa_mkdirs(dir);
    free(dir);
    char *path = sa_pcsave_path(s, slot);
    bool ok = sa_pcsave_write(s, path);
    free(path);
    return ok && write_sidecar(s, slot, count);
}

static bool write_sidecar(SA *s, int slot, int count) {
    Writer w = {0};
    put32(&w, SAVE_MAGIC);
    put32(&w, SAVE_VERSION);
    write_summary(&w, s, count);
    put_ints(&w, s->vm.c, SA_C_VARS);
    put(&w, s->vm.f, SA_F_VARS);
    put(&w, s->vm.g, SA_G_VARS);
    for (int c = 0; c < TOWN_CELLS; c++) {
        put32(&w, (u32)s->town.cell[c].type);
        put32(&w, (u32)s->town.cell[c].level);
        put32(&w, (u32)s->town.cell[c].exp);
        put32(&w, (u32)s->town.cell[c].built);
    }
    put32(&w, (u32)s->town.stage);
    put32(&w, (u32)s->town.condition);
    const Party *p = &s->party;
    put_ints(&w, p->unit, PARTY_SLOTS);
    put_ints(&w, p->hp, PARTY_SLOTS);
    put_ints(&w, p->mp, PARTY_SLOTS);
    put_ints(&w, p->level, PARTY_SLOTS);
    put_ints(&w, p->exp, PARTY_SLOTS);
    put_ints(&w, p->equip, PARTY_SLOTS);
    put_ints(&w, p->sp, PARTY_SLOTS);
    for (int i = 0; i < PARTY_SLOTS; i++) put_ints(&w, p->trained[i], 10);
    for (int i = 0; i < PARTY_SLOTS; i++) put_ints(&w, &p->order[i].mode, 3);
    for (int k = 0; k < PARTY_PRESETS; k++) {
        put32(&w, p->presetSaved[k]);
        for (int i = 0; i < PARTY_SLOTS; i++) put_ints(&w, &p->preset[k][i].mode, 3);
    }
    put32(&w, (u32)p->items);
    put_ints(&w, p->itemId, (size_t)p->items);
    put_ints(&w, p->itemCount, (size_t)p->items);
    put32(&w, (u32)p->rosterCount);
    put_ints(&w, p->roster, (size_t)p->rosterCount);
    put32(&w, (u32)p->benchCount);
    for (int i = 0; i < p->benchCount; i++) {
        const Bench *b = &p->bench[i];
        const int v[8] = {b->unit, b->hp, b->mp, b->level, b->exp, b->equip, b->sp, b->tile};
        put_ints(&w, v, 8);
        put_ints(&w, b->trained, 10);
    }
    put(&w, s->exp.explored, sizeof(*s->exp.explored) * EXP_DUNGEONS);
    put(&w, s->exp.passages, sizeof(*s->exp.passages) * EXP_DUNGEONS);
    for (int d = 0; d < EXP_DUNGEONS; d++) put_ints(&w, &s->exp.floorInfo[d][0][0], EXP_FLOORS * 4);
    const int exp[3] = {s->exp.active, s->exp.dungeon, s->exp.floor};
    put_ints(&w, exp, 3);
    put_ints(&w, s->party.post, PARTY_POSTS);
    put32(&w, (u32)s->missions.logCount);
    put_ints(&w, &s->missions.log[0][0], MISSION_LOG * 3);
    put_ints(&w, s->exp.last, 4);
    put_ints(&w, s->exp.entry, 4);
    put32(&w, (u32)s->exp.groupCount);
    for (int i = 0; i < s->exp.groupCount; i++) {
        const ExpGroup *g = &s->exp.group[i];
        const int v[11] = {g->alive, g->party, g->number, g->x, g->y, g->dir, g->flags, g->cooldown, g->fought, g->extra, g->stay};
        put_ints(&w, v, 11);
        put_ints(&w, g->cells, BATTLE_CELLS);
        put_ints(&w, &g->target[0][0], EXP_TARGETS * 2);
    }
    put_ints(&w, &s->maze.event[0][0][0], sizeof s->maze.event / sizeof(int));
    put_ints(&w, s->exp.special, EXP_SPECIALS);
    put_ints(&w, s->exp.specialValue, EXP_SPECIALS);
    put32(&w, (u32)s->vm.boxCount);
    for (int i = 0; i < s->vm.boxCount; i++) put(&w, s->vm.boxes[i].file, sizeof s->vm.boxes[i].file);
    char *dir = sa_fmt("%s/vndegen", s->saveDir);
    sa_mkdirs(dir);
    free(dir);
    char *path = slot_path(s, slot);
    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(w.d, 1, w.n, f) == w.n;
    if (f) fclose(f);
    sa_log(s, "save slot %d: %s (%zu bytes)", slot, ok ? "written" : "failed", w.n);
    free(path);
    free(w.d);
    return ok;
}

void sa_save_delete(SA *s, int slot) {
    char *path = sa_pcsave_path(s, slot), *side = slot_path(s, slot);
    remove(path);
    remove(side);
    sa_log(s, "save slot %d: deleted", slot);
    free(path);
    free(side);
}

bool sa_save_summary(SA *s, int slot, SaveSummary *out) {
    char *path = sa_pcsave_path(s, slot);
    bool ok = sa_pcsave_summary(s, path, out);
    free(path);
    return ok;
}

static u32 sidecar_time(SA *s, int slot) {
    char *path = slot_path(s, slot);
    size_t n;
    u8 *d = sa_read_file(path, &n);
    free(path);
    if (!d) return 0;
    Reader r = {d, n, 0, false};
    u32 when = get32(&r) == SAVE_MAGIC && get32(&r) == SAVE_VERSION ? get32(&r) : 0;
    free(d);
    return r.bad ? 0 : when;
}

static bool read_sidecar(SA *s, int slot);

bool sa_save_read(SA *s, int slot) {
    SaveSummary pc;
    if (!sa_save_summary(s, slot, &pc)) return false;
    bool ok;
    if (sidecar_time(s, slot) == pc.time) ok = read_sidecar(s, slot);
    else {
        char *path = sa_pcsave_path(s, slot);
        ok = sa_pcsave_read(s, path);
        free(path);
    }
    return ok;
}

static bool read_sidecar(SA *s, int slot) {
    char *path = slot_path(s, slot);
    size_t n;
    u8 *d = sa_read_file(path, &n);
    free(path);
    if (!d) return false;
    Reader r = {d, n, 0, false};
    if (get32(&r) != SAVE_MAGIC || get32(&r) != SAVE_VERSION) {
        free(d);
        return false;
    }
    SaveSummary skip;
    read_summary(&r, &skip);
    get_ints(&r, s->vm.c, SA_C_VARS);
    get(&r, s->vm.f, SA_F_VARS);
    get(&r, s->vm.g, SA_G_VARS);
    for (int c = 0; c < TOWN_CELLS; c++) {
        s->town.cell[c].type = (int)get32(&r);
        s->town.cell[c].level = (int)get32(&r);
        s->town.cell[c].exp = (int)get32(&r);
        s->town.cell[c].built = (int)get32(&r);
    }
    s->town.stage = (int)get32(&r);
    s->town.condition = (int)get32(&r);
    Party *p = &s->party;
    int unit[PARTY_SLOTS];
    get_ints(&r, unit, PARTY_SLOTS);
    for (int i = 0; i < PARTY_SLOTS; i++) sa_party_place(p, &s->vfs, i, unit[i]);
    get_ints(&r, p->hp, PARTY_SLOTS);
    get_ints(&r, p->mp, PARTY_SLOTS);
    get_ints(&r, p->level, PARTY_SLOTS);
    get_ints(&r, p->exp, PARTY_SLOTS);
    get_ints(&r, p->equip, PARTY_SLOTS);
    get_ints(&r, p->sp, PARTY_SLOTS);
    for (int i = 0; i < PARTY_SLOTS; i++) get_ints(&r, p->trained[i], 10);
    for (int i = 0; i < PARTY_SLOTS; i++) get_ints(&r, &p->order[i].mode, 3);
    for (int k = 0; k < PARTY_PRESETS; k++) {
        p->presetSaved[k] = get32(&r) != 0;
        for (int i = 0; i < PARTY_SLOTS; i++) get_ints(&r, &p->preset[k][i].mode, 3);
    }
    p->items = (int)get32(&r);
    if (p->items < 0 || p->items > PARTY_ITEMS) r.bad = true, p->items = 0;
    get_ints(&r, p->itemId, (size_t)p->items);
    get_ints(&r, p->itemCount, (size_t)p->items);
    p->rosterCount = (int)get32(&r);
    if (p->rosterCount < 0 || p->rosterCount > PARTY_ROSTER) r.bad = true, p->rosterCount = 0;
    get_ints(&r, p->roster, (size_t)p->rosterCount);
    p->benchCount = (int)get32(&r);
    if (p->benchCount < 0 || p->benchCount > PARTY_BENCH) r.bad = true, p->benchCount = 0;
    for (int i = 0; i < p->benchCount; i++) {
        Bench *b = &p->bench[i];
        int v[8];
        get_ints(&r, v, 8);
        b->unit = v[0], b->hp = v[1], b->mp = v[2], b->level = v[3], b->exp = v[4], b->equip = v[5], b->sp = v[6], b->tile = v[7];
        get_ints(&r, b->trained, 10);
    }
    get(&r, s->exp.explored, sizeof(*s->exp.explored) * EXP_DUNGEONS);
    get(&r, s->exp.passages, sizeof(*s->exp.passages) * EXP_DUNGEONS);
    for (int d = 0; d < EXP_DUNGEONS; d++) get_ints(&r, &s->exp.floorInfo[d][0][0], EXP_FLOORS * 4);
    int exp[3];
    get_ints(&r, exp, 3);
    s->exp.active = exp[0] != 0;
    s->exp.dungeon = exp[1];
    s->exp.floor = exp[2];
    get_ints(&r, s->party.post, PARTY_POSTS);
    s->missions.logCount = (int)get32(&r);
    if (s->missions.logCount < 0 || s->missions.logCount > MISSION_LOG) r.bad = true, s->missions.logCount = 0;
    get_ints(&r, &s->missions.log[0][0], MISSION_LOG * 3);
    get_ints(&r, s->exp.last, 4);
    get_ints(&r, s->exp.entry, 4);
    s->exp.groupCount = (int)get32(&r);
    if (s->exp.groupCount < 0 || s->exp.groupCount > EXP_GROUPS) r.bad = true, s->exp.groupCount = 0;
    for (int i = 0; i < s->exp.groupCount; i++) {
        ExpGroup *g = &s->exp.group[i];
        int v[11];
        get_ints(&r, v, 11);
        g->alive = v[0] != 0, g->party = v[1], g->number = v[2], g->x = v[3], g->y = v[4], g->dir = v[5], g->flags = v[6], g->cooldown = v[7], g->fought = v[8];
        g->extra = v[9], g->stay = v[10] != 0;
        get_ints(&r, g->cells, BATTLE_CELLS);
        get_ints(&r, &g->target[0][0], EXP_TARGETS * 2);
    }
    get_ints(&r, &s->maze.event[0][0][0], sizeof s->maze.event / sizeof(int));
    get_ints(&r, s->exp.special, EXP_SPECIALS);
    get_ints(&r, s->exp.specialValue, EXP_SPECIALS);
    int boxes = (int)get32(&r);
    for (int i = 0; i < boxes && i < SA_BOXES && !r.bad; i++) {
        char file[sizeof s->vm.boxes[0].file];
        get(&r, file, sizeof file);
        file[sizeof file - 1] = 0;
        if (!r.bad && *file) sa_vm_register_hooks(&s->vm, &s->vfs, file);
    }
    free(d);
    sa_log(s, "load slot %d: %s", slot, r.bad ? "short" : "ok");
    return !r.bad;
}
