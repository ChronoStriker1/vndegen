#include "age.h"

#include <time.h>
#include <unistd.h>

typedef struct Buf {
    u8 *d;
    size_t n, cap;
} Buf;

static void bput(Buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 4096;
        b->d = ag_realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}

static void b32(Buf *b, u32 v) {
    u8 t[4];
    wr32(t, v);
    bput(b, t, 4);
}

static void bzero_(Buf *b, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 4096;
        b->d = ag_realloc(b->d, b->cap);
    }
    memset(b->d + b->n, 0, n);
    b->n += n;
}

static u32 crc_ref_tab[256], crc_msb_tab[256];

static void crc_init(void) {
    if (crc_ref_tab[1]) return;
    for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_ref_tab[i] = c;
        c = i << 24;
        for (int k = 0; k < 8; k++) c = c & 0x80000000u ? (c << 1) ^ 0x04c11db7u : c << 1;
        crc_msb_tab[i] = c;
    }
}

static u32 crc_ref(const u8 *p, size_t n) {
    crc_init();
    u32 c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) c = crc_ref_tab[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

static u32 crc_msb(const u8 *p, size_t n) {
    crc_init();
    u32 c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) c = crc_msb_tab[((c >> 24) ^ p[i]) & 255] ^ (c << 8);
    return c ^ 0xffffffffu;
}

static u8 *lzss_pack(const u8 *src, size_t n, size_t *outLen) {
    Buf o = { 0 };
    enum { HB = 1 << 14 };
    i32 *head = ag_alloc(sizeof(i32) * HB);
    i32 *prev = ag_alloc(sizeof(i32) * (n + 1));
    for (int i = 0; i < HB; i++) head[i] = -1;
    size_t i = 0;
    while (i < n) {
        size_t flagAt = o.n;
        u8 flags = 0;
        bzero_(&o, 1);
        for (int b = 0; b < 8 && i < n; b++) {
            int best = 0;
            size_t bestPos = 0;
            if (i + 3 <= n) {
                u32 h = ((u32)src[i] << 6 ^ (u32)src[i + 1] << 3 ^ src[i + 2]) & (HB - 1);
                int tries = 32;
                for (i32 j = head[h]; j >= 0 && tries-- > 0; j = prev[j]) {
                    if (i - (size_t)j > 4096 - 18) break;
                    int l = 0;
                    while (l < 18 && i + (size_t)l < n && src[(size_t)j + (size_t)l] == src[i + (size_t)l]) l++;
                    if (l > best) { best = l; bestPos = (size_t)j; if (l == 18) break; }
                }
            }
            int adv;
            if (best >= 3) {
                u32 off = (u32)((0xfee + bestPos) & 0xfff);
                u8 t[2] = { (u8)(off & 255), (u8)((off >> 4 & 0xf0) | (u32)(best - 3)) };
                bput(&o, t, 2);
                adv = best;
            } else {
                flags |= (u8)(1 << b);
                bput(&o, &src[i], 1);
                adv = 1;
            }
            for (int k = 0; k < adv; k++, i++) {
                if (i + 3 <= n) {
                    u32 h = ((u32)src[i] << 6 ^ (u32)src[i + 1] << 3 ^ src[i + 2]) & (HB - 1);
                    prev[i] = head[h];
                    head[h] = (i32)i;
                }
            }
        }
        o.d[flagAt] = flags;
    }
    free(head);
    free(prev);
    *outLen = o.n;
    return o.d;
}

static char *save_path(AG *g, const char *name) {
    const char *dir = g->saveDir ? g->saveDir : ".";
    char *d = ag_fmt("%s/SAVE", dir);
    ag_mkdirs(d);
    char *p = ag_fmt("%s/%s", d, name);
    free(d);
    return p;
}

static void save_versions(AG *g, i32 *v1, i32 *v2) {
    const char *s = store_setting(&g->st, "SAVEVERSION");
    int v = s ? atoi(s) : 320;
    *v1 = v / 100;
    *v2 = v % 100;
}

static u32 playtime(AG *g) { return (u32)(g->playBase + (g->now - g->playStart) / 1000); }

