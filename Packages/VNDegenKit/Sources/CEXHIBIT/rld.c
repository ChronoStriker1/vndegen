#include "ex.h"

#include <dirent.h>
#include <pthread.h>

#define RLD_FIXED_KEY 0xae85a916u
#define RLD_WORDS 0x3ff0

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static void key_stream(u32 key, u32 out[256]) {
    MT m;
    mt_seed(&m, key);
    for (int i = 0; i < 256; i++) out[i] = mt_next(&m) ^ key;
}

static bool key_fits(u32 key, const u8 *d) {
    u32 s[256];
    key_stream(key, s);
    for (int i = 3; i < 64; i++)
        if (rd32(d + 16 + 4 * i) != s[i]) return false;
    return true;
}

static u32 lcgA[1300], lcgB[1300];

static u32 state_word(u32 k, int i) {
    u32 a = lcgA[2 * i] * k + lcgB[2 * i], b = lcgA[2 * i + 1] * k + lcgB[2 * i + 1];
    return (b >> 16) | (a & 0xffff0000u);
}

static u32 first_output(u32 k, int i) {
    u32 y = (state_word(k, i) & 0x80000000u) | (state_word(k, i + 1) & 0x7fffffffu);
    y = state_word(k, i + 397) ^ (y >> 1) ^ ((y & 1) ? 0x9908b0dfu : 0);
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y ^ k;
}

typedef struct Search {
    const u8 *data;
    u32 target;
    u64 from, to;
    volatile int *found;
    u32 *key;
    pthread_mutex_t *lock;
} Search;

static void *search_part(void *arg) {
    Search *s = arg;
    for (u64 kk = s->from; kk < s->to; kk++) {
        if ((kk & 0xfffff) == 0 && *s->found) break;
        u32 k = (u32)kk;
        if (first_output(k, 5) != s->target) continue;
        if (!key_fits(k, s->data)) continue;
        pthread_mutex_lock(s->lock);
        *s->found = 1;
        *s->key = k;
        pthread_mutex_unlock(s->lock);
        break;
    }
    return NULL;
}

static bool search_key(EX *e, const u8 *d, u32 *key) {
    lcgA[0] = 1;
    lcgB[0] = 0;
    for (int n = 1; n < 1300; n++) {
        lcgA[n] = lcgA[n - 1] * 69069u;
        lcgB[n] = lcgB[n - 1] * 69069u + 1;
    }
    enum { T = 8 };
    pthread_t th[T];
    Search s[T];
    volatile int found = 0;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    u64 span = 0x100000000ull / T;
    for (int i = 0; i < T; i++) {
        s[i] = (Search){d, rd32(d + 16 + 20), span * (u64)i, i == T - 1 ? 0x100000000ull : span * (u64)(i + 1), &found, key, &lock};
        pthread_create(&th[i], NULL, search_part, &s[i]);
    }
    for (int i = 0; i < T; i++) pthread_join(th[i], NULL);
    ex_log(e, found ? "found the script key %08x" : "no script key fits", *key);
    return found;
}

static char *key_cache_path(EX *e) { return ex_fmt("%s/exhibit-keys.txt", e->cacheDir ? e->cacheDir : e->saveDir); }

static void keys_load(EX *e) {
    e->keys[e->keyCount++] = RLD_FIXED_KEY;
    char *path = key_cache_path(e);
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    if (!d) return;
    for (char *p = d; *p && e->keyCount < 8;) {
        char *end;
        unsigned long v = strtoul(p, &end, 16);
        if (end == p) break;
        e->keys[e->keyCount++] = (u32)v;
        p = end;
        while (*p == '\n' || *p == ' ' || *p == '\r') p++;
    }
    free(d);
}

static void keys_save(EX *e) {
    char buf[128] = {0};
    size_t o = 0;
    for (int i = 1; i < e->keyCount; i++) o += (size_t)snprintf(buf + o, sizeof buf - o, "%08x\n", e->keys[i]);
    char *path = key_cache_path(e);
    ex_write_file(path, buf, o);
    free(path);
}

static bool decrypt(EX *e, u8 *d, size_t n) {
    if (n < 16 || memcmp(d, "\0DLR", 4)) return false;
    if (rd32(d + 4) < 3 || n < 16 + 256) return true;
    u32 key = 0;
    bool have = false;
    for (int i = 0; i < e->keyCount && !have; i++)
        if (key_fits(e->keys[i], d)) {
            key = e->keys[i];
            have = true;
        }
    if (!have) {
        if (e->keyCount >= 8 || !search_key(e, d, &key)) return false;
        e->keys[e->keyCount++] = key;
        keys_save(e);
    }
    u32 s[256];
    key_stream(key, s);
    size_t words = (n - 16) / 4;
    if (words > RLD_WORDS) words = RLD_WORDS;
    for (size_t i = 0; i < words; i++) {
        u8 *p = d + 16 + 4 * i;
        u32 w = rd32(p) ^ s[i & 255];
        p[0] = (u8)w;
        p[1] = (u8)(w >> 8);
        p[2] = (u8)(w >> 16);
        p[3] = (u8)(w >> 24);
    }
    return true;
}

