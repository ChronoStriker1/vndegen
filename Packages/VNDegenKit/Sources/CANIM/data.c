#include "anim.h"

#include <ctype.h>

void an_gax_crypt(u8 key[16], u8 *data, size_t size, bool encrypt) {
    u8 *k = key;
    size_t i = 0;
    while (i < size) {
        int j = 0;
        u8 m = 0;
        for (; j < 16 && i < size; j++, i++) {
            u8 c = data[i];
            data[i] = c ^ k[j];
            if (j == 14) m = encrypt ? c : data[i];
        }
        if (j < 16) break;
        switch (m & 7) {
        case 0:
            k[0] += m;
            k[3] += (u8)(m + 2);
            k[4] = (u8)(k[2] + m + 11);
            k[8] = (u8)(k[6] + 7);
            break;
        case 1:
            k[2] = (u8)(k[9] + k[10]);
            k[6] = (u8)(k[7] + k[15]);
            k[8] += k[1];
            k[15] = (u8)(k[3] + k[5]);
            break;
        case 2:
            k[1] += k[2];
            k[5] += k[6];
            k[7] += k[8];
            k[10] += k[11];
            break;
        case 3:
            k[9] = (u8)(k[1] + k[2]);
            k[11] = (u8)(k[5] + k[6]);
            k[12] = (u8)(k[7] + k[8]);
            k[13] = (u8)(k[10] + k[11]);
            break;
        case 4:
            k[0] = (u8)(k[1] + 0x6f);
            k[3] = (u8)(k[4] + 0x47);
            k[4] = (u8)(k[5] + 0x11);
            k[14] = (u8)(k[15] + 0x40);
            break;
        case 5:
            k[2] += k[10];
            k[4] = (u8)(k[5] + k[12]);
            k[6] = (u8)(k[8] + k[14]);
            k[8] = (u8)(k[0] + k[11]);
            break;
        case 6:
            k[9] = (u8)(k[1] + k[11]);
            k[11] = (u8)(k[3] + k[13]);
            k[13] = (u8)(k[5] + k[15]);
            k[15] = (u8)(k[7] + k[9]);
            k[1] = (u8)(k[5] + k[9]);
            k[2] = (u8)(k[6] + k[10]);
            k[3] = (u8)(k[7] + k[11]);
            k[4] = (u8)(k[8] + k[12]);
            break;
        case 7:
            k[1] = (u8)(k[5] + k[9]);
            k[2] = (u8)(k[6] + k[10]);
            k[3] = (u8)(k[7] + k[11]);
            k[4] = (u8)(k[8] + k[12]);
            break;
        }
    }
}

u8 *an_gax_open(const u8 *file, size_t size, size_t *outSize) {
    if (size < 20 || rd32(file) != 0x01000000u) return NULL;
    u8 key[16];
    memcpy(key, file + 4, 16);
    size_t n = size - 20;
    u8 *out = an_alloc(n + 1);
    memcpy(out, file + 20, n);
    an_gax_crypt(key, out, n, false);
    *outSize = n;
    return out;
}

static char *dup_string(const u8 *p, size_t max) {
    size_t n = 0;
    while (n < max && p[n]) n++;
    char *raw = an_alloc(n + 1);
    memcpy(raw, p, n);
    bool ascii = true;
    for (size_t i = 0; i < n; i++)
        if ((u8)raw[i] >= 0x80) ascii = false;
    if (ascii) return raw;
    char *u = an_sjis_to_utf8(raw);
    free(raw);
    return u;
}

Script *an_script_load(const char *path, char *err, size_t errSize) {
    size_t size;
    u8 *file = an_read_file(path, &size);
    if (!file) {
        snprintf(err, errSize, "can't read %s", path);
        return NULL;
    }
    size_t n;
    u8 *d = an_gax_open(file, size, &n);
    free(file);
    if (!d || n < 8) {
        free(d);
        snprintf(err, errSize, "%s isn't an ANIM script", path);
        return NULL;
    }
    Script *s = an_alloc(sizeof *s);
    s->data = d;
    s->size = n;
    s->codeEnd = rd32(d);
    s->tableEnd = rd32(d + 4);
    if (s->codeEnd > n || s->tableEnd > n || s->codeEnd > s->tableEnd) {
        snprintf(err, errSize, "%s has a bad header", path);
        an_script_free(s);
        return NULL;
    }
    int cap = 64;
    s->labelAddr = an_alloc(sizeof(u32) * (size_t)cap);
    s->labelName = an_alloc(sizeof(char *) * (size_t)cap);
    for (size_t p = s->codeEnd; p < s->tableEnd;) {
        const u8 *z = memchr(d + p, 0, s->tableEnd - p);
        if (!z || (size_t)(z - d) + 5 > s->tableEnd + 1) break;
        if (s->labelCount == cap) {
            cap *= 2;
            s->labelAddr = an_realloc(s->labelAddr, sizeof(u32) * (size_t)cap);
            s->labelName = an_realloc(s->labelName, sizeof(char *) * (size_t)cap);
        }
        s->labelName[s->labelCount] = dup_string(d + p, (size_t)(z - (d + p)));
        s->labelAddr[s->labelCount] = rd32(z + 1);
        s->labelCount++;
        p = (size_t)(z - d) + 5;
    }
    size_t p = s->tableEnd;
    int scap = 1024;
    s->strs = an_alloc(sizeof(char *) * (size_t)scap);
    while (p < n) {
        const u8 *z = memchr(d + p, 0, n - p);
        size_t len = z ? (size_t)(z - (d + p)) : n - p;
        if (s->strCount == scap) {
            scap *= 2;
            s->strs = an_realloc(s->strs, sizeof(char *) * (size_t)scap);
        }
        s->strs[s->strCount++] = dup_string(d + p, len);
        p += len + 1;
    }
    return s;
}