static bool container_write(AG *g, const char *path, const u8 *payload, size_t n, const u8 *tail, size_t tailN) {
    Buf chk = { 0 };
    b32(&chk, crc_msb(payload, n));
    b32(&chk, crc_ref(payload, n));
    bput(&chk, payload, n);
    size_t packedN = 0;
    u8 *packed = lzss_pack(chk.d, chk.n, &packedN);
    Buf src = { 0 };
    b32(&src, (u32)chk.n);
    b32(&src, (u32)chk.n);
    if (packedN < chk.n) {
        b32(&src, (u32)packedN);
        bput(&src, packed, packedN);
    } else {
        b32(&src, (u32)chk.n);
        bput(&src, chk.d, chk.n);
    }
    while (src.n & 3) bzero_(&src, 1);
    free(packed);
    size_t words = src.n / 4;
    Buf ex = { 0 };
    u32 seed = (u32)time(NULL) * 2654435761u, mult = ((u32)rand() | 1u) & 0xffff;
    if (mult < 3) mult = 0x3ae3;
    u32 s0 = seed, m0 = mult;
    for (size_t i = 0; i < words; i++) {
        u32 v = rd32(src.d + i * 4) ^ seed;
        b32(&ex, (v >> 16) * mult);
        b32(&ex, (v & 0xffff) * mult);
        seed += 0x0b0b0b0bu;
        mult = (mult + 0x0b02u) & 0xffff;
    }
    u8 h[0x138] = { 0 };
    memcpy(h, "S4SD", 4);
    memcpy(h + 4, "433F", 4);
    memcpy(h + 8, g->st.gameId, 0xe7);
    wr32(h + 0xf0, (u32)chk.n);
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    u16 st[8] = { (u16)(tm.tm_year + 1900), (u16)(tm.tm_mon + 1), (u16)tm.tm_wday, (u16)tm.tm_mday, (u16)tm.tm_hour, (u16)tm.tm_min, (u16)tm.tm_sec, 0 };
    for (int i = 0; i < 8; i++) wr16(h + 0x108 + i * 2, st[i]);
    wr32(h + 0x118, playtime(g));
    i32 v1, v2;
    save_versions(g, &v1, &v2);
    wr32(h + 0x11c, (u32)v1);
    wr32(h + 0x120, (u32)v2);
    wr32(h + 0x124, (u32)(ex.n / 4));
    wr32(h + 0x128, crc_msb(ex.d, ex.n));
    wr32(h + 0x12c, crc_ref(ex.d, ex.n));
    wr32(h + 0x130, s0);
    wr32(h + 0x134, m0);
    Buf file = { 0 };
    bput(&file, h, sizeof h);
    bput(&file, ex.d, ex.n);
    if (tail) bput(&file, tail, tailN);
    bool ok = ag_write_file(path, file.d, file.n);
    free(chk.d);
    free(src.d);
    free(ex.d);
    free(file.d);
    return ok;
}

static bool header_ok(AG *g, const u8 *d, size_t n) {
    if (n < 0x138 || memcmp(d, "S4SD", 4)) return false;
    if (strncmp((const char *)d + 8, g->st.gameId, 0xe7)) return false;
    return true;
}

static u8 *container_read(AG *g, const char *path, size_t *outN, u8 **tail, size_t *tailN, u8 *hdr) {
    size_t n = 0;
    u8 *d = ag_read_file(path, &n);
    if (!d) return NULL;
    if (!header_ok(g, d, n)) { free(d); return NULL; }
    if (hdr) memcpy(hdr, d, 0x124);
    u32 words = rd32(d + 0x124), seed = rd32(d + 0x130), mult = rd32(d + 0x134) & 0xffff;
    if (0x138 + (size_t)words * 4 > n || (words & 1) || !mult) { free(d); return NULL; }
    if (rd32(d + 0x12c) != crc_ref(d + 0x138, (size_t)words * 4)) { free(d); return NULL; }
    size_t sw = words / 2;
    u8 *src = ag_alloc(sw * 4 + 16);
    for (size_t i = 0; i < sw; i++) {
        u32 a = rd32(d + 0x138 + i * 8), b = rd32(d + 0x13c + i * 8);
        if (a % mult || b % mult) { free(src); free(d); return NULL; }
        u32 hi = a / mult, lo = b / mult;
        wr32(src + i * 4, (hi << 16 | lo) ^ seed);
        seed += 0x0b0b0b0bu;
        mult = (mult + 0x0b02u) & 0xffff;
    }
    u32 orig = rd32(src), stored = rd32(src + 8);
    if (12 + (size_t)stored > sw * 4 || orig < 8) { free(src); free(d); return NULL; }
    u8 *chk = ag_alloc((size_t)orig + 4);
    if (stored == orig) memcpy(chk, src + 12, orig);
    else ag_lzss(src + 12, stored, chk, orig);
    free(src);
    if (rd32(chk) != crc_msb(chk + 8, orig - 8) || rd32(chk + 4) != crc_ref(chk + 8, orig - 8)) { free(chk); free(d); return NULL; }
    size_t end = 0x138 + (size_t)words * 4;
    if (tail) {
        *tailN = n - end;
        *tail = ag_alloc(*tailN + 4);
        memcpy(*tail, d + end, *tailN);
    }
    free(d);
    memmove(chk, chk + 8, orig - 8);
    *outN = orig - 8;
    return chk;
}

static void put_cstr(Buf *b, const char *s) { bput(b, s, strlen(s) + 1); }

static void strings_blob(Buf *b, AG *g, u32 count) {
    Buf s = { 0 };
    for (u32 i = 0; i < count; i++) put_cstr(&s, i < g->ngs && g->gs[i].s ? g->gs[i].s : "");
    size_t dw = s.n / 4 + 1;
    b32(b, (u32)dw);
    bput(b, s.d, s.n);
    bzero_(b, dw * 4 - s.n);
    free(s.d);
}

