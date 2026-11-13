#include "yr.h"
#include "vm.h"

typedef struct Out {
    char *p;
    size_t n, cap;
} Out;

static void emit(Out *o, const char *s, size_t n) {
    if (o->n + n + 1 > o->cap) {
        o->cap = (o->n + n + 1) * 2;
        o->p = yr_realloc(o->p, o->cap);
    }
    memcpy(o->p + o->n, s, n);
    o->n += n;
    o->p[o->n] = 0;
}

static void emit_byte(Out *o, u32 v) {
    char c[2] = { (char)('a' + ((v >> 4) & 15)), (char)('a' + (v & 15)) };
    emit(o, c, 2);
}

static void emit_u32(Out *o, u32 v) {
    for (int s = 24; s >= 0; s -= 8) emit_byte(o, v >> s);
}

static void enclosing_ifs(YR *y, u16 script, u32 ip, u32 *ifs, u32 *ends, int *n, int max) {
    YScript *s = yr_script(y, script);
    if (!s) return;
    for (u32 i = 0; i < s->count && i < ip && *n < max; i++) {
        if (y->cmds[s->ins[i].cmd].id != C_IF) continue;
        u32 end = i;
        for (int guard = 0; guard < 100000; guard++) {
            u32 next = s->ins[end].jump;
            if (!next || next >= s->count) break;
            end = next;
            if (y->cmds[s->ins[end].cmd].id == C_IFEND) break;
        }
        if (y->cmds[s->ins[end].cmd].id != C_IFEND || end < ip) continue;
        ifs[*n] = i;
        ends[*n] = end;
        (*n)++;
    }
}

