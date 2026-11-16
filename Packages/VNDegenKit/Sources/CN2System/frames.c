#include "n2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define NPS_TAG 0xf1f2b5c5u
#define FRAME_REC 0x7800
#define IDLE_CALLBACK 0x1001u

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static bool call_is(const char *call, const char *fn) {
    size_t n = strlen(fn);
    return call && strncasecmp(call, fn, n) == 0 && call[n] == '(';
}

static void put_str(Buf *b, const char *utf8) {
    size_t len = 0;
    char *s = n2_utf8_to_sjis(utf8 ? utf8 : "", &len);
    buf_put(b, s, len);
    free(s);
    u8 z[4] = {0};
    buf_put(b, z, 4);
}

static void put_i32(Buf *b, i32 v) { buf_u32(b, (u32)v); }

static void put_f64(Buf *b, double v) { buf_put(b, &v, 8); }

static void put_u16(Buf *b, int v) {
    u8 p[2] = {(u8)v, (u8)(v >> 8)};
    buf_put(b, p, 2);
}

static void put_geom(Buf *b, int x, int y, int w, int h, int zx, int zy, int rz, bool window) {
    int hw = w / 2, hh = h / 2;
    buf_u32(b, 4);
    double quad[12] = {-hw, hh, 0, hw, hh, 0, hw, hh + 1 - h, 0, -hw, hh + 1 - h, 0};
    for (int i = 0; i < 12; i++) put_f64(b, quad[i]);
    put_i32(b, w);
    put_i32(b, h);
    int sx[4] = {0, w - 1, w - 1, 0}, sy[4] = {0, 0, h - 1, h - 1};
    int dx[4] = {x, x + 2 * hw, x + 2 * hw, x}, dy[4] = {y, y, y + h - 1, y + h - 1};
    if (zx != 1000 || zy != 1000 || rz) {
        double a = rz * M_PI / 180.0;
        double px[4] = {-hw, hw, hw, -hw}, py[4] = {hh, hh, hh + 1 - h, hh + 1 - h};
        for (int i = 0; i < 4; i++) {
            double qx = px[i] * zx / 1000.0, qy = py[i] * zy / 1000.0;
            dx[i] = (int)lround(x + hw + qx * cos(a) + qy * sin(a)) & 0xffff;
            dy[i] = (int)lround(y + hh + qx * sin(a) - qy * cos(a)) & 0xffff;
        }
    }
    for (int i = 0; i < 4; i++) put_u16(b, sx[i]);
    for (int i = 0; i < 4; i++) put_u16(b, sy[i]);
    for (int i = 0; i < 4; i++) put_u16(b, dx[i]);
    for (int i = 0; i < 4; i++) put_u16(b, dy[i]);
    put_f64(b, x);
    put_f64(b, y);
    put_f64(b, 1000);
    put_i32(b, 0);
    put_i32(b, 0);
    put_i32(b, rz);
    put_i32(b, zx);
    put_i32(b, zy);
    put_i32(b, 1000);
    put_f64(b, hw);
    put_f64(b, hh);
    put_f64(b, 1000);
    if (window)
        for (int i = 0; i < 3; i++) put_f64(b, 0);
}

static void flags(Buf *b, int count) {
    for (int i = 0; i < count; i++) buf_u32(b, 0);
}

typedef struct Collect {
    Obj **items;
    int count;
} Collect;

static void collect(Obj *o, Collect *c) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *ch = o->children[i];
        c->items = n2_grow(c->items, sizeof(Obj *) * (size_t)(c->count + 1));
        c->items[c->count++] = ch;
        collect(ch, c);
    }
}

static int by_serial(const void *a, const void *b) {
    const Obj *x = *(Obj *const *)a, *y = *(Obj *const *)b;
    return x->serial < y->serial ? -1 : x->serial > y->serial;
}

static char *parent_path(Obj *o, Obj *root) {
    Buf b = {0};
    if (o->scenePath && o->scenePath[0]) buf_str(&b, o->scenePath);
    Obj *chain[64];
    int n = 0;
    for (Obj *p = o->parent; p && p != root && n < 64; p = p->parent) chain[n++] = p;
    for (int i = n - 1; i >= 0; i--) {
        if (b.len) buf_u8(&b, '/');
        buf_str(&b, chain[i]->name);
    }
    buf_u8(&b, 0);
    return (char *)b.data;
}