static void history_tail(AG *g, Buf *t) {
    Text *x = &g->text;
    Buf h = { 0 };
    b32(&h, (u32)x->nent);
    for (int i = 0; i < x->nent; i++) { b32(&h, (u32)x->ent[i].layout); b32(&h, (u32)x->ent[i].first); }
    b32(&h, (u32)x->nrec);
    for (int i = 0; i < x->nrec; i++)
        for (int k = 0; k < 11; k++) b32(&h, (u32)x->rec[i].d[k]);
    Buf s = { 0 };
    for (int i = 0; i < x->nrec; i++) put_cstr(&s, x->rec[i].text ? x->rec[i].text : "");
    while (s.n & 3) bzero_(&s, 1);
    b32(&h, (u32)(s.n / 4));
    for (size_t i = 0; i < s.n; i++) s.d[i] = (u8)~s.d[i];
    bput(&h, s.d, s.n);
    free(s.d);
    size_t pn = 0;
    u8 *packed = lzss_pack(h.d, h.n, &pn);
    b32(t, (u32)h.n);
    b32(t, (u32)h.n);
    if (pn < h.n) { b32(t, (u32)pn); bput(t, packed, pn); }
    else { b32(t, (u32)h.n); bput(t, h.d, h.n); }
    free(packed);
    free(h.d);
}

static void history_load(AG *g, const u8 *tail, size_t tn) {
    hist_clear(g);
    if (tn < 12) return;
    u32 len = rd32(tail), stored = rd32(tail + 8);
    if (12 + (size_t)stored > tn || len > (64u << 20)) return;
    u8 *h = ag_alloc((size_t)len + 4);
    if (stored == len) memcpy(h, tail + 12, len);
    else ag_lzss(tail + 12, stored, h, len);
    Text *x = &g->text;
    size_t q = 0;
    u32 ne = len >= 4 ? rd32(h) : 0;
    q = 4;
    if (q + (size_t)ne * 8 > len) { free(h); return; }
    x->cent = (int)ne + 64;
    x->ent = ag_realloc(x->ent, sizeof(HistEntry) * (size_t)x->cent);
    for (u32 i = 0; i < ne; i++) { x->ent[i].layout = (i32)rd32(h + q); x->ent[i].first = (i32)rd32(h + q + 4); q += 8; }
    x->nent = (int)ne;
    x->anchor = ne ? (int)ne - 1 : 0;
    u32 nr = q + 4 <= len ? rd32(h + q) : 0;
    q += 4;
    if (q + (size_t)nr * 44 + 4 > len) { x->nent = 0; free(h); return; }
    x->crec = (int)nr + 256;
    x->rec = ag_realloc(x->rec, sizeof(HistRec) * (size_t)x->crec);
    for (u32 i = 0; i < nr; i++) {
        for (int k = 0; k < 11; k++) x->rec[i].d[k] = (i32)rd32(h + q + k * 4);
        x->rec[i].text = NULL;
        q += 44;
    }
    x->nrec = (int)nr;
    u32 sdw = rd32(h + q);
    q += 4;
    if (q + (size_t)sdw * 4 <= len) {
        u8 *sb = h + q;
        for (size_t i = 0; i < (size_t)sdw * 4; i++) sb[i] = (u8)~sb[i];
        const char *s = (const char *)sb, *e = s + (size_t)sdw * 4;
        for (u32 i = 0; i < nr && s < e; i++) {
            size_t l = strnlen(s, (size_t)(e - s));
            char *v = ag_alloc(l + 1);
            memcpy(v, s, l);
            x->rec[i].text = v;
            s += l + 1;
        }
    }
    free(h);
    hist_drop_last_group(g);
}

