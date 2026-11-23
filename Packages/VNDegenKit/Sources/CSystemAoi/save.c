#include "sa_private.h"
#include <time.h>

#define SAVE_MAGIC 0x41534e56u
#define SAVE_VERSION 1u

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

static void write_summary(Writer *w, const SA *s) {
    put32(w, (u32)time(NULL));
    put32(w, (u32)s->vm.c[C_TURN]);
    for (int i = 0; i < 4; i++) put32(w, (u32)s->vm.c[C_RESIDENTS + i]);
    put32(w, (u32)(s->party.level[PARTY_FRONT + 1] ? s->party.level[PARTY_FRONT + 1] : 1));
}

bool sa_save_write(SA *s, int slot) {
    Writer w = {0};
    put32(&w, SAVE_MAGIC);
    put32(&w, SAVE_VERSION);
    write_summary(&w, s);
    put_ints(&w, s->vm.c, SA_C_VARS);
    put(&w, s->vm.f, SA_F_VARS);
    put(&w, s->vm.g, SA_G_VARS);
    for (int c = 0; c < TOWN_CELLS; c++) {
        put32(&w, (u32)s->town.cell[c].type);
        put32(&w, (u32)s->town.cell[c].level);
        put32(&w, (u32)s->town.cell[c].exp);
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
        const int v[7] = {b->unit, b->hp, b->mp, b->level, b->exp, b->equip, b->sp};
        put_ints(&w, v, 7);
        put_ints(&w, b->trained, 10);
    }
    put(&w, s->exp.explored, sizeof(*s->exp.explored) * EXP_DUNGEONS);
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

bool sa_save_summary(SA *s, int slot, SaveSummary *out) {
    memset(out, 0, sizeof *out);
    char *path = slot_path(s, slot);
    size_t n;
    u8 *d = sa_read_file(path, &n);
    free(path);
    if (!d) return false;
    Reader r = {d, n, 0, false};
    bool ok = get32(&r) == SAVE_MAGIC && get32(&r) == SAVE_VERSION;
    if (ok) {
        out->time = get32(&r);
        out->turn = (int)get32(&r);
        for (int i = 0; i < 4; i++) out->town[i] = (int)get32(&r);
        out->level = (int)get32(&r);
        out->used = !r.bad;
    }
    free(d);
    return out->used;
}

bool sa_save_read(SA *s, int slot) {
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
    for (int i = 0; i < 7; i++) get32(&r);
    get_ints(&r, s->vm.c, SA_C_VARS);
    get(&r, s->vm.f, SA_F_VARS);
    get(&r, s->vm.g, SA_G_VARS);
    for (int c = 0; c < TOWN_CELLS; c++) {
        s->town.cell[c].type = (int)get32(&r);
        s->town.cell[c].level = (int)get32(&r);
        s->town.cell[c].exp = (int)get32(&r);
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
        int v[7];
        get_ints(&r, v, 7);
        b->unit = v[0], b->hp = v[1], b->mp = v[2], b->level = v[3], b->exp = v[4], b->equip = v[5], b->sp = v[6];
        get_ints(&r, b->trained, 10);
    }
    get(&r, s->exp.explored, sizeof(*s->exp.explored) * EXP_DUNGEONS);
    free(d);
    sa_log(s, "load slot %d: %s", slot, r.bad ? "short" : "ok");
    return !r.bad;
}