static char *scope_of(Obj *o, Obj *root) {
    if (o->parent && o->parent != root && o->createCall) {
        const char *q = strchr(o->createCall, '"');
        const char *e = q ? strchr(q + 1, '"') : NULL;
        if (q && e) {
            const char *slash = NULL;
            for (const char *p = q + 1; p < e; p++)
                if (*p == '/') slash = p;
            if (slash) return n2_strndup(q + 1, (size_t)(slash - q - 1));
        }
    }
    return n2_strdup(o->scenePath ? o->scenePath : "");
}

static void record_head(Buf *b, u32 owner, u32 id, const char *call, const char *scope, const char *name,
                        const char *path, const char *alias) {
    buf_u32(b, owner);
    buf_u32(b, id);
    buf_u32(b, 0);
    put_str(b, call);
    put_str(b, scope);
    put_str(b, name);
    put_str(b, path);
    put_str(b, alias);
    buf_u32(b, id);
}

static void record_core(Buf *b, Obj *o, int prio, int x, int y, int created) {
    for (int i = 0; i < 4; i++) buf_u32(b, 0);
    put_i32(b, o && o->locked);
    put_i32(b, o && o->disused);
    put_i32(b, o && o->hideable);
    put_i32(b, o && o->kind == OBJ_CHOICE && !o->unclickable);
    put_i32(b, prio);
    put_i32(b, 0);
    put_i32(b, created);
    put_i32(b, x);
    put_i32(b, y);
    for (int i = 0; i < 4; i++) put_i32(b, 0);
}

static void tail(Buf *b, const u32 cb[3], bool loopPoints, u32 ls, u32 le, bool choice) {
    buf_u32(b, 0);
    for (int i = 0; i < 3; i++) buf_u32(b, cb[i]);
    buf_u32(b, 0);
    buf_u32(b, loopPoints);
    if (loopPoints) {
        buf_u32(b, ls);
        buf_u32(b, le);
    }
    buf_u32(b, 0);
    buf_u32(b, choice);
    if (choice)
        for (int i = 0; i < 4; i++) buf_u32(b, 1);
}

static void name_record(Buf *b, u32 owner, u32 id, const char *name, const char *scope) {
    char *call = n2_fmt("CreateName(\"%s\");", name);
    record_head(b, owner, id, call, scope, name, scope, "");
    free(call);
    record_core(b, NULL, 0, 0, 0, 0);
    flags(b, 16);
    static const u32 idle[3] = {IDLE_CALLBACK, IDLE_CALLBACK, IDLE_CALLBACK};
    tail(b, idle, false, 0, 0, false);
}

static void pad_record(Buf *out, Buf *r) {
    if (r->len > FRAME_REC) r->len = FRAME_REC;
    buf_put(out, r->data, r->len);
    u8 *z = n2_alloc(FRAME_REC - r->len + 1);
    buf_put(out, z, FRAME_REC - r->len);
    free(z);
}