static bool numbered_write(AG *g, int slot) {
    Buf b = { 0 };
    int cutoff = g->saveFrame;
    if (cutoff > g->top) cutoff = g->top;
    if (cutoff < 0) cutoff = 0;
    b32(&b, (u32)cutoff);
    b32(&b, 0);
    b32(&b, (u32)g->snd.bgm);
    for (int i = 0; i < 10; i++) b32(&b, (u32)g->snd.sfxRes[i]);
    for (int i = 0; i < 100; i++) { b32(&b, 0xffffffffu); b32(&b, 0); b32(&b, 0); }
    for (int i = 0; i < 1000; i++)
        for (int k = 0; k < 5; k++) b32(&b, (u32)g->gfx.surf[i].snap[k]);
    for (int i = 0; i <= cutoff; i++) {
        Frame *f = &g->fr[i];
        b32(&b, (u32)(i - 1));
        b32(&b, f->sc ? f->sc->id : 0);
        i32 rets[256];
        int nr = 0;
        for (int k = 0; k < f->rsp && nr < 256; k++) {
            i32 idx = f->sc ? script_table_index(f->sc->t3, f->sc->t3Count, f->ret[k] - 3) : -1;
            if (idx >= 0) rets[nr++] = idx;
        }
        b32(&b, (u32)nr);
        for (int k = 0; k < 256; k++) b32(&b, k < nr ? (u32)rets[k] : 0);
        b32(&b, (u32)f->t1);
        b32(&b, i == cutoff ? 0xffffffffu : (u32)f->t2);
    }
    for (int i = 0; i < 6; i++) b32(&b, g->st.bankSaved[i]);
    for (u32 i = 0; i < g->st.bankSaved[0]; i++) b32(&b, i < g->ngi ? (u32)g->gi[i] : 0);
    for (u32 i = 0; i < g->st.bankSaved[1]; i++) b32(&b, 0);
    strings_blob(&b, g, g->st.bankSaved[2]);
    for (int k = 3; k < 6; k++)
        for (u32 i = 0; i < g->st.bankSaved[k]; i++) b32(&b, 0);
    b32(&b, 0x2e4);
    b32(&b, (u32)g->gfx.nobj);
    u8 rec[0x2e4];
    for (int i = 0; i < g->gfx.nobj; i++) {
        Obj *o = g->gfx.obj[i];
        b32(&b, (u32)o->handle);
        gfx_obj_record(o, rec);
        bput(&b, rec, sizeof rec);
        bzero_(&b, 0x2e4 * 4 - 0x2e4);
    }
    b32(&b, (u32)g->gfx.rangeFirst);
    b32(&b, (u32)g->gfx.rangeCount);
    gfx_obj_record(&g->gfx.range, rec);
    wr32(rec + 0x68, 1);
    bput(&b, rec, sizeof rec);
    Buf tail = { 0 };
    history_tail(g, &tail);
    char name[32];
    snprintf(name, sizeof name, "SAVE%02d.DAT", slot);
    char *p = save_path(g, name);
    bool ok = container_write(g, p, b.d, b.n, tail.d, tail.n);
    free(p);
    free(b.d);
    free(tail.d);
    ag_log(g, "saved slot %d: %d frames, %d objects, ok=%d", slot, cutoff + 1, g->gfx.nobj, ok);
    return ok;
}

static bool numbered_read(AG *g, int slot) {
    char name[32];
    snprintf(name, sizeof name, "SAVE%02d.DAT", slot);
    char *p = save_path(g, name);
    size_t n = 0, tn = 0;
    u8 *tail = NULL, hdr[0x124];
    u8 *b = container_read(g, p, &n, &tail, &tn, hdr);
    free(p);
    if (!b) { free(tail); return false; }
    if ((i32)rd32(hdr + 0x11c) != 3) { free(b); free(tail); return false; }
    history_load(g, tail, tn);
    free(tail);
    size_t q = 0;
#define NEED(k) if (q + (k) > n) goto bad
    NEED(12);
    i32 cutoff = (i32)rd32(b);
    i32 bgm = (i32)rd32(b + 8);
    q = 12;
    NEED(40);
    i32 sfx[10];
    for (int i = 0; i < 10; i++) sfx[i] = (i32)rd32(b + q + i * 4);
    q += 40 + 1200;
    NEED(20000);
    const u8 *surfRec = b + q;
    q += 20000;
    if (cutoff < 0 || cutoff >= AG_FRAMES - 4) goto bad;
    NEED((size_t)(cutoff + 1) * 0x414);
    free(g->restore);
    g->restore = ag_alloc(sizeof(SavedFrame) * (size_t)(cutoff + 1));
    g->nrestore = cutoff + 1;
    for (int i = 0; i <= cutoff; i++) {
        const u8 *f = b + q;
        SavedFrame *s = &g->restore[i];
        s->id = rd32(f + 4);
        s->nret = (i32)rd32(f + 8);
        if (s->nret < 0 || s->nret > 256) s->nret = 0;
        for (int k = 0; k < s->nret; k++) s->ret[k] = (i32)rd32(f + 12 + k * 4);
        s->t1 = (i32)rd32(f + 0x40c);
        s->t2 = (i32)rd32(f + 0x410);
        q += 0x414;
    }
    NEED(24);
    u32 counts[6];
    for (int i = 0; i < 6; i++) counts[i] = rd32(b + q + i * 4);
    q += 24;
    NEED((size_t)counts[0] * 4);
    memset(g->gi, 0, sizeof(i32) * g->ngi);
    for (u32 i = 0; i < counts[0] && i < g->ngi; i++) g->gi[i] = (i32)rd32(b + q + i * 4);
    q += (size_t)counts[0] * 4;
    q += (size_t)counts[1] * 4;
    NEED(4);
    u32 sdw = rd32(b + q);
    q += 4;
    NEED((size_t)sdw * 4);
    for (u32 i = 0; i < g->ngs; i++) { free(g->gs[i].s); g->gs[i].s = NULL; g->gs[i].len = 0; }
    {
        const char *s = (const char *)b + q, *e = s + (size_t)sdw * 4;
        for (u32 i = 0; i < counts[2] && s < e; i++) {
            size_t l = strnlen(s, (size_t)(e - s));
            char *v = ag_alloc(l + 1);
            memcpy(v, s, l);
            if (i < g->ngs) str_set(&g->gs[i], v);
            free(v);
            s += l + 1;
        }
    }
    q += (size_t)sdw * 4;
    q += ((size_t)counts[3] + counts[4] + counts[5]) * 4;
    NEED(8);
    u32 rsize = rd32(b + q), nobj = rd32(b + q + 4);
    q += 8;
    if (rsize != 0x2e4) goto bad;
    NEED((size_t)nobj * (4 + rsize * 4));
    i32 *handles = ag_alloc(sizeof(i32) * (nobj + 1));
    u8 *recs = ag_alloc((size_t)rsize * (nobj + 1));
    for (u32 i = 0; i < nobj; i++) {
        handles[i] = (i32)rd32(b + q);
        memcpy(recs + (size_t)i * rsize, b + q + 4, rsize);
        q += 4 + (size_t)rsize * 4;
    }
    if (q + 8 + rsize > n) { free(handles); free(recs); goto bad; }
    i32 rfirst = (i32)rd32(b + q), rcount = (i32)rd32(b + q + 4);
    const char *af = store_setting(&g->st, "AUTOFREETEX");
    if (af && (atoi(af) & 2)) {
        for (int i = 0; i < AG_SURFACES; i++) {
            movie_stop(g, i);
            image_free(g->gfx.surf[i].im);
            g->gfx.surf[i].im = NULL;
            g->gfx.surf[i].res = -1;
        }
    }
    for (int i = 0; i < 1000; i++) {
        i32 r[5];
        for (int k = 0; k < 5; k++) r[k] = (i32)rd32(surfRec + i * 20 + k * 4);
        memcpy(g->gfx.surf[i].rec, r, sizeof r);
        memcpy(g->gfx.surf[i].snap, r, sizeof r);
    }
    gfx_restore_objects(g, recs, handles, (int)nobj, rfirst, rcount, b + q + 8);
    free(handles);
    free(recs);
    snd_restore(g, bgm, sfx);
    g->playBase = rd32(hdr + 0x118);
    g->playStart = g->now;
    free(b);
    return true;
bad:
    free(b);
    free(g->restore);
    g->restore = NULL;
    g->nrestore = 0;
    return false;
#undef NEED
}

