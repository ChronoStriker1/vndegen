#include "yr.h"
#include "vm.h"
#include "gfx.h"
#include <sys/stat.h>
#include <zlib.h>

static u32 file_version(YR *y);

enum { SLOTS = 1024, KIND_VAR = 0, KIND_LAYER = 1, KIND_GROUP = 2 };

typedef struct Rec {
    u32 dno;
    u8 kind;
    u16 vid;
    u8 *data;
    u32 size;
} Rec;

typedef struct RecList {
    Rec *recs;
    int n, cap;
} RecList;

typedef struct MemSlot {
    i64 id;
    RecList list;
} MemSlot;

struct YSaveState {
    RecList reserve;
    MemSlot *mems;
    int nmems;
    bool embedMems;
};

enum { MEM_SLOTS = 500, MEM_DNO = 1024, MEM_ALL = 99999 };

typedef struct LayerFormat {
    u32 version;
    u32 attrLen;
    u32 wOff, hOff;
    const u32 (*fields)[2];
    int nfields;
} LayerFormat;

static const u32 fields192[][2] = { { 80, 1 }, { 112, 0x1000000 }, { 120, 256 }, { 128, 0x11011130 }, { 476, 128 },
                                    { 480, 0x808080 } };
static const u32 fields479[][2] = { { 80, 1 }, { 120, 0x1000100 }, { 128, 256 }, { 132, 0x10000 }, { 492, 128 },
                                    { 496, 0x808080 } };
static const u32 fields406[][2] = { { 576, 1 }, { 608, 0x1000000 }, { 616, 256 }, { 972, 128 }, { 976, 0x808080 } };
static const u32 fields473[][2] = { { 80, 1 }, { 120, 0x1000000 }, { 128, 256 }, { 492, 128 }, { 496, 0x808080 } };
static const u32 fields312[][2] = { { 80, 1 }, { 128, 0x1000100 }, { 136, 256 }, { 140, 0x10000 }, { 492, 128 },
                                    { 496, 0x808080 } };
static const u32 fields480[][2] = { { 80, 1 }, { 128, 0x1000100 }, { 132, 0x10000 }, { 136, 256 }, { 448, 10000 },
                                    { 456, 10000 }, { 516, 128 }, { 520, 0x808080 }, { 1568, 0x3f800000 },
                                    { 1572, 0x3f800000 }, { 1576, 0x3f800000 }, { 1580, 1 } };
static const u32 fields555[][2] = { { 88, 1 }, { 128, 128 }, { 132, 0x808080 }, { 160, 1 }, { 164, 256 },
                                    { 168, 0x1000000 }, { 552, 10000 }, { 560, 10000 }, { 620, 128 }, { 624, 0x808080 } };
static const LayerFormat layerFormats[] = {
    { 192, 1520, 104, 108, fields192, 6 },
    { 312, 1536, 120, 124, fields312, 6 },
    { 480, 1600, 120, 124, fields480, 12 },
    { 555, 1680, 136, 140, fields555, 10 },
    { 473, 1536, 112, 116, fields473, 5 },
    { 406, 992, 600, 604, fields406, 5 },
    { 479, 1536, 112, 116, fields479, 6 },
};

u32 yr_save_version(YR *y) { return file_version(y); }

static u32 file_version(YR *y) {
    u32 v = y->scriptVersion;
    if (v < 470) return 192;
    if (v < 480) return 312;
    if (v == 481) return 481;
    return 480;
}

static bool uses_snappy(YR *y) { return y->scriptVersion == 481 || y->scriptVersion >= 500; }

static const LayerFormat *layer_format(YR *y) {
    u32 version = file_version(y);
    if (y->scriptVersion >= 500) return &layerFormats[3];
    if (version == 312 && y->scriptVersion < 474) return &layerFormats[4];
    if (version == 312 && y->scriptVersion > 474) return &layerFormats[6];
    if (y->scriptVersion < 466) return &layerFormats[5];
    return version < 300 ? &layerFormats[0] : version < 400 ? &layerFormats[1] : &layerFormats[2];
}

