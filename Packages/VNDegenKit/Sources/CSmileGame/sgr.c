#include "sg.h"

bool sgr_parse(Sgr *s, u8 *data, size_t size) {
    memset(s, 0, sizeof *s);
    s->data = data;
    s->size = size;
    if (!data || size < 0x11 || memcmp(data, "YUKAR", 5) != 0) return false;
    s->version = (i32)((u32)data[5] | (u32)data[6] << 8 | (u32)data[7] << 16 | (u32)data[8] << 24);
    size_t o = s->version >= 10 ? 0x15 : 0x11;
    int cap = 16;
    s->recs = sg_alloc(sizeof(SgrRec) * (size_t)cap);
    while (o + 6 <= size) {
        u32 n = (u32)data[o] | (u32)data[o + 1] << 8 | (u32)data[o + 2] << 16 | (u32)data[o + 3] << 24;
        if (n > size - o - 6) return false;
        if (s->count == cap) {
            cap *= 2;
            s->recs = sg_realloc(s->recs, sizeof(SgrRec) * (size_t)cap);
        }
        s->recs[s->count++] = (SgrRec){data[o + 4], data[o + 5], data + o + 6, n};
        o += 6 + n;
    }
    return o == size;
}

void sgr_free(Sgr *s) {
    free(s->data);
    free(s->recs);
    memset(s, 0, sizeof *s);
}

static char **name_list(Rd *r, int *count) {
    i32 n = rd_i32(r);
    if (n < 0 || n > 100000) {
        r->bad = true;
        n = 0;
    }
    char **names = sg_alloc(sizeof(char *) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r->bad; i++) {
        rd_i32(r);
        names[i] = rd_str(r);
    }
    *count = n;
    return names;
}

static void skip_strs(Rd *r, int n) {
    for (int i = 0; i < n; i++) free(rd_str(r));
}

bool settings_parse(Settings *s, const SgrRec *rec) {
    memset(s, 0, sizeof *s);
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    s->name = rd_str(&r);
    i32 party = rd_i32(&r);
    for (int i = 0; i < party; i++) {
        Guid g = rd_guid(&r);
        if (i < 4) s->party[i] = g;
    }
    s->partyCount = party < 4 ? party : 4;
    rd_skip(&r, 8);
    s->startX = rd_i32(&r);
    s->startZ = rd_i32(&r);
    s->startMap = rd_guid(&r);
    s->switchNames = name_list(&r, &s->switchCount);
    s->varNames = name_list(&r, &s->varCount);
    s->windowImage = rd_guid(&r);
    rd_skip(&r, 16 * 5 + 2);
    skip_strs(&r, 8);
    skip_strs(&r, rd_i32(&r));
    rd_guid(&r);
    skip_strs(&r, 50);
    rd_guid(&r);
    skip_strs(&r, 23);
    rd_skip(&r, 16 * 10 + 16);
    s->titleOverlay = rd_guid(&r);
    rd_skip(&r, 1);
    free(rd_str(&r));
    rd_skip(&r, 28);
    s->titleImage = rd_guid(&r);
    s->titleBgm = rd_guid(&r);
    rd_skip(&r, 16);
    for (int i = 0; i < 3; i++) s->titleItems[i] = rd_str(&r);
    s->titleLayout = rd_i32(&r);
    for (int i = 0; i < 4; i++) s->titlePlace[i] = rd_u8(&r);
    char *color = rd_str(&r);
    s->titleColor = color_parse(color);
    free(color);
    rd_skip(&r, 8);
    s->titleCursor = rd_guid(&r);
    s->logo = rd_guid(&r);
    if (rec->size >= 8) {
        Rd t = rd_make(rec->body + rec->size - 8, 8);
        s->screenW = rd_i32(&t);
        s->screenH = rd_i32(&t);
    }
    if (s->screenW < 320 || s->screenW > 4096 || s->screenH < 240 || s->screenH > 4096) {
        s->screenW = 960;
        s->screenH = 544;
    }
    return !r.bad;
}

void settings_free(Settings *s) {
    free(s->name);
    for (int i = 0; i < s->switchCount; i++) free(s->switchNames[i]);
    for (int i = 0; i < s->varCount; i++) free(s->varNames[i]);
    free(s->switchNames);
    free(s->varNames);
    for (int i = 0; i < 3; i++) free(s->titleItems[i]);
    memset(s, 0, sizeof *s);
}

bool map_parse(Map *m, const SgrRec *rec) {
    memset(m, 0, sizeof *m);
    Rd r = rd_make(rec->body, rec->size);
    m->id = rd_guid(&r);
    m->name = rd_str(&r);
    rd_i32(&r);
    free(rd_str(&r));
    rd_i64(&r);
    i32 n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    m->events = sg_alloc(sizeof(MapEventRef) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        m->events[i].x = rd_i32(&r);
        m->events[i].z = rd_i32(&r);
        m->events[i].id = rd_guid(&r);
    }
    m->eventCount = n;
    m->encounterRate = rd_i32(&r);
    n = rd_i32(&r);
    if (n < 0 || n > 10000) return false;
    rd_skip(&r, 24 * (size_t)n);
    rd_skip(&r, 36);
    m->w = rd_i32(&r);
    m->h = rd_i32(&r);
    if (r.bad || m->w <= 0 || m->h <= 0 || m->w > 4096 || m->h > 4096) return false;
    size_t cells = (size_t)m->w * (size_t)m->h;
    m->height = sg_alloc(sizeof(i16) * cells);
    m->chip = sg_alloc(sizeof(i16) * cells);
    for (size_t i = 0; i < cells; i++) m->height[i] = rd_i16(&r);
    for (size_t i = 0; i < cells; i++) m->chip[i] = rd_i16(&r);
    n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    m->objects = sg_alloc(sizeof(MapObject) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        m->objects[i].x = rd_i32(&r);
        m->objects[i].z = rd_i32(&r);
        m->objects[i].dir = rd_i32(&r);
        m->objects[i].id = rd_guid(&r);
    }
    m->objectCount = n;
    n = rd_i32(&r);
    if (n < 0 || n > 100000) return false;
    m->stairs = sg_alloc(sizeof(MapStair) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        m->stairs[i].x = rd_i32(&r);
        m->stairs[i].z = rd_i32(&r);
        m->stairs[i].dir = rd_i32(&r);
        m->stairs[i].kind = rd_i32(&r);
    }
    m->stairCount = n;
    for (int i = 0; i < 8; i++) m->colors[i] = rd_i32(&r);
    m->env = rd_guid(&r);
    m->bgm = rd_guid(&r);
    rd_skip(&r, 48 + 4 + 2);
    n = rd_i32(&r);
    if (n < 0 || n > 4096) return false;
    m->chips = sg_alloc(sizeof(MapChip) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        m->chips[i].index = rd_i32(&r);
        m->chips[i].id = rd_guid(&r);
    }
    m->chipCount = n;
    return !r.bad;
}