void an_script_free(Script *s) {
    if (!s) return;
    for (int i = 0; i < s->labelCount; i++) free(s->labelName[i]);
    for (int i = 0; i < s->strCount; i++) free(s->strs[i]);
    free(s->labelAddr);
    free(s->labelName);
    free(s->strs);
    free(s->data);
    free(s);
}

int an_script_label(const Script *s, const char *name) {
    for (int i = 0; i < s->labelCount; i++)
        if (!strcmp(s->labelName[i], name)) return (int)s->labelAddr[i];
    return -1;
}

const char *an_script_label_at(const Script *s, u32 pc) {
    const char *best = "-";
    u32 bestAddr = 0;
    for (int i = 0; i < s->labelCount; i++)
        if (s->labelAddr[i] <= pc && s->labelAddr[i] >= bestAddr) {
            bestAddr = s->labelAddr[i];
            best = s->labelName[i];
        }
    return best;
}

enum { SPRITE_RECORD = 524, SPRITE_NAME = 260 };

static bool sprite_name_ok(const u8 *p) {
    if (!(p[0] == 'p' || p[0] == 'q' || (p[0] == 's' && p[1] == 'p'))) return false;
    int n = 0;
    while (n < SPRITE_NAME && p[n]) {
        if (!isalnum(p[n]) && p[n] != '_') return false;
        n++;
    }
    return n > 0 && n < 64;
}

static bool sprite_file_ok(const u8 *p) {
    int n = 0;
    while (n < 16 && p[n]) {
        if (!isalnum(p[n])) return false;
        n++;
    }
    return n == 6 && !p[n];
}

static bool sprite_record(const u8 *d, size_t size, size_t at) {
    return at + SPRITE_NAME + 16 <= size && sprite_name_ok(d + at) && sprite_file_ok(d + at + SPRITE_NAME);
}

static size_t char_record(const u8 *d, size_t size, size_t at) {
    if (at + 16 > size || d[at] != 'M' || !islower(d[at + 1])) return 0;
    size_t n = 1;
    while (n < 15 && isalpha(d[at + n])) n++;
    if (n < 3 || d[at + n] != 0 || at + n + 9 > size) return 0;
    double v;
    memcpy(&v, d + at + n + 1, 8);
    return v >= 20.0 && v <= 1000.0 ? n : 0;
}

static const u8 *idle_field(const u8 *p, const u8 *end, bool number) {
    const u8 *z = memchr(p, 0, (size_t)(end - p));
    if (!z || z - p > 31) return NULL;
    for (const u8 *c = p; c < z; c++)
        if (number ? !(isdigit(*c) || *c == '-' || *c == '.') : !(isalnum(*c) || *c == '_')) return NULL;
    return z + 1;
}

static size_t idle_record(Define *def, const u8 *d, size_t size, size_t at) {
    const u8 *end = d + size, *p = d + at + 4;
    if (rd32(d + at) != 10 || p >= end || !isalpha(*p)) return 0;
    const u8 *q = idle_field(p, end, false);
    if (!q || q - p < 3) return 0;
    IdleMotion m = {dup_string(p, (size_t)(q - p)), NULL, 0};
    int cap = 0;
    while (q < end && *q) {
        const u8 *lo = idle_field(q, end, false), *hi = lo ? idle_field(lo, end, true) : NULL, *next = hi ? idle_field(hi, end, true) : NULL;
        if (!next) break;
        if (m.count == cap) {
            cap = cap ? cap * 2 : 8;
            m.vars = an_realloc(m.vars, sizeof(IdleVar) * (size_t)cap);
        }
        m.vars[m.count++] = (IdleVar){dup_string(q, (size_t)(lo - q)), (float)atof((const char *)lo), (float)atof((const char *)hi)};
        q = next;
    }
    if (q >= end || *q || !m.count) {
        for (int i = 0; i < m.count; i++) free(m.vars[i].name);
        free(m.vars);
        free(m.name);
        return 0;
    }
    def->idles = an_realloc(def->idles, sizeof(IdleMotion) * (size_t)(def->idleCount + 1));
    def->idles[def->idleCount++] = m;
    return (size_t)(q + 1 - d);
}