static u8 *unpack(const u8 *z, u32 clen, u32 ulen, u32 *got) {
    if (clen >= 2 && (z[0] & 0x0f) == 8 && ((z[0] << 8) | z[1]) % 31 == 0 && ulen <= (u64)clen * 1032 + 64) {
        u8 *raw = yr_alloc(ulen ? ulen : 1);
        uLongf n = ulen;
        if (uncompress(raw, &n, z, clen) == Z_OK) { *got = (u32)n; return raw; }
        free(raw);
    }
    size_t n = 0;
    u8 *raw = yr_snappy_decode(z, clen, &n);
    *got = raw ? (u32)n : 0;
    return raw;
}

static u8 *pack(const u8 *d, size_t n, bool snappy, u32 *clen) {
    if (snappy) {
        size_t m;
        u8 *z = yr_snappy_encode(d, n, &m);
        *clen = (u32)m;
        return z;
    }
    uLongf m = compressBound(n);
    u8 *z = yr_alloc(m);
    compress2(z, &m, d, n, 6);
    *clen = (u32)m;
    return z;
}

static struct YSaveState *state(YR *y) {
    if (!y->save) y->save = yr_alloc(sizeof(struct YSaveState));
    return y->save;
}

static void list_clear(RecList *l) {
    for (int i = 0; i < l->n; i++) free(l->recs[i].data);
    free(l->recs);
    memset(l, 0, sizeof *l);
}

static Rec *list_add(RecList *l) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 32;
        l->recs = yr_realloc(l->recs, sizeof(Rec) * (size_t)l->cap);
    }
    Rec *r = &l->recs[l->n++];
    memset(r, 0, sizeof *r);
    return r;
}

static Rec *list_find(RecList *l, u32 dno) {
    for (int i = 0; i < l->n; i++)
        if (l->recs[i].dno == dno) return &l->recs[i];
    return NULL;
}

static void list_copy(RecList *dst, const RecList *src) {
    list_clear(dst);
    for (int i = 0; i < src->n; i++) {
        Rec *r = list_add(dst);
        *r = src->recs[i];
        r->data = yr_alloc(r->size ? r->size : 1);
        memcpy(r->data, src->recs[i].data, r->size);
    }
}

void yr_save_close(YR *y) {
    if (!y->save) return;
    list_clear(&y->save->reserve);
    for (int i = 0; i < y->save->nmems; i++) list_clear(&y->save->mems[i].list);
    free(y->save->mems);
    free(y->save);
    y->save = NULL;
}

typedef struct Buf {
    u8 *p;
    size_t n, cap;
} Buf;

static void put(Buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 64;
        b->p = yr_realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}

static void put32(Buf *b, u32 v) {
    u8 x[4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) };
    put(b, x, 4);
}

static void put64(Buf *b, u64 v) {
    put32(b, (u32)v);
    put32(b, (u32)(v >> 32));
}

static Buf var_payload(const YVar *v, long index) {
    Buf b = { 0 };
    u8 t = v->t ? v->t : YT_INT;
    bool whole = index < 0;
    u32 count = whole ? v->count : 1;
    put32(&b, t);
    put32(&b, whole ? v->ndim : 0);
    if (whole)
        for (int i = 0; i < v->ndim; i++) put32(&b, v->dims[i]);
    size_t sizeAt = b.n;
    put32(&b, 0);
    size_t start = b.n;
    for (u32 i = 0; i < count; i++) {
        long k = whole ? (long)i : index;
        if (k < 0 || k >= (long)v->count) {
            if (t == YT_STR) put32(&b, 0);
            else put64(&b, 0);
            continue;
        }
        if (t == YT_INT) put64(&b, (u64)v->ints[k]);
        else if (t == YT_FLT) { u64 bits; memcpy(&bits, &v->flts[k], 8); put64(&b, bits); }
        else {
            const char *s = v->strs[k] ? v->strs[k] : "";
            u32 n = (u32)strlen(s);
            put32(&b, n);
            put(&b, s, n);
        }
    }
    u32 size = (u32)(b.n - start);
    memcpy(b.p + sizeAt, &size, 4);
    return b;
}

