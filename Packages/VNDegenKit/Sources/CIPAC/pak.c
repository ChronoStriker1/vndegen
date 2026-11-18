#include "ipac.h"
#include "ip_casepath.h"

#include <ctype.h>
#include <dirent.h>

static const char *const kindDirs[PK_KINDS] = {"BG", "CHR", "PARTS", "SCR", "SE", "VOICE", "BGM"};

static u32 hash_str(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u8)*s) * 16777619u;
    return h;
}

static void group_add(PakGroup *g, char *key, u32 off, u32 size, int file) {
    if (g->count == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 256;
        g->entries = ip_realloc(g->entries, sizeof(PakEntry) * (size_t)g->cap);
    }
    g->entries[g->count++] = (PakEntry){key, off, size, file};
}

static void group_index(PakGroup *g) {
    free(g->hash);
    g->hashSize = 16;
    while (g->hashSize < g->count * 2) g->hashSize *= 2;
    g->hash = ip_alloc(sizeof(int) * (size_t)g->hashSize);
    for (int i = 0; i < g->hashSize; i++) g->hash[i] = -1;
    for (int i = 0; i < g->count; i++) {
        u32 h = hash_str(g->entries[i].key) & (u32)(g->hashSize - 1);
        while (g->hash[h] >= 0 && strcmp(g->entries[g->hash[h]].key, g->entries[i].key)) h = (h + 1) & (u32)(g->hashSize - 1);
        if (g->hash[h] < 0) g->hash[h] = i;
    }
}

static const PakEntry *group_find(const PakGroup *g, const char *key) {
    if (!g->hashSize) return NULL;
    u32 h = hash_str(key) & (u32)(g->hashSize - 1);
    while (g->hash[h] >= 0) {
        if (!strcmp(g->entries[g->hash[h]].key, key)) return &g->entries[g->hash[h]];
        h = (h + 1) & (u32)(g->hashSize - 1);
    }
    return NULL;
}

static int pak_kind(const char *name) {
    char up[64];
    snprintf(up, sizeof up, "%s", name);
    for (char *p = up; *p; p++) *p = (char)toupper((u8)*p);
    size_t n = strlen(up);
    if (n < 5 || strcmp(up + n - 4, ".PAK")) return -1;
    if (!strncmp(up, "BGM", 3)) return PK_BGM;
    if (!strncmp(up, "BG", 2)) return PK_BG;
    if (!strncmp(up, "CHR", 3)) return PK_CHR;
    if (!strncmp(up, "PARTS", 5)) return PK_PARTS;
    if (!strncmp(up, "SCR", 3)) return PK_SCR;
    if (!strncmp(up, "SE", 2)) return PK_SE;
    if (!strncmp(up, "VOICE", 5)) return PK_VOICE;
    return -1;
}

static bool pak_read(Store *s, const char *path, int kind) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    u8 head[8];
    if (fread(head, 1, 8, f) != 8 || memcmp(head, "IPAC", 4)) {
        fclose(f);
        return false;
    }
    int count = rd16(head + 4);
    size_t n = (size_t)count * 0x2c;
    u8 *idx = ip_alloc(n);
    if (fread(idx, 1, n, f) != n) {
        free(idx);
        fclose(f);
        return false;
    }
    s->files = ip_realloc(s->files, sizeof(FILE *) * (size_t)(s->fileCount + 1));
    int fi = s->fileCount++;
    s->files[fi] = f;
    for (int i = 0; i < count; i++) {
        const u8 *e = idx + (size_t)i * 0x2c;
        char name[0x21];
        memcpy(name, e, 0x20);
        name[0x20] = 0;
        ip_lower(name);
        group_add(&s->groups[kind], ip_strdup(name), rd32(e + 0x24), rd32(e + 0x28), fi);
    }
    free(idx);
    return true;
}