bool an_define_load(Define *def, const char *path) {
    memset(def, 0, sizeof *def);
    size_t size;
    u8 *file = an_read_file(path, &size);
    if (!file) return false;
    def->data = an_gax_open(file, size, &def->size);
    free(file);
    if (!def->data) return false;
    const u8 *d = def->data;
    size_t best = 0;
    int bestCount = 0;
    for (size_t at = 0; at + SPRITE_NAME + 16 <= def->size; at++) {
        if (!sprite_record(d, def->size, at)) continue;
        if (at >= SPRITE_RECORD && sprite_record(d, def->size, at - SPRITE_RECORD)) continue;
        int count = 0;
        while (sprite_record(d, def->size, at + (size_t)count * SPRITE_RECORD)) count++;
        if (count > bestCount) {
            bestCount = count;
            best = at;
        }
    }
    def->sprites = an_alloc(sizeof(SpriteName) * (size_t)(bestCount ? bestCount : 1));
    for (int i = 0; i < bestCount; i++) {
        const u8 *r = d + best + (size_t)i * SPRITE_RECORD;
        def->sprites[i].name = dup_string(r, SPRITE_NAME);
        def->sprites[i].file = dup_string(r + SPRITE_NAME, SPRITE_NAME);
    }
    def->spriteCount = bestCount;
    size_t bestRun = 0, runStart = 0;
    for (size_t at = 1; at + 16 < def->size; at++) {
        if (d[at - 1] != 0) continue;
        int count = 0;
        for (size_t p = at, n; (n = char_record(d, def->size, p)) != 0 && count < 16; p += n + 9) count++;
        if ((size_t)count > bestRun) {
            bestRun = (size_t)count;
            runStart = at;
        }
    }
    for (size_t p = runStart, n; bestRun >= 2 && (n = char_record(d, def->size, p)) != 0 && def->charCount < 16; p += n + 9) {
        double v;
        memcpy(&v, d + p + n + 1, 8);
        memcpy(def->charName[def->charCount], d + p, n);
        def->charName[def->charCount][n] = 0;
        def->charScale[def->charCount++] = (float)v;
    }
    int cap = 64;
    def->presets = an_alloc(sizeof(EmotePreset) * (size_t)cap);
    for (size_t at = 0; at + 16 < def->size; at++) {
        if (rd32(d + at) != 9 || rd32(d + at + 4) > 8 || !isalpha(d[at + 8])) continue;
        size_t p = at + 8;
        const u8 *z = memchr(d + p, 0, def->size - p);
        if (!z || z - (d + p) > 64 || !islower(z[1])) continue;
        EmotePreset pr = {dup_string(d + p, (size_t)(z - (d + p))), NULL, 0};
        p = (size_t)(z - d) + 1;
        int vcap = 0;
        while (p < def->size && d[p]) {
            const u8 *nz = memchr(d + p, 0, def->size - p);
            if (!nz) break;
            const u8 *vz = memchr(nz + 1, 0, def->size - (size_t)(nz + 1 - d));
            if (!vz) break;
            if (pr.count == vcap) {
                vcap = vcap ? vcap * 2 : 16;
                pr.vars = an_realloc(pr.vars, sizeof(EmoteVar) * (size_t)vcap);
            }
            pr.vars[pr.count++] = (EmoteVar){dup_string(d + p, (size_t)(nz - (d + p))), (float)atof((const char *)nz + 1)};
            p = (size_t)(vz - d) + 1;
        }
        if (def->presetCount == cap) {
            cap *= 2;
            def->presets = an_realloc(def->presets, sizeof(EmotePreset) * (size_t)cap);
        }
        def->presets[def->presetCount++] = pr;
        at = p;
    }
    for (size_t at = 0; at + 8 < def->size; at++) {
        size_t next = idle_record(def, d, def->size, at);
        if (next) at = next - 1;
    }
    return true;
}

const IdleMotion *an_define_idle(const Define *def, const char *name) {
    for (int i = 0; i < def->idleCount; i++)
        if (!strcmp(def->idles[i].name, name)) return &def->idles[i];
    return NULL;
}