static Buf scalar_payload(const YVal *v) {
    Buf b = { 0 };
    u8 t = v->t ? v->t : YT_INT;
    put32(&b, t);
    put32(&b, 0);
    if (t == YT_STR) {
        const char *s = v->s ? v->s : "";
        u32 n = (u32)strlen(s);
        put32(&b, n + 4);
        put32(&b, n);
        put(&b, s, n);
    } else {
        put32(&b, 8);
        if (t == YT_FLT) { u64 bits; memcpy(&bits, &v->f, 8); put64(&b, bits); }
        else put64(&b, (u64)v->i);
    }
    return b;
}

static void apply_payload(YR *y, YTask *t, const u8 *d, u32 n, YVar *var, long index, u32 vid) {
    if (n < 12) return;
    u32 type = yr_le32(d), nd = yr_le32(d + 4);
    if (nd > 8 || 12 + 4 * nd > n) return;
    u32 dims[8];
    for (u32 i = 0; i < nd; i++) dims[i] = yr_le32(d + 8 + 4 * i);
    const u8 *p = d + 12 + 4 * nd, *end = d + n;
    if (!var) return;
    if (index < 0 && (nd || !var->count)) {
        u8 vt = var->init.t ? var->init.t : var->t ? var->t : (u8)type;
        bool same = var->count && var->ndim == nd && var->t == vt;
        for (u32 i = 0; same && i < nd; i++) same = var->dims[i] == dims[i];
        if (!same) yr_var_declare(y, var, vt, (u8)nd, dims, NULL);
    }
    u32 count = 1;
    for (u32 i = 0; i < nd; i++) count *= dims[i] ? dims[i] : 1;
    for (u32 i = 0; i < count && p < end; i++) {
        YVal v;
        if (type == YT_STR) {
            if (p + 4 > end) break;
            u32 len = yr_le32(p);
            if (p + 4 + len > end) break;
            v = yv_str_own(yr_strndup((const char *)p + 4, len));
            p += 4 + len;
        } else {
            if (p + 8 > end) break;
            u64 bits = yr_le64(p);
            if (type == YT_FLT) { double f; memcpy(&f, &bits, 8); v = yv_flt(f); }
            else v = yv_int((i64)bits);
            p += 8;
        }
        long k = index >= 0 ? index : (long)i;
        if (vid < 1000 && yr_sys_dynamic(&y->sys[vid])) { i64 sub = k; yr_sys_set(y, t, &y->sys[vid], 0, &sub, &v); }
        else yr_var_set(var, k, &v);
        yv_free(&v);
        if (index >= 0) break;
    }
}

static const char *const classKeys[] = { "G_INT", "G_FLT", "G_STR", "G_INT2", "G_FLT2", "G_STR2", "G_INT3", "G_FLT3",
                                         "G_STR3", "G_INT4", "G_FLT4", "G_STR4" };

static bool class_of(YCall *c, u8 *cls, u8 *type) {
    for (int i = 0; i < 12; i++)
        if (yc_int_or(c, classKeys[i], 0)) {
            *cls = (u8)(i / 3 + 1);
            *type = (u8)(i % 3 + 1);
            return true;
        }
    return false;
}