static u32 marker(u32 index) { return index * 0x053d6f99u + 0xb0b0b0b0u; }

static u32 modexp(u32 b, u32 e, u32 m) {
    if (!m) return 0;
    u64 r = 1, x = b % m;
    while (e) {
        if (e & 1) r = r * x % m;
        x = x * x % m;
        e >>= 1;
    }
    return (u32)r;
}

static void put_table(Buf *b, const u8 *seen, u32 slots) {
    b32(b, 1u ^ 0x87912345u);
    b32(b, 0xfffffffbu);
    for (u32 i = 0; i < slots; i++) {
        u32 v = seen && seen[i] ? marker(i) : 0;
        if (v >= 0xfffffffbu) v &= 0xffff;
        b32(b, v);
    }
}

static void read_table(const u8 *p, u32 slots, u8 *seen) {
    u32 e = rd32(p) ^ 0x87912345u, m = rd32(p + 4);
    for (u32 i = 0; i < slots; i++) {
        u32 c = rd32(p + 8 + i * 4);
        if (!c) continue;
        u32 v = modexp(c, e, m);
        if ((v & 0xffff) == (marker(i) & 0xffff) && seen) seen[i] = 1;
    }
}

void save_profile_write(AG *g) {
    Buf b = { 0 };
    u32 base = (u32)g->st.cat[0].count;
    b32(&b, base + 2);
    put_table(&b, g->st.seen[0], base);
    b32(&b, (u32)g->npint);
    for (int i = 0; i < g->npint; i++) {
        u8 k[12] = { 0 };
        k[0] = 3;
        char hex[16];
        snprintf(hex, sizeof hex, "%08x", g->pint[i].cell);
        memcpy(k + 1, hex, 8);
        bput(&b, k, 12);
        b32(&b, (u32)g->pint[i].v);
    }
    Buf s = { 0 };
    for (int i = 0; i < g->npstr; i++) {
        char key[16];
        snprintf(key, sizeof key, "\x05%08x", g->pstr[i].cell);
        put_cstr(&s, key);
        put_cstr(&s, g->pstr[i].v ? g->pstr[i].v : "");
    }
    size_t dw = s.n / 4 + 1;
    b32(&b, (u32)g->npstr);
    b32(&b, (u32)dw);
    bput(&b, s.d, s.n);
    bzero_(&b, dw * 4 - s.n);
    free(s.d);
    u32 ext = 0;
    for (int c = 0; c < 256; c++) {
        u32 cnt = c > 0 && g->st.cat[c].e ? (u32)g->st.cat[c].count : 0;
        b32(&b, cnt);
        ext += cnt;
    }
    b32(&b, ext + 2);
    b32(&b, 1u ^ 0x87912345u);
    b32(&b, 0xfffffffbu);
    for (int c = 1; c < 256; c++) {
        if (!g->st.cat[c].e) continue;
        for (int i = 0; i < g->st.cat[c].count; i++) b32(&b, g->st.seen[c] && g->st.seen[c][i] ? marker((u32)i) : 0);
    }
    for (int i = 0; i < 9; i++) b32(&b, 0);
    char *p = save_path(g, "SAVE.DAT");
    container_write(g, p, b.d, b.n, NULL, 0);
    free(p);
    free(b.d);
    Buf r = { 0 };
    bput(&r, "S3RT", 4);
    bput(&r, "433F", 4);
    u8 gid[0x100] = { 0 };
    memcpy(gid, g->st.gameId, 0xe7);
    bput(&r, gid, sizeof gid);
    b32(&r, 1);
    b32(&r, 0);
    b32(&r, (u32)g->nrt);
    for (int i = 0; i < g->nrt; i++) { b32(&r, g->rt[i].id); b32(&r, g->rt[i].count); b32(&r, 0); }
    for (int i = 0; i < g->nrt; i++)
        for (u32 k = 0; k < g->rt[i].count; k++) b32(&r, g->rt[i].flags[k]);
    p = save_path(g, "RT.DAT");
    ag_write_file(p, r.d, r.n);
    free(p);
    free(r.d);
}

