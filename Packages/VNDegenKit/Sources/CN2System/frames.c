#include "n2.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define NPS_TAG 0xf1f2b5c5u
#define FRAME_REC 0x7800

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static const char *str_at(const u8 *r, size_t *o) {
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
        if (strncasecmp(call, "LoadText", 8) == 0) {
            *hadText = true;
            continue;
        }
        u32 before = 0;
        Obj *dummy = NULL;
        before = newest_serial(&n->root, &dummy);
        if (!n2_run_snippet(n, call)) continue;
        Obj *made = NULL;
        u32 after = newest_serial(&n->root, &made);
        if (!made || after <= before) continue;
        made->prio = (int)rd32(core + 4 + 8 * 4);
        made->x = (i32)rd32(core + 4 + 11 * 4);
        made->y = (i32)rd32(core + 4 + 12 * 4);
    }
    n->dirty = true;
    return true;
}

Buf frames_save(N2 *n, int procCount) {
    Buf b = {0};
    buf_str(&b, "NPSF");
    buf_u32(&b, NPS_TAG);
    buf_u32(&b, 16);
    buf_u32(&b, 0);
    return b;
}