void map_free(Map *m) {
    free(m->name);
    free(m->events);
    free(m->height);
    free(m->chip);
    free(m->objects);
    free(m->stairs);
    free(m->chips);
    memset(m, 0, sizeof *m);
}

Guid map_chip_guid(const Map *m, int index) {
    for (int i = 0; i < m->chipCount; i++)
        if (m->chips[i].index == index) return m->chips[i].id;
    return (Guid){{0}};
}

static void codes_of(u32 mask, u8 *codes, int *n) {
    while (mask && *n < 64) {
        codes[(*n)++] = (u8)(mask & 3);
        mask >>= 2;
    }
}

bool script_parse(Script *s, const SgrRec *rec) {
    memset(s, 0, sizeof *s);
    Rd r = rd_make(rec->body, rec->size);
    s->id = rd_guid(&r);
    s->flag = rd_u8(&r);
    s->trigger = rd_i32(&r);
    i32 n = rd_i32(&r);
    if (r.bad || n < 0 || n > 100000) return false;
    s->cmds = sg_alloc(sizeof(Cmd) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        Cmd *c = &s->cmds[i];
        c->type = rd_i32(&r);
        c->nest = rd_i32(&r);
        u32 mask = (u32)rd_i32(&r);
        u8 codes[64];
        int nc = 0;
        if (mask >> 28 == 4) {
            u32 more = (u32)rd_i32(&r);
            codes_of(more, codes, &nc);
            codes_of(mask & 0x0fffffffu, codes, &nc);
        } else {
            codes_of(mask, codes, &nc);
        }
        c->args = sg_alloc(sizeof(CmdArg) * (size_t)(nc ? nc : 1));
        for (int k = 0; k < nc && !r.bad; k++) {
            CmdArg *a = &c->args[k];
            a->kind = codes[k];
            if (a->kind == ARG_INT)
                a->i = rd_i32(&r);
            else if (a->kind == ARG_GUID)
                a->g = rd_guid(&r);
            else if (a->kind == ARG_STR)
                a->s = rd_str(&r);
            else
                r.bad = true;
            c->argCount = k + 1;
        }
        s->count = i + 1;
    }
    s->tail[0] = rd_u8(&r);
    s->tail[1] = rd_u8(&r);
    return !r.bad && r.p == r.end;
}

void script_free(Script *s) {
    for (int i = 0; i < s->count; i++) {
        for (int k = 0; k < s->cmds[i].argCount; k++) free(s->cmds[i].args[k].s);
        free(s->cmds[i].args);
    }
    free(s->cmds);
    memset(s, 0, sizeof *s);
}

i32 cmd_int(const Cmd *c, int i) { return i < c->argCount && c->args[i].kind == ARG_INT ? c->args[i].i : 0; }

Guid cmd_guid(const Cmd *c, int i) { return i < c->argCount && c->args[i].kind == ARG_GUID ? c->args[i].g : (Guid){{0}}; }

const char *cmd_str(const Cmd *c, int i) { return i < c->argCount && c->args[i].kind == ARG_STR ? c->args[i].s : ""; }

bool event_parse(EventDef *e, const SgrRec *rec) {
    memset(e, 0, sizeof *e);
    Rd r = rd_make(rec->body, rec->size);
    e->id = rd_guid(&r);
    e->name = rd_str(&r);
    i32 n = rd_i32(&r);
    if (r.bad || n < 0 || n > 1000) return false;
    e->sheets = sg_alloc(sizeof(EventSheet) * (size_t)(n ? n : 1));
    for (int i = 0; i < n && !r.bad; i++) {
        EventSheet *s = &e->sheets[i];
        i32 conds = rd_i32(&r);
        if (conds < 0 || conds > 64) {
            r.bad = true;
            break;
        }
        for (int k = 0; k < conds; k++) {
            EventCond c = {rd_i32(&r), rd_i32(&r), rd_i32(&r), rd_i32(&r)};
            rd_skip(&r, 16);
            if (k < 8) s->conds[s->condCount++] = c;
        }
        free(rd_str(&r));
        s->graphic = rd_guid(&r);
        s->direction = rd_i32(&r);
        rd_skip(&r, 18);
        s->script = rd_guid(&r);
        free(rd_str(&r));
    }
    if (r.bad) e->sheetCount = 0;
    else e->sheetCount = n;
    return e->sheetCount > 0;
}

void event_free(EventDef *e) {
    free(e->name);
    free(e->sheets);
    memset(e, 0, sizeof *e);
}