static ReadText *rt_find(AG *g, u32 id, u32 count) {
    for (int i = 0; i < g->nrt; i++)
        if (g->rt[i].id == id) {
            ReadText *r = &g->rt[i];
            if (count > r->count) {
                r->flags = ag_realloc(r->flags, count);
                memset(r->flags + r->count, 0, count - r->count);
                r->count = count;
            }
            return r;
        }
    if (g->nrt >= g->crt) { g->crt = g->crt ? g->crt * 2 : 128; g->rt = ag_realloc(g->rt, sizeof(ReadText) * (size_t)g->crt); }
    ReadText *r = &g->rt[g->nrt++];
    r->id = id;
    r->count = count;
    r->flags = ag_alloc(count + 1);
    return r;
}

void readtext_mark(AG *g, u32 script, i32 index, u32 count) {
    if (index < 0 || !count) return;
    ReadText *r = rt_find(g, script, count);
    if ((u32)index < r->count) r->flags[index] = 1;
}

static void profile_read(AG *g) {
    char *p = save_path(g, "SAVE.DAT");
    size_t n = 0;
    u8 *b = container_read(g, p, &n, NULL, NULL, NULL);
    free(p);
    if (b) {
        size_t q = 0;
        u32 cnt = n >= 4 ? rd32(b) : 0;
        q = 4;
        if (cnt >= 2 && q + (size_t)cnt * 4 <= n) read_table(b + q, cnt - 2, g->st.seen[0] && (u32)g->st.cat[0].count >= cnt - 2 ? g->st.seen[0] : NULL);
        q += (size_t)cnt * 4;
        u32 ni = q + 4 <= n ? rd32(b + q) : 0;
        q += 4;
        for (u32 i = 0; i < ni && q + 16 <= n; i++, q += 16) {
            char hex[9];
            memcpy(hex, b + q + 1, 8);
            hex[8] = 0;
            u32 cell = (u32)strtoul(hex, NULL, 16);
            if (g->npint >= g->cpint) { g->cpint = g->cpint ? g->cpint * 2 : 256; g->pint = ag_realloc(g->pint, sizeof(ProfInt) * (size_t)g->cpint); }
            g->pint[g->npint].cell = cell;
            g->pint[g->npint].v = (i32)rd32(b + q + 12);
            g->npint++;
        }
        if (q + 8 <= n) {
            u32 ns = rd32(b + q), sdw = rd32(b + q + 4);
            q += 8;
            const char *s = (const char *)b + q, *e = s + (size_t)sdw * 4;
            if (q + (size_t)sdw * 4 > n) e = (const char *)b + n;
            for (u32 i = 0; i < ns && s < e; i++) {
                size_t kl = strnlen(s, (size_t)(e - s));
                const char *k = s;
                s += kl + 1;
                if (s >= e) break;
                size_t vl = strnlen(s, (size_t)(e - s));
                if (g->npstr >= g->cpstr) { g->cpstr = g->cpstr ? g->cpstr * 2 : 64; g->pstr = ag_realloc(g->pstr, sizeof(ProfStr) * (size_t)g->cpstr); }
                g->pstr[g->npstr].cell = kl >= 9 ? (u32)strtoul(k + 1, NULL, 16) : 0;
                char *v = ag_alloc(vl + 1);
                memcpy(v, s, vl);
                g->pstr[g->npstr].v = v;
                g->npstr++;
                s += vl + 1;
            }
            q += (size_t)sdw * 4;
            if (q + 1024 + 4 <= n) {
                u32 sel[256];
                for (int c = 0; c < 256; c++) sel[c] = rd32(b + q + c * 4);
                q += 1024;
                u32 ext = rd32(b + q);
                q += 4;
                if (ext >= 2 && q + (size_t)ext * 4 <= n) {
                    u32 e2 = rd32(b + q) ^ 0x87912345u, m2 = rd32(b + q + 4);
                    size_t at = q + 8;
                    for (int c = 1; c < 256; c++) {
                        for (u32 i = 0; i < sel[c] && at + 4 <= n; i++, at += 4) {
                            u32 v = rd32(b + at);
                            if (v && (modexp(v, e2, m2) & 0xffff) == (marker(i) & 0xffff) && g->st.seen[c] && i < (u32)g->st.cat[c].count) g->st.seen[c][i] = 1;
                        }
                    }
                }
            }
        }
        free(b);
    }
    p = save_path(g, "RT.DAT");
    size_t rn = 0;
    u8 *r = ag_read_file(p, &rn);
    free(p);
    if (r && rn >= 0x114 && !memcmp(r, "S3RT", 4)) {
        u32 cnt = rd32(r + 0x110);
        size_t q = 0x114, f = q + (size_t)cnt * 12;
        for (u32 i = 0; i < cnt && q + 12 <= rn; i++, q += 12) {
            u32 id = rd32(r + q), mc = rd32(r + q + 4);
            ReadText *t = rt_find(g, id, mc);
            for (u32 k = 0; k < mc && f + 4 <= rn; k++, f += 4) t->flags[k] = rd32(r + f) != 0;
        }
    }
    free(r);
}