static bool parse(Script *sc, size_t n) {
    u8 *d = sc->data;
    size_t o = rd32(d + 8);
    int cap = 64;
    sc->ops = ex_alloc(sizeof(Op) * (size_t)cap);
    while (o + 4 <= n) {
        u32 w = rd32(d + o);
        o += 4;
        if (sc->count == cap) {
            cap *= 2;
            sc->ops = ex_realloc(sc->ops, sizeof(Op) * (size_t)cap);
        }
        Op *op = &sc->ops[sc->count++];
        memset(op, 0, sizeof *op);
        op->op = (u16)(w & 0xffff);
        op->nv = (u8)((w >> 16) & 0xff);
        op->ns = (u8)((w >> 24) & 0xf);
        op->flags = (u8)(w >> 28);
        if (o + 4u * op->nv > n) return false;
        op->v = ex_alloc(sizeof(i32) * (op->nv ? op->nv : 1));
        for (int i = 0; i < op->nv; i++, o += 4) op->v[i] = (i32)rd32(d + o);
        op->s = ex_alloc(sizeof(char *) * (op->ns ? op->ns : 1));
        for (int i = 0; i < op->ns; i++) {
            u8 *z = memchr(d + o, 0, n - o);
            if (!z) return false;
            op->s[i] = (char *)(d + o);
            o = (size_t)(z - d) + 1;
        }
    }
    return o == n;
}

static void index_labels(Script *sc) {
    for (int i = 0; i < sc->count; i++) {
        Op *op = &sc->ops[i];
        if (op->op == 5 && op->ns) {
            int id = field_int(op->s[0]);
            if (id >= 0) {
                sc->labels = ex_realloc(sc->labels, sizeof(Label) * (size_t)(sc->labelCount + 1));
                sc->labels[sc->labelCount++] = (Label){id, i};
            }
        } else if (op->op == 47 && op->nv) {
            sc->funcs = ex_realloc(sc->funcs, sizeof(Label) * (size_t)(sc->funcCount + 1));
            sc->funcs[sc->funcCount++] = (Label){op->v[0], i};
        }
    }
}

int script_label(Script *sc, int id) {
    for (int i = 0; sc && i < sc->labelCount; i++)
        if (sc->labels[i].id == id) return sc->labels[i].pc;
    return -1;
}

int script_func(Script *sc, int id) {
    for (int i = 0; sc && i < sc->funcCount; i++)
        if (sc->funcs[i].id == id) return sc->funcs[i].pc;
    return -1;
}

static Script *load(EX *e, const char *name) {
    char *rel = ex_fmt("rld/%s.rld", name);
    char *path = ex_game_path(e, rel);
    free(rel);
    size_t n;
    u8 *d = ex_read_file(path, &n);
    free(path);
    if (!d) {
        ex_log(e, "missing script %s", name);
        return NULL;
    }
    if (!decrypt(e, d, n)) {
        ex_log(e, "can't decrypt script %s", name);
        free(d);
        return NULL;
    }
    Script *sc = ex_alloc(sizeof *sc);
    sc->name = ex_strdup(name);
    sc->data = d;
    if (!parse(sc, n)) ex_log(e, "script %s doesn't parse to its end", name);
    if (n >= 20 && rd32(d + 16) == 1 && d[20]) sc->library = ex_strdup((char *)d + 20);
    index_labels(sc);
    return sc;
}

bool rld_init(EX *e) {
    e->keyCount = 0;
    keys_load(e);
    return true;
}

Script *rld_get(EX *e, const char *name) {
    for (int i = 0; i < e->scriptCount; i++)
        if (ex_ieq(e->scripts[i]->name, name)) return e->scripts[i];
    Script *sc = load(e, name);
    if (!sc) return NULL;
    e->scripts = ex_realloc(e->scripts, sizeof(Script *) * (size_t)(e->scriptCount + 1));
    e->scripts[e->scriptCount++] = sc;
    return sc;
}

void rld_free_all(EX *e) {
    for (int i = 0; i < e->scriptCount; i++) {
        Script *sc = e->scripts[i];
        for (int j = 0; j < sc->count; j++) {
            free(sc->ops[j].v);
            free(sc->ops[j].s);
        }
        free(sc->ops);
        free(sc->labels);
        free(sc->funcs);
        free(sc->library);
        free(sc->name);
        free(sc->data);
        free(sc);
    }
    free(e->scripts);
    e->scripts = NULL;
    e->scriptCount = 0;
}