static Buf group_payload(YR *y, u8 cls, u8 type) {
    Buf b = { 0 };
    for (u32 id = 1000; id < y->varCount; id++) {
        YVar *v = &y->vars[id];
        if (v->scope != 1 || v->cls != cls || (v->t ? v->t : v->init.t) != type) continue;
        Buf part = var_payload(v, -1);
        u32 clen;
        u8 *z = pack(part.p, part.n, uses_snappy(y), &clen);
        u16 vid = file_version(y) >= 480 ? (u16)id : 0;
        u8 head[12] = { KIND_VAR, 1, (u8)vid, (u8)(vid >> 8) };
        memcpy(head + 4, &(u32){ (u32)clen }, 4);
        memcpy(head + 8, &(u32){ (u32)part.n }, 4);
        put(&b, head, 12);
        put(&b, z, clen);
        free(z);
        free(part.p);
    }
    return b;
}

static void apply_group(YR *y, YTask *t, const u8 *d, u32 n, u32 onlyVid, YVar *onlyVar, long onlyIndex) {
    const u8 *p = d, *end = d + n;
    while (p + 12 <= end) {
        u16 vid = yr_le16(p + 2);
        u32 clen = yr_le32(p + 4), ulen = yr_le32(p + 8);
        if (clen > (size_t)(end - p) - 12) break;
        if (!onlyVar || vid == onlyVid) {
            u32 got;
            u8 *raw = unpack(p + 12, clen, ulen, &got);
            if (raw) apply_payload(y, t, raw, got, onlyVar ? onlyVar : yr_var(y, vid), onlyVar ? onlyIndex : -1, vid);
            free(raw);
        }
        p += 12 + clen;
    }
}

static Buf layer_payload(YR *y, YLayer *l) {
    Buf b = { 0 };
    const LayerFormat *f = layer_format(y);
    int w = 0, h = 0;
    bool owned = false;
    uint32_t *px = NULL;
    if (l->isSet) {
        w = l->sx > 0 ? l->sx : y->width;
        h = l->sy > 0 ? l->sy : y->height;
        px = yr_render_set(y, l, w, h);
        owned = true;
    } else if (l->px) {
        w = l->w;
        h = l->h;
        px = l->px;
    }
    u8 *attr = yr_alloc(f->attrLen);
    for (int i = 0; i < f->nfields; i++) memcpy(attr + f->fields[i][0], &f->fields[i][1], 4);
    memcpy(attr + f->wOff, &(u32){ (u32)w }, 4);
    memcpy(attr + f->hOff, &(u32){ (u32)h }, 4);
    if (f->version == 555) {
        memcpy(attr + 240, &(u32){ (u32)w }, 4);
        memcpy(attr + 244, &(u32){ (u32)h }, 4);
    }
    put32(&b, f->attrLen);
    put32(&b, (u32)w * (u32)h * 4);
    put(&b, attr, f->attrLen);
    free(attr);
    for (size_t i = 0; px && i < (size_t)w * (size_t)h; i++) {
        u32 p = px[i];
        u8 q[4] = { (u8)(p >> 16), (u8)(p >> 8), (u8)p, (u8)(p >> 24) };
        put(&b, q, 4);
    }
    if (owned) free(px);
    return b;
}

static void apply_layer(YR *y, const u8 *d, u32 n, const char *id) {
    if (n < 8) return;
    u32 attrLen = yr_le32(d), pixLen = yr_le32(d + 4);
    if ((u64)8 + attrLen + pixLen > n) return;
    const u8 *attr = d + 8;
    u32 w = 0, h = 0;
    for (u32 o = 0; o + 8 <= attrLen; o += 4) {
        u32 a = yr_le32(attr + o), b = yr_le32(attr + o + 4);
        if (a && b && a <= 8192 && b <= 8192 && (u64)a * b * 4 == pixLen) { w = a; h = b; break; }
    }
    if (!w) return;
    YLayer *l = yr_layer(y, id, true, false);
    if (!l || l->isSet) return;
    yr_layer_pixels(l, (int)w, (int)h);
    const u8 *p = attr + attrLen;
    for (size_t i = 0; i < (size_t)w * h; i++, p += 4) l->px[i] = (u32)p[2] | (u32)p[1] << 8 | (u32)p[0] << 16 | (u32)p[3] << 24;
    yr_layer_check_opaque(l);
    y->gfx->dirty = true;
}