void save_init(AG *g) {
    profile_read(g);
    ag_log(g, "profile: %d int cells, %d string cells, %d read-text scripts", g->npint, g->npstr, g->nrt);
    g->playStart = 0;
}

void save_close(AG *g) {
    for (int i = 0; i < g->nrt; i++) free(g->rt[i].flags);
    free(g->rt);
    free(g->restore);
}

static void write_sth(AG *g, int slot, Image *im) {
    int w = im->w, h = im->h, stride = (w * 3 + 3) & ~3;
    size_t n = 54 + (size_t)stride * (size_t)h;
    u8 *d = ag_alloc(n);
    d[0] = 'B';
    d[1] = 'M';
    wr32(d + 2, (u32)(n - 14));
    wr32(d + 10, 54);
    wr32(d + 14, 40);
    wr32(d + 18, (u32)w);
    wr32(d + 22, (u32)h);
    wr16(d + 26, 1);
    wr16(d + 28, 24);
    wr32(d + 34, (u32)(stride * h));
    for (int y = 0; y < h; y++) {
        u8 *row = d + 54 + (size_t)(h - 1 - y) * (size_t)stride;
        for (int x = 0; x < w; x++) {
            u32 c = im->px[(size_t)y * (size_t)w + (size_t)x];
            row[x * 3] = (u8)(c >> 16);
            row[x * 3 + 1] = (u8)(c >> 8);
            row[x * 3 + 2] = (u8)c;
        }
    }
    char name[32];
    snprintf(name, sizeof name, "SAVE%02d.STH", slot);
    char *p = save_path(g, name);
    ag_write_file(p, d, n);
    free(p);
    free(d);
}

static int op_save(AG *g) {
    i32 slot = arg_i(g, 2);
    bool ok = numbered_write(g, slot);
    if (ok) save_profile_write(g);
    arg_set_i(g, 1, ok ? 0 : 1);
    return OP_NEXT;
}

static int op_meta(AG *g) {
    char name[32];
    snprintf(name, sizeof name, "SAVE%02d.DAT", arg_i(g, 2));
    char *p = save_path(g, name);
    FILE *f = fopen(p, "rb");
    free(p);
    if (!f) { arg_set_i(g, 1, 1); return OP_NEXT; }
    u8 h[0x124];
    size_t got = fread(h, 1, sizeof h, f);
    fclose(f);
    if (got < sizeof h || !header_ok(g, h, 0x138)) { arg_set_i(g, 1, 2); return OP_NEXT; }
    arg_set_i(g, 1, 0);
    arg_set_i(g, 3, rd16(h + 0x108));
    arg_set_i(g, 4, rd16(h + 0x10a));
    arg_set_i(g, 5, rd16(h + 0x10e));
    arg_set_i(g, 6, rd16(h + 0x110));
    arg_set_i(g, 7, rd16(h + 0x112));
    arg_set_i(g, 8, rd16(h + 0x114));
    arg_set_i(g, 9, (i32)rd32(h + 0x118));
    return OP_NEXT;
}

static int op_load(AG *g) {
    if (!numbered_read(g, arg_i(g, 2))) { arg_set_i(g, 1, 1); return OP_NEXT; }
    for (int i = 0; i < AG_FRAMES; i++) {
        Frame *f = &g->fr[i];
        if (f->sc) script_free(f->sc);
        free(f->li);
        if (f->ls) for (u32 k = 0; k < f->nls; k++) free(f->ls[k].s);
        free(f->ls);
        free(f->lir);
        free(f->lsr);
        memset(f, 0, sizeof *f);
    }
    g->top = 0;
    g->wait = W_NONE;
    g->coState = 0;
    g->in.nhot = 0;
    g->in.hotArmed = false;
    g->restorePending = true;
    g->restoring = false;
    i32 cb = store_find(&g->st, "CALLBACK_LOAD.BIN");
    if (cb < 0 || !vm_load_root(g, (u32)cb)) vm_restore_step(g);
    return OP_STAY;
}