Buf frames_save(N2 *n, int procCount) {
    Collect c = {0};
    collect(&n->root, &c);
    qsort(c.items, (size_t)c.count, sizeof(Obj *), by_serial);
    Buf recs = {0};
    u32 count = 0, nextId = 0x03000000u;
    char **names = NULL;
    int nameCount = 0;
    for (int i = 0; i < c.count; i++) {
        Obj *o = c.items[i];
        if (!o->createCall || o->kind == OBJ_TEXT || o->kind == OBJ_BACKLOG) continue;
        int proc = o->proc < procCount ? o->proc : procCount - 1;
        if (proc < 0) proc = 0;
        u32 owner = slot_proc_serial(proc);
        if (o->parent == &n->root && o->scenePath && o->scenePath[0]) {
            char *path = n2_strdup(o->scenePath);
            for (char *p = path;; p++) {
                if (*p != '/' && *p != 0) continue;
                char saved = *p;
                *p = 0;
                bool seen = false;
                for (int k = 0; k < nameCount; k++) seen = seen || strcmp(names[k], path) == 0;
                if (!seen) {
                    names = n2_grow(names, sizeof(char *) * (size_t)(nameCount + 1));
                    names[nameCount++] = n2_strdup(path);
                    char *slash = strrchr(path, '/');
                    char *scope = slash ? n2_strndup(path, (size_t)(slash - path)) : n2_strdup("");
                    Buf r = {0};
                    name_record(&r, owner, nextId += 0x100, slash ? slash + 1 : path, scope);
                    pad_record(&recs, &r);
                    buf_free(&r);
                    free(scope);
                    count++;
                }
                *p = saved;
                if (!saved) break;
            }
            free(path);
        }
        double cx = o->x, cy = o->y;
        if (o->centerX) cx = (N2_W - o->w) / 2.0;
        if (o->centerY) cy = (N2_H - o->h) / 2.0;
        int x = (int)lround(cx), y = (int)lround(cy);
        char *path = parent_path(o, &n->root);
        char *scope = scope_of(o, &n->root);
        u32 id = nextId += 0x100;
        Buf r = {0};
        record_head(&r, owner, id, o->createCall, scope, o->name, path, o->alias ? o->alias : "");
        free(path);
        free(scope);
        bool tex = call_is(o->createCall, "CreateTexture") || call_is(o->createCall, "CreateColor");
        bool win = call_is(o->createCall, "CreateWindow");
        bool snd = call_is(o->createCall, "CreateSound") && o->sound;
        bool img = call_is(o->createCall, "LoadImage");
        bool choice = o->kind == OBJ_CHOICE;
        int created = o->kind == OBJ_NAME || choice ? 0 : 1;
        record_core(&r, o, o->prio, x, y, created);
        flags(&r, 2);
        buf_u32(&r, win);
        if (win) {
            int hc = o->h < N2_H - y ? o->h : N2_H - y;
            int rects[24] = {x, y, o->w, o->h, x, y, o->w, o->h, x, y, o->w, hc,
                             x, y, o->w, hc, 3 * x, y, 3 * o->w, hc, 4 * x, y, 4 * o->w, hc};
            for (int k = 0; k < 24; k++) put_i32(&r, rects[k]);
            put_geom(&r, x, y, o->w, o->h, 1000, 1000, 0, true);
        }
        buf_u32(&r, 0);
        buf_u32(&r, tex);
        if (tex) {
            buf_u32(&r, id);
            buf_u32(&r, 1);
            put_i32(&r, x);
            put_i32(&r, y);
            double op = o->opacity < 0 ? 0 : o->opacity > 1000 ? 1000 : o->opacity;
            buf_u8(&r, (u8)(op * 0.255));
            buf_u32(&r, o->blend == 1 ? 2u : 0x101u);
            put_geom(&r, x, y, o->w, o->h, (int)lround(o->zoomX), (int)lround(o->zoomY), (int)lround(o->rotZ), false);
            buf_u32(&r, 1);
            buf_u32(&r, 0);
            buf_u32(&r, 0);
            buf_u32(&r, 0);
        }
        buf_u32(&r, img);
        if (img) {
            buf_u32(&r, 0);
            buf_u32(&r, 0);
            buf_u32(&r, 0);
        }
        flags(&r, 1 + 6 + 2);
        SoundState st = {0};
        if (snd) sound_get_state(n, o->sound, &st);
        buf_u32(&r, snd);
        if (snd) {
            u8 state = st.playing ? (st.kind == 0 ? 0x11 : st.kind == 2 ? 0x13 : 0x14) : 2;
            buf_u8(&r, state);
            put_i32(&r, st.loop ? -1 : 0);
            put_i32(&r, (i32)lround(st.volume));
            put_i32(&r, 1000);
            put_i32(&r, 0);
            put_i32(&r, st.playing ? (i32)st.position : 0);
            put_i32(&r, 0);
            flags(&r, 3);
        }
        static const u32 texCb[3] = {0x1001, 0x2001, 0x4001}, idleCb[3] = {IDLE_CALLBACK, IDLE_CALLBACK, IDLE_CALLBACK};
        tail(&r, tex ? texCb : idleCb, snd, st.loopStart, st.loopEnd, choice);
        pad_record(&recs, &r);
        buf_free(&r);
        count++;
    }
    for (int k = 0; k < nameCount; k++) free(names[k]);
    free(names);
    free(c.items);
    Buf b = {0};
    buf_str(&b, "NPSF");
    buf_u32(&b, NPS_TAG);
    buf_u32(&b, 16 + count * FRAME_REC);
    buf_u32(&b, count);
    buf_put(&b, recs.data, recs.len);
    buf_free(&recs);
    return b;
}