static char *sd_path(YR *y, const char *file) {
    char name[1024];
    size_t n = strlen(file);
    bool hasExt = n > 3 && !strcasecmp(file + n - 3, ".sd");
    snprintf(name, sizeof name, "%s%s", file, hasExt ? "" : ".sd");
    return yr_save_path(y, name);
}

static Buf build_file(YR *y, const RecList *l) {
    Buf b = { 0 };
    put(&b, "YSSD", 4);
    put32(&b, file_version(y));
    put32(&b, (u32)l->n);
    put32(&b, SLOTS);
    size_t table = b.n;
    for (int i = 0; i < SLOTS; i++) put32(&b, 0);
    for (int i = 0; i < l->n; i++) {
        const Rec *r = &l->recs[i];
        if (r->dno < 1 || r->dno > SLOTS) continue;
        u32 off = (u32)b.n;
        memcpy(b.p + table + 4 * (r->dno - 1), &off, 4);
        u8 head[16];
        memcpy(head, &r->dno, 4);
        head[4] = r->kind;
        head[5] = 1;
        u16 vid = file_version(y) >= 480 ? r->vid : 0;
        head[6] = (u8)vid;
        head[7] = (u8)(vid >> 8);
        if (r->kind == KIND_GROUP || r->dno == MEM_DNO) {
            memcpy(head + 8, &r->size, 4);
            u32 total = 0;
            if (r->dno == MEM_DNO)
                for (const u8 *p = r->data; p + 8 <= r->data + r->size; p += 8 + yr_le32(p + 4)) total += 8 + yr_le32(p);
            else
                for (const u8 *p = r->data; p + 12 <= r->data + r->size; p += 12 + yr_le32(p + 4)) total += 12 + yr_le32(p + 8);
            memcpy(head + 12, &total, 4);
            put(&b, head, 16);
            put(&b, r->data, r->size);
        } else {
            u32 clen;
            u8 *z = pack(r->data, r->size, uses_snappy(y) && r->kind != KIND_LAYER, &clen);
            memcpy(head + 8, &clen, 4);
            memcpy(head + 12, &r->size, 4);
            put(&b, head, 16);
            put(&b, z, clen);
            free(z);
        }
    }
    return b;
}

static bool write_file(YR *y, const char *file, const RecList *l) {
    char *path = sd_path(y, file);
    if (!path) return false;
    Buf b = build_file(y, l);
    char *slash = strrchr(path, '/');
    if (slash) {
        *slash = 0;
        mkdir(path, 0755);
        *slash = '/';
    }
    bool ok = yr_write_file(path, b.p, b.n);
    free(b.p);
    free(path);
    return ok;
}

static bool parse_file(const u8 *d, size_t size, RecList *out) {
    bool ok = size >= 16 && !memcmp(d, "YSSD", 4);
    u32 slots = ok ? yr_le32(d + 12) : 0;
    if (ok && 16 + (size_t)slots * 4 > size) ok = false;
    for (u32 i = 0; ok && i < slots; i++) {
        u32 off = yr_le32(d + 16 + 4 * i);
        if (!off || (size_t)off + 16 > size) continue;
        u32 clen = yr_le32(d + off + 8), ulen = yr_le32(d + off + 12);
        if ((size_t)off + 16 + clen > size) continue;
        Rec *r = list_add(out);
        r->dno = yr_le32(d + off);
        r->kind = d[off + 4];
        r->vid = yr_le16(d + off + 6);
        if (r->kind == KIND_GROUP || r->dno == MEM_DNO) {
            r->data = yr_alloc(clen ? clen : 1);
            memcpy(r->data, d + off + 16, clen);
            r->size = clen;
        } else {
            u32 got;
            r->data = unpack(d + off + 16, clen, ulen, &got);
            if (!r->data) r->data = yr_alloc(1);
            r->size = got;
        }
    }
    return ok;
}