char *yr_scriptpos_get(YR *y, YTask *t) {
    enum { MAXIF = 256 };
    u32 ifs[MAXIF], ends[MAXIF];
    u8 ifLoop[MAXIF];
    int nif = 0, ifsAfter[64];
    int depth = t->depth < 64 ? t->depth : 64;
    for (int d = 1; d <= depth; d++) {
        u16 sc = d < t->depth ? t->frames[d].retScript : t->script;
        u32 ip = d < t->depth ? t->frames[d].retIp : t->ip;
        int before = nif;
        enclosing_ifs(y, sc, ip, ifs, ends, &nif, MAXIF);
        int lo = d > 1 ? t->frames[d - 1].loopBase : 0, hi = d < t->depth ? t->frames[d].loopBase : t->loopDepth;
        for (int i = before; i < nif; i++) {
            int k = lo;
            while (k < hi && t->loops[k].start < ifs[i]) k++;
            ifLoop[i] = (u8)k;
        }
        ifsAfter[d - 1] = nif;
    }
    Out o = { 0 };
    static const char magic[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    emit(&o, magic, 8);
    emit_byte(&o, (u32)nif);
    for (int i = 0; i < nif; i++) {
        emit_byte(&o, ifLoop[i]);
        emit_u32(&o, ifs[i]);
        emit_u32(&o, ends[i]);
    }
    bool old = yr_save_version(y) < 400;
    if (old) emit_byte(&o, (u32)(t->depth - 1) >> 8);
    emit_byte(&o, (u32)(t->depth - 1));
    for (int d = 1; d < t->depth; d++) {
        emit_byte(&o, d < 64 ? (u32)ifsAfter[d - 1] : 0);
        emit_byte(&o, (u32)t->frames[d].loopBase);
        emit_u32(&o, t->frames[d].retIp);
        emit_u32(&o, t->frames[d].retScript);
    }
    emit_byte(&o, (u32)t->loopDepth);
    for (int i = 0; i < t->loopDepth; i++) {
        YLoop *l = &t->loops[i];
        emit_byte(&o, 0);
        emit_u32(&o, l->start + 1);
        emit_u32(&o, l->end);
        emit_u32(&o, (u32)l->lc);
        emit_u32(&o, (u32)l->count);
    }
    if (old) emit_byte(&o, 0);
    emit_u32(&o, 0);
    emit_u32(&o, t == y->cur ? t->ip + 1 : t->ip);
    const char *name = t->script < y->scriptCount ? y->scripts[t->script].name : "";
    emit_u32(&o, (u32)strlen(name));
    emit(&o, name, strlen(name));
    return o.p;
}

typedef struct In {
    const u8 *p, *end;
    bool bad;
} In;

static u32 read_byte(In *in) {
    if (in->p + 2 > in->end || in->p[0] < 'a' || in->p[0] > 'p' || in->p[1] < 'a' || in->p[1] > 'p') {
        in->bad = true;
        return 0;
    }
    u32 v = (u32)(in->p[0] - 'a') << 4 | (u32)(in->p[1] - 'a');
    in->p += 2;
    return v;
}

static u32 read_u32(In *in) {
    u32 v = 0;
    for (int i = 0; i < 4; i++) v = v << 8 | read_byte(in);
    return v;
}

bool yr_scriptpos_set(YR *y, YTask *t, const char *pos) {
    size_t n = strlen(pos);
    if (n < 8 || memcmp(pos, "\x01\x02\x03\x04\x05\x06\x07\x08", 8)) return false;
    In in = { (const u8 *)pos + 8, (const u8 *)pos + n, false };
    u32 nif = read_byte(&in);
    for (u32 i = 0; i < nif && !in.bad; i++) { read_byte(&in); read_u32(&in); read_u32(&in); }
    bool old = yr_save_version(y) < 400;
    u32 nframes = read_byte(&in);
    if (old) nframes = nframes << 8 | read_byte(&in);
    YFrame frames[256];
    for (u32 i = 0; i < nframes && i < 256 && !in.bad; i++) {
        memset(&frames[i], 0, sizeof frames[i]);
        read_byte(&in);
        frames[i].loopBase = (int)read_byte(&in);
        frames[i].retIp = read_u32(&in);
        frames[i].retScript = (u16)read_u32(&in);
    }
    u32 nloops = read_byte(&in);
    YLoop loops[256];
    for (u32 i = 0; i < nloops && i < 256 && !in.bad; i++) {
        read_byte(&in);
        u32 start = read_u32(&in), end = read_u32(&in), lc = read_u32(&in), count = read_u32(&in);
        loops[i] = (YLoop){ 0, start ? start - 1 : 0, end, (i64)(int32_t)count, (i64)lc };
    }
    if (old) read_byte(&in);
    read_u32(&in);
    u32 ip = read_u32(&in), len = read_u32(&in);
    if (in.bad || nframes > 255 || nloops > 255 || in.p + len > in.end) return false;
    int script = -1;
    for (u32 i = 0; i < y->scriptCount; i++)
        if (strlen(y->scripts[i].name) == len && !strncasecmp(y->scripts[i].name, (const char *)in.p, len)) { script = (int)i; break; }
    if (script < 0) return false;
    for (u32 i = 0; i < nframes; i++) {
        if (frames[i].retScript >= y->scriptCount) return false;
        int floor = i ? frames[i - 1].loopBase : 0;
        if (frames[i].loopBase < floor) frames[i].loopBase = floor;
        if (frames[i].loopBase > (int)nloops) frames[i].loopBase = (int)nloops;
    }
    while (t->depth > 1) {
        YFrame *f = &t->frames[--t->depth];
        for (int k = 0; k < YR_PARAMS; k++) { free(f->params.s[k]); free(f->results.s[k]); }
    }
    for (u32 i = 0; i < nframes; i++) {
        if (t->depth == t->frameCap) {
            t->frameCap = t->frameCap ? t->frameCap * 2 : 16;
            t->frames = yr_realloc(t->frames, sizeof(YFrame) * (size_t)t->frameCap);
        }
        t->frames[t->depth++] = frames[i];
    }
    if ((int)nloops > t->loopCap) {
        t->loopCap = (int)nloops;
        t->loops = yr_realloc(t->loops, sizeof(YLoop) * (size_t)t->loopCap);
    }
    for (u32 i = 0; i < nloops; i++) {
        u16 owner = (u16)script;
        for (u32 d = 0; d < nframes; d++)
            if ((u32)frames[d].loopBase > i) { owner = frames[d].retScript; break; }
        loops[i].script = owner;
        t->loops[i] = loops[i];
    }
    t->loopDepth = (int)nloops;
    t->script = (u16)script;
    t->ip = ip;
    return true;
}