static const char *str_at(const u8 *r, size_t *o) {
    if (*o >= FRAME_REC) return NULL;
    const u8 *s = r + *o;
    const u8 *z = memchr(s, 0, FRAME_REC - *o);
    if (!z) return NULL;
    *o = (size_t)(z - r) + 4;
    return (const char *)s;
}

static u32 newest_serial(Obj *o, Obj **out) {
    u32 best = 0;
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if (c->serial >= best) {
            best = c->serial;
            *out = c;
        }
        Obj *sub = NULL;
        u32 s = newest_serial(c, &sub);
        if (sub && s >= best) {
            best = s;
            *out = sub;
        }
    }
    return best;
}

static int snippetCount;

bool n2_run_snippet(N2 *n, const char *sjisCode) {
    Buf src = {0};
    buf_str(&src, "chapter main\r\n{\r\n");
    buf_str(&src, sjisCode);
    buf_str(&src, "\r\n}\r\n");
    char err[512] = "";
    char *key = n2_fmt("vndegen-restore/%d.nss", ++snippetCount);
    Script *s = script_compile(key, src.data, src.len, err, sizeof err);
    buf_free(&src);
    if (!s) {
        n2_log(n, "load: can't run %s: %s", sjisCode, err);
        free(key);
        return false;
    }
    map_set(&n->scripts, key, s);
    free(key);
    Chunk *c = script_find(s, CHUNK_CHAPTER, "main");
    Thread *t = thread_new(n, "restore");
    t->ctx = &n->root;
    Obj *saved = n->curRoot;
    n->curRoot = &n->root;
    thread_call(n, t, c, NULL, 0);
    for (int i = 0; i < 64 && t->fp > 0 && t->wait != WAIT_DONE; i++) {
        t->wait = WAIT_NONE;
        thread_run(n, t);
    }
    n->curRoot = saved;
    thread_kill(n, t);
    for (int i = 0; i < n->threadCount; i++) {
        if (n->threads[i] == t) {
            n->threads[i] = n->threads[--n->threadCount];
            break;
        }
    }
    thread_free(t);
    return true;
}

typedef struct Reader {
    const u8 *r;
    size_t o;
    bool bad;
} Reader;

static u32 ru(Reader *R) {
    if (R->o + 4 > FRAME_REC) {
        R->bad = true;
        return 0;
    }
    u32 v = rd32(R->r + R->o);
    R->o += 4;
    return v;
}

static void skip(Reader *R, size_t n) {
    if (R->o + n > FRAME_REC) R->bad = true;
    else R->o += n;
}

static void skip_str(Reader *R) {
    if (!str_at(R->r, &R->o)) R->bad = true;
}

static bool flag(Reader *R) {
    u32 v = ru(R);
    if (v > 1) R->bad = true;
    return v == 1;
}

typedef struct Geom {
    int w, h, rz, zx, zy;
} Geom;

static Geom read_geom(Reader *R, bool window) {
    Geom g = {0};
    ru(R);
    skip(R, 12 * 8);
    g.w = (i32)ru(R);
    g.h = (i32)ru(R);
    skip(R, 16 * 2 + 3 * 8);
    ru(R);
    ru(R);
    g.rz = (i32)ru(R);
    g.zx = (i32)ru(R);
    g.zy = (i32)ru(R);
    ru(R);
    skip(R, 3 * 8);
    if (window) skip(R, 3 * 8);
    return g;
}

static void skip_dump(Reader *R) {
    if (flag(R)) {
        skip(R, 2);
        skip_str(R);
    }
}