static bool read_file(YR *y, const char *file, RecList *out) {
    char *path = sd_path(y, file);
    if (!path) return false;
    size_t size;
    u8 *d = yr_read_file(path, &size);
    free(path);
    if (!d) return false;
    bool ok = parse_file(d, size, out);
    free(d);
    return ok;
}

static MemSlot *mem_slot(YR *y, i64 id, bool create) {
    struct YSaveState *s = state(y);
    for (int i = 0; i < s->nmems; i++)
        if (s->mems[i].id == id) return &s->mems[i];
    if (!create) return NULL;
    s->mems = yr_realloc(s->mems, sizeof(MemSlot) * (size_t)(s->nmems + 1));
    MemSlot *m = &s->mems[s->nmems++];
    memset(m, 0, sizeof *m);
    m->id = id;
    return m;
}

static Rec mems_record(YR *y) {
    Buf b = { 0 };
    for (i64 k = 0; k < MEM_SLOTS; k++) {
        MemSlot *m = mem_slot(y, k, false);
        if (!m || !m->list.n) { put32(&b, 0); put32(&b, 0); continue; }
        Buf f = build_file(y, &m->list);
        u32 clen;
        u8 *z = pack(f.p, f.n, false, &clen);
        put32(&b, (u32)f.n);
        put32(&b, clen);
        put(&b, z, clen);
        free(z);
        free(f.p);
    }
    Rec r = { MEM_DNO, KIND_VAR, 0, b.p, (u32)b.n };
    return r;
}

static void restore_mems(YR *y, const Rec *r) {
    const u8 *p = r->data, *end = r->data + r->size;
    for (i64 k = 0; k < MEM_SLOTS && p + 8 <= end; k++) {
        u32 ulen = yr_le32(p), clen = yr_le32(p + 4);
        if (clen > (size_t)(end - p) - 8) break;
        MemSlot *m = mem_slot(y, k, ulen != 0);
        if (m) list_clear(&m->list);
        if (ulen && m) {
            u32 got;
            u8 *raw = unpack(p + 8, clen, ulen, &got);
            if (raw) parse_file(raw, got, &m->list);
            free(raw);
        }
        p += 8 + clen;
    }
}