bool store_open(Store *s, const char *gameDir, char *err, size_t errSize) {
    memset(s, 0, sizeof *s);
    char want[2048], real[2048];
    snprintf(want, sizeof want, "%s/DATA", gameDir);
    if (ip_casepath_resolve(want, real, sizeof real) < 0) {
        if (err) snprintf(err, errSize, "This game's DATA folder is missing.");
        return false;
    }
    s->dataDir = ip_strdup(real);
    DIR *d = opendir(real);
    if (!d) {
        if (err) snprintf(err, errSize, "This game's DATA folder can't be read.");
        free(s->dataDir);
        return false;
    }
    char **names = NULL;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (pak_kind(de->d_name) < 0) continue;
        names = ip_realloc(names, sizeof(char *) * (size_t)(count + 1));
        names[count++] = ip_strdup(de->d_name);
    }
    closedir(d);
    for (int i = 0; i < count; i++)
        for (int j = i + 1; j < count; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char *x = names[i];
                names[i] = names[j];
                names[j] = x;
            }
    for (int i = 0; i < count; i++) {
        char path[2048];
        snprintf(path, sizeof path, "%s/%s", real, names[i]);
        pak_read(s, path, pak_kind(names[i]));
        free(names[i]);
    }
    free(names);
    for (int k = 0; k < PK_KINDS; k++) group_index(&s->groups[k]);
    if (!s->groups[PK_SCR].count && !store_has(s, PK_SCR, "seeninit", ".org")) {
        if (err) snprintf(err, errSize, "This game's scripts (Scr.PAK) are missing.");
        store_close(s);
        return false;
    }
    return true;
}

void store_close(Store *s) {
    for (int i = 0; i < s->fileCount; i++) fclose(s->files[i]);
    free(s->files);
    for (int k = 0; k < PK_KINDS; k++) {
        for (int i = 0; i < s->groups[k].count; i++) free(s->groups[k].entries[i].key);
        free(s->groups[k].entries);
        free(s->groups[k].hash);
    }
    free(s->dataDir);
    memset(s, 0, sizeof *s);
}

u8 *lzss_decode(const u8 *src, size_t n, size_t outLen) {
    u8 *out = ip_alloc(outLen + 1);
    u8 win[4096];
    memset(win, 0x20, sizeof win);
    int pos = 0xfee;
    size_t i = 0, o = 0;
    while (i < n && o < outLen) {
        u8 flags = src[i++];
        for (int b = 0; b < 8 && i < n && o < outLen; b++) {
            if (flags & (1 << b)) {
                u8 c = src[i++];
                out[o++] = c;
                win[pos] = c;
                pos = (pos + 1) & 0xfff;
            } else {
                if (i + 1 >= n) {
                    i = n;
                    break;
                }
                int off = src[i] | (src[i + 1] & 0xf0) << 4, len = (src[i + 1] & 0x0f) + 3;
                i += 2;
                for (int k = 0; k < len && o < outLen; k++) {
                    u8 c = win[(off + k) & 0xfff];
                    out[o++] = c;
                    win[pos] = c;
                    pos = (pos + 1) & 0xfff;
                }
            }
        }
    }
    return out;
}

static char *make_key(const char *name, const char *ext) {
    char *k = ip_fmt("%s%s", name, ext ? ext : "");
    ip_lower(k);
    return k;
}

static u8 *loose_load(Store *s, int kind, const char *file, size_t *size) {
    char *u = ip_sjis_to_utf8(file);
    char path[2048];
    snprintf(path, sizeof path, "%s/%s/%s", s->dataDir, kindDirs[kind], u);
    free(u);
    return ip_read_file(path, size);
}

u8 *store_load(Store *s, int kind, const char *name, const char *ext, size_t *size) {
    char *file = ip_fmt("%s%s", name, ext ? ext : "");
    size_t n = 0;
    u8 *d = loose_load(s, kind, file, &n);
    free(file);
    if (!d) {
        char *key = make_key(name, ext);
        const PakEntry *e = group_find(&s->groups[kind], key);
        free(key);
        if (!e) return NULL;
        FILE *f = s->files[e->file];
        u8 *raw = ip_alloc(e->size + 1);
        if (fseek(f, e->offset, SEEK_SET) || fread(raw, 1, e->size, f) != e->size) {
            free(raw);
            return NULL;
        }
        if (e->size >= 8 && !memcmp(raw, "IEL1", 4)) {
            n = rd32(raw + 4);
            d = lzss_decode(raw + 8, e->size - 8, n);
            free(raw);
        } else {
            d = raw;
            n = e->size;
        }
    }
    if (size) *size = n;
    return d;
}

bool store_has(Store *s, int kind, const char *name, const char *ext) {
    char *key = make_key(name, ext);
    bool found = group_find(&s->groups[kind], key) != NULL;
    free(key);
    if (found) return true;
    char *u = ip_sjis_to_utf8(name);
    char path[2048];
    snprintf(path, sizeof path, "%s/%s/%s%s", s->dataDir, kindDirs[kind], u, ext ? ext : "");
    free(u);
    return ip_file_exists(path);
}