int vm_restore_step(AG *g) {
    g->restorePending = false;
    g->restoring = true;
    if (!g->nrestore || !vm_load_root(g, g->restore[0].id)) { g->restoring = false; g->halted = true; return OP_HALT; }
    return OP_STAY;
}

static int op_restore_continue(AG *g) {
    if (!g->restoring) return OP_NEXT;
    int i = g->top;
    if (i >= g->nrestore) { g->restoring = false; return OP_NEXT; }
    Frame *f = &g->fr[i];
    SavedFrame *s = &g->restore[i];
    if (i < g->nrestore - 1) {
        f->t1 = s->t1;
        if (s->t2 >= 0 && f->sc->t2 && (u32)s->t2 < f->sc->t2Count) { f->pc = f->sc->t2[s->t2]; return OP_JUMP; }
        g->restoring = false;
        return OP_NEXT;
    }
    g->restoring = false;
    g->saveFrame = i;
    f->t1 = s->t1;
    f->rsp = 0;
    for (int k = 0; k < s->nret && k < AG_RET; k++)
        if (f->sc->t3 && s->ret[k] >= 0 && (u32)s->ret[k] < f->sc->t3Count) f->ret[f->rsp++] = f->sc->t3[s->ret[k]] + 3;
    if (s->t1 >= 0 && f->sc->t1 && (u32)s->t1 < f->sc->t1Count) { f->pc = f->sc->t1[s->t1]; return OP_JUMP; }
    return OP_NEXT;
}

static int op_delete(AG *g) {
    char name[32];
    snprintf(name, sizeof name, "SAVE%02d.DAT", arg_i(g, 2));
    char *p = save_path(g, name);
    int a = unlink(p);
    free(p);
    snprintf(name, sizeof name, "SAVE%02d.STH", arg_i(g, 2));
    p = save_path(g, name);
    int b = unlink(p);
    free(p);
    arg_set_i(g, 1, b ? 2 : a ? 1 : 0);
    return OP_NEXT;
}

static bool copy_file(AG *g, const char *from, const char *to) {
    char *a = save_path(g, from), *b = save_path(g, to);
    size_t n = 0;
    u8 *d = ag_read_file(a, &n);
    bool ok = d && ag_write_file(b, d, n);
    free(d);
    free(a);
    free(b);
    return ok;
}

static int op_copy(AG *g) {
    char a[32], b[32], c[32], d[32];
    snprintf(a, sizeof a, "SAVE%02d.DAT", arg_i(g, 2));
    snprintf(b, sizeof b, "SAVE%02d.DAT", arg_i(g, 3));
    snprintf(c, sizeof c, "SAVE%02d.STH", arg_i(g, 2));
    snprintf(d, sizeof d, "SAVE%02d.STH", arg_i(g, 3));
    bool x = copy_file(g, a, b), y = copy_file(g, c, d);
    arg_set_i(g, 1, !y ? 2 : !x ? 1 : 0);
    return OP_NEXT;
}

static int op_thumb_write(AG *g) {
    Image *im = gfx_surface(g, arg_i(g, 3));
    if (!im) { arg_set_i(g, 1, 2); return OP_NEXT; }
    write_sth(g, arg_i(g, 2), im);
    arg_set_i(g, 1, 0);
    return OP_NEXT;
}

static int op_thumb_read(AG *g) {
    char name[32];
    snprintf(name, sizeof name, "SAVE%02d.STH", arg_i(g, 2));
    char *p = save_path(g, name);
    size_t n = 0;
    u8 *d = ag_read_file(p, &n);
    free(p);
    if (!d) { arg_set_i(g, 1, 1); return OP_NEXT; }
    Image *im = agf_decode(d, n);
    free(d);
    if (!im) { arg_set_i(g, 1, 2); return OP_NEXT; }
    gfx_set_surface(g, arg_i(g, 3), im, -1, true);
    arg_set_i(g, 1, 0);
    return OP_NEXT;
}

static int op_unlocked(AG *g) {
    u32 id = (u32)arg_i(g, 2);
    int sel = (int)(id >> 24);
    u32 i = id & 0xffffff;
    arg_set_i(g, 1, g->st.seen[sel] && i < (u32)g->st.cat[sel].count && g->st.seen[sel][i]);
    return OP_NEXT;
}

void save_register(void) {
    vm_register(0x19d, op_unlocked);
    vm_register(0x19e, op_save);
    vm_register(0x1a0, op_meta);
    vm_register(0x1a1, op_load);
    vm_register(0x1ab, op_delete);
    vm_register(0x1ac, op_copy);
    vm_register(0x1ae, op_thumb_write);
    vm_register(0x1af, op_thumb_read);
    vm_register(0xae, op_restore_continue);
}