int yr_cmd_save(YCall *c, int id) {
    YR *y = c->y;
    struct YSaveState *s = state(y);
    if (id == C_SAVEEND) {
        list_clear(&s->reserve);
        return YR_NEXT;
    }
    if (id == C_SAVE) {
        i64 dno;
        if (yc_int(c, "DNO", &dno) && dno >= 1 && dno <= SLOTS) {
            Rec *old = list_find(&s->reserve, (u32)dno);
            Rec *r = old ? old : list_add(&s->reserve);
            if (old) free(old->data);
            memset(r, 0, sizeof *r);
            r->dno = (u32)dno;
            YRef ref;
            u8 cls, type;
            Buf b = { 0 };
            char *lid = yc_str(c, "ID");
            if (lid) {
                YLayer *l = yr_layer(y, lid, false, false);
                if (l) b = layer_payload(y, l);
                r->kind = KIND_LAYER;
                free(lid);
            } else if (class_of(c, &cls, &type)) {
                b = group_payload(y, cls, type);
                r->kind = KIND_GROUP;
            } else if (yc_ref(c, "SET", &ref)) {
                YVar *v = ref.var ? ref.var : yr_var(y, ref.vid);
                if (ref.vid < 1000 && yr_sys_dynamic(&y->sys[ref.vid])) {
                    YVal val = yr_ref_get(y, c->t, &ref);
                    b = scalar_payload(&val);
                    yv_free(&val);
                } else if (v) b = var_payload(v, ref.index);
                r->kind = KIND_VAR;
                r->vid = (u16)ref.vid;
            }
            r->data = b.p ? b.p : yr_alloc(1);
            r->size = (u32)b.n;
        }
        i64 mem;
        if (yc_int(c, "MEM", &mem)) {
            if (mem == MEM_ALL) s->embedMems = true;
            else {
                list_copy(&mem_slot(y, mem, true)->list, &s->reserve);
                list_clear(&s->reserve);
            }
        }
        char *file = yc_str(c, "FILE");
        if (file) {
            if (s->embedMems) {
                Rec *old = list_find(&s->reserve, MEM_DNO);
                if (old) free(old->data);
                *(old ? old : list_add(&s->reserve)) = mems_record(y);
                s->embedMems = false;
            }
            if (!write_file(y, file, &s->reserve)) yr_log(y, "SAVE: cannot write %s\n", file);
            list_clear(&s->reserve);
            free(file);
        }
        return YR_NEXT;
    }
    RecList tmp = { 0 }, *src = &tmp;
    i64 mem;
    char *file = yc_str(c, "FILE");
    if (yc_int(c, "MEM", &mem)) {
        MemSlot *m = mem_slot(y, mem, false);
        src = m ? &m->list : &tmp;
    } else if (!file || !read_file(y, file, &tmp)) {
        free(file);
        list_clear(&tmp);
        return YR_NEXT;
    }
    free(file);
    i64 dno = yc_int_or(c, "DNO", 0);
    Rec *r = list_find(src, (u32)dno);
    if (r && r->dno == MEM_DNO && !yc_has(c, "MEM")) {
        restore_mems(y, r);
        YRef ref;
        if (yc_ref(c, "LET", &ref)) { YVal one = yv_int(1); yr_ref_set(y, c->t, &ref, &one); }
    } else if (r) {
        YRef ref;
        char *lid = yc_str(c, "ID");
        u8 cls, type;
        if (lid) {
            if (r->kind == KIND_LAYER) apply_layer(y, r->data, r->size, lid);
            free(lid);
        } else if (r->kind == KIND_GROUP && class_of(c, &cls, &type)) {
            apply_group(y, c->t, r->data, r->size, 0, NULL, -1);
        } else if (yc_ref(c, "LET", &ref)) {
            YVar *v = ref.var ? ref.var : yr_var(y, ref.vid);
            if (r->kind == KIND_GROUP) apply_group(y, c->t, r->data, r->size, ref.vid, v, ref.index);
            else if (r->kind == KIND_VAR) apply_payload(y, c->t, r->data, r->size, v, ref.index, ref.vid);
        }
    }
    list_clear(&tmp);
    return YR_NEXT;
}

char *yr_save_path(YR *y, const char *name) {
    if (!name || !*name || name[0] == '/' || name[0] == '\\' || strchr(name, ':')) return NULL;
    char key[1024];
    snprintf(key, sizeof key, "%s", name);
    for (char *p = key; *p; p++) if (*p == '\\') *p = '/';
    if (strstr(key, "..")) return NULL;
    const char *rel = key;
    while (!strncmp(rel, "./", 2)) rel += 2;
    if (!strncasecmp(rel, "save/", 5)) rel += 5;
    char *found = yr_find_file(y->saveDir, rel);
    return found ? found : yr_path(y->saveDir, rel);
}

bool yr_save_exists(YR *y, const char *name) {
    char *p = yr_save_path(y, name);
    if (!p) return false;
    struct stat st;
    bool ok = stat(p, &st) == 0;
    free(p);
    return ok;
}

void yr_save_mkdir(YR *y, const char *name) {
    char *p = yr_save_path(y, name);
    if (!p) return;
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') p[--n] = 0;
    for (char *s = p + 1; *s; s++) {
        if (*s != '/') continue;
        *s = 0;
        mkdir(p, 0755);
        *s = '/';
    }
    mkdir(p, 0755);
    free(p);
}