float an_define_char_scale(const Define *def, const char *model) {
    for (int i = 0; i < def->charCount; i++) {
        size_t n = strlen(def->charName[i]);
        if (!strncmp(model, def->charName[i], n) && !isalpha((u8)model[n])) return def->charScale[i] / 100.0f;
    }
    return 1.0f;
}

const EmotePreset *an_define_preset(const Define *def, const char *name) {
    for (int i = 0; i < def->presetCount; i++)
        if (!strcmp(def->presets[i].name, name)) return &def->presets[i];
    return NULL;
}

void an_define_free(Define *def) {
    for (int i = 0; i < def->spriteCount; i++) {
        free(def->sprites[i].name);
        free(def->sprites[i].file);
    }
    free(def->sprites);
    for (int i = 0; i < def->presetCount; i++) {
        for (int k = 0; k < def->presets[i].count; k++) free(def->presets[i].vars[k].name);
        free(def->presets[i].vars);
        free(def->presets[i].name);
    }
    free(def->presets);
    for (int i = 0; i < def->idleCount; i++) {
        for (int k = 0; k < def->idles[i].count; k++) free(def->idles[i].vars[k].name);
        free(def->idles[i].vars);
        free(def->idles[i].name);
    }
    free(def->idles);
    free(def->data);
    memset(def, 0, sizeof *def);
}

const char *an_define_sprite(const Define *def, const char *name) {
    for (int i = 0; i < def->spriteCount; i++)
        if (an_ieq(def->sprites[i].name, name)) return def->sprites[i].file;
    return NULL;
}

static void pack_hash(Pack *p) {
    p->hashSize = 64;
    while (p->hashSize < p->count * 2) p->hashSize *= 2;
    p->hash = an_alloc(sizeof(int) * (size_t)p->hashSize);
    for (int i = 0; i < p->hashSize; i++) p->hash[i] = -1;
    for (int i = 0; i < p->count; i++) {
        u32 h = an_hash(p->e[i].name) & (u32)(p->hashSize - 1);
        while (p->hash[h] >= 0) h = (h + 1) & (u32)(p->hashSize - 1);
        p->hash[h] = i;
    }
}

bool an_pack_open(Pack *p, const char *path, bool wide) {
    memset(p, 0, sizeof *p);
    char real[2048];
    if (an_path_resolve(path, real, sizeof real) <= 0) return false;
    FILE *f = fopen(real, "rb");
    if (!f) return false;
    u8 head[8];
    if (fread(head, 1, 8, f) != 8) {
        fclose(f);
        return false;
    }
    u32 count = rd32(head);
    if (!count || count > 0xfffff) {
        fclose(f);
        return false;
    }
    size_t start = wide ? 8 : 4;
    size_t indexSize = 12 * (size_t)count;
    size_t names = 260 * (size_t)count;
    u8 *ix = an_alloc(indexSize + names);
    fseek(f, (long)start, SEEK_SET);
    size_t got = fread(ix, 1, indexSize + names, f);
    if (got < indexSize) {
        free(ix);
        fclose(f);
        return false;
    }
    p->e = an_alloc(sizeof(PackEntry) * count);
    size_t q = indexSize;
    for (u32 i = 0; i < count; i++) {
        const u8 *r = ix + 12 * (size_t)i;
        PackEntry *e = &p->e[i];
        e->offset = wide ? rd32(r) : rd32(r + 4);
        e->size = wide ? rd32(r + 4) : rd32(r + 8);
        size_t len = 0;
        while (q + len < got && ix[q + len]) len++;
        if (q + len >= got) {
            free(ix);
            an_pack_close(p);
            fclose(f);
            return false;
        }
        e->name = dup_string(ix + q, len);
        q += len + 1;
        p->count++;
    }
    free(ix);
    p->f = f;
    pack_hash(p);
    return true;
}

void an_pack_close(Pack *p) {
    for (int i = 0; i < p->count; i++) free(p->e[i].name);
    free(p->e);
    free(p->hash);
    if (p->f) fclose(p->f);
    memset(p, 0, sizeof *p);
}

const PackEntry *an_pack_find(const Pack *p, const char *name) {
    if (!p->hashSize) return NULL;
    u32 h = an_hash(name) & (u32)(p->hashSize - 1);
    for (int i; (i = p->hash[h]) >= 0; h = (h + 1) & (u32)(p->hashSize - 1))
        if (an_ieq(p->e[i].name, name)) return &p->e[i];
    return NULL;
}

u8 *an_pack_read(Pack *p, const PackEntry *e) {
    if (!p->f || !e) return NULL;
    u8 *d = an_alloc((size_t)e->size + 1);
    if (fseek(p->f, (long)e->offset, SEEK_SET) || fread(d, 1, e->size, p->f) != e->size) {
        free(d);
        return NULL;
    }
    return d;
}