static void apply_state(N2 *n, Obj *o, Reader *R) {
    if (flag(R)) skip(R, 9 * 4);
    if (flag(R)) skip(R, 16 * 4);
    if (flag(R)) {
        skip(R, 24 * 4);
        read_geom(R, true);
    }
    if (flag(R)) skip(R, 53 * 4);
    if (flag(R)) {
        skip(R, 4 * 4);
        if (R->o < FRAME_REC) o->opacity = R->r[R->o] * 1000 / 255;
        skip(R, 1);
        u32 render = ru(R);
        Geom g = read_geom(R, false);
        if (render == 2) o->blend = 1;
        if (g.zx) o->zoomX = g.zx;
        if (g.zy) o->zoomY = g.zy;
        o->rotZ = g.rz;
        if (flag(R)) skip_dump(R);
        skip(R, 8);
    }
    if (flag(R)) {
        skip_dump(R);
        skip(R, 8);
    }
    if (flag(R)) {
        R->bad = true;
        return;
    }
    if (flag(R)) {
        u8 to = R->o + 1 < FRAME_REC ? R->r[R->o + 1] : 255;
        o->opacity = to * 1000 / 255;
        skip(R, 2 + 4 * 4);
        skip_str(R);
    }
    if (flag(R)) {
        skip(R, 2 * 4);
        o->zoomX = (i32)ru(R);
        o->zoomY = (i32)ru(R);
        skip(R, 4 * 4);
        skip_str(R);
    }
    if (flag(R)) {
        skip(R, 3 * 4);
        o->x = (i32)ru(R);
        o->y = (i32)ru(R);
        skip(R, 5 * 4);
        skip_str(R);
        if (flag(R)) {
            ru(R);
            skip_str(R);
        }
    }
    if (flag(R)) {
        skip(R, 3 * 4);
        o->rotX = (i32)ru(R);
        o->rotY = (i32)ru(R);
        o->rotZ = (i32)ru(R);
        skip(R, 4 * 4);
        skip_str(R);
    }
    if (flag(R)) {
        skip(R, 12 * 4);
        skip_str(R);
    }
    if (flag(R)) {
        skip(R, 8 * 4);
        skip_str(R);
        skip_str(R);
    }
    if (flag(R)) {
        R->bad = true;
        return;
    }
    if (flag(R)) {
        R->bad = true;
        return;
    }
    SoundState st = {0};
    bool snd = flag(R);
    if (snd) {
        u8 state = R->o < FRAME_REC ? R->r[R->o] : 0;
        skip(R, 1);
        st.playing = (state & 0x10) != 0;
        st.loop = (i32)ru(R) != 0;
        st.volume = (i32)ru(R);
        ru(R);
        ru(R);
        st.position = ru(R);
        ru(R);
        for (int k = 0; k < 3; k++) {
            if (flag(R)) {
                ru(R);
                i32 to = (i32)ru(R);
                if (k == 0) st.volume = to;
                skip(R, 3 * 4);
                skip_str(R);
            }
        }
    }
    if (flag(R)) skip(R, 5 * 4);
    skip(R, 3 * 4);
    if (flag(R)) skip(R, 5 * 4);
    if (flag(R)) {
        st.loopStart = ru(R);
        st.loopEnd = ru(R);
    }
    if (snd && o->sound && !R->bad) sound_set_state(n, o->sound, &st);
}

bool frames_load(N2 *n, const u8 *d, size_t size, bool *hadText) {
    *hadText = false;
    if (size < 16 || memcmp(d, "NPSF", 4) != 0) return false;
    u32 count = rd32(d + 12);
    if (16 + (size_t)count * FRAME_REC > size) return false;
    for (u32 i = 0; i < count; i++) {
        const u8 *r = d + 16 + (size_t)i * FRAME_REC;
        size_t o = 12;
        if (rd32(r + 8) == 1) {
            o = 12 + 0x17;
            if (!str_at(r, &o)) continue;
        }
        const char *call = str_at(r, &o);
        if (!call || !str_at(r, &o) || !str_at(r, &o) || !str_at(r, &o) || !str_at(r, &o)) continue;
        if (o + 72 > FRAME_REC) continue;
        const u8 *core = r + o;
        if (call_is(call, "LoadText") || call_is(call, "CreateText")) {
            *hadText = *hadText || call_is(call, "LoadText");
            continue;
        }
        if (call_is(call, "CreateName")) continue;
        Obj *dummy = NULL;
        u32 before = newest_serial(&n->root, &dummy);
        if (!n2_run_snippet(n, call)) continue;
        Obj *made = NULL;
        u32 after = newest_serial(&n->root, &made);
        if (!made || after <= before) continue;
        made->locked = rd32(core + 4 + 4 * 4) != 0;
        made->disused = rd32(core + 4 + 5 * 4) != 0;
        made->hideable = rd32(core + 4 + 6 * 4) != 0;
        made->prio = (int)rd32(core + 4 + 8 * 4);
        made->x = (i32)rd32(core + 4 + 11 * 4);
        made->y = (i32)rd32(core + 4 + 12 * 4);
        made->centerX = made->centerY = false;
        Reader R = {r, o + 72, false};
        apply_state(n, made, &R);
        if (R.bad) n2_log(n, "load: %s has state VNDegen doesn't read", call);
    }
    n->dirty = true;
    return true;
}
