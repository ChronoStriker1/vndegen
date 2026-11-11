// Game files: .ypf archives and loose files.
//
// An archive is "YPF\0", a version, an entry count and the directory size, then the directory from offset 32. Each
// entry is a 4-byte name hash, a scrambled name length, the name XORed with a per-archive byte, the type, a packed
// flag, the sizes, the data offset (4 or 8 bytes) and a data checksum. The length byte goes through a swap table that
// differs between games, so the reader takes the length whose decoded name matches the stored MurmurHash2 (or CRC-32)
// instead of using a table, and takes the offset width whose entries end at the directory size.
#include "yr.h"
#include "vm.h"
#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>

typedef struct YEntry {
    char *key;        // lowercased, '/' separated
    u16 arc;
    u8 packed;
    u32 size, csize;
    u64 offset;
} YEntry;

typedef struct YArchive { char *path; FILE *f; } YArchive;

struct YFS {
    YArchive *arcs;
    int arcCount;
    YEntry *table;    // open addressing, capacity a power of two
    u32 cap, used;
};

static u32 key_hash(const char *k) {
    u32 h = 2166136261u;
    for (; *k; k++) h = (h ^ (u8)*k) * 16777619u;
    return h;
}

static YEntry *slot(YFS *fs, const char *key) {
    u32 i = key_hash(key) & (fs->cap - 1);
    while (fs->table[i].key && strcmp(fs->table[i].key, key)) i = (i + 1) & (fs->cap - 1);
    return &fs->table[i];
}

static void put(YFS *fs, YEntry e) {
    if ((fs->used + 1) * 2 > fs->cap) {
        YEntry *old = fs->table;
        u32 oldCap = fs->cap;
        fs->cap = oldCap ? oldCap * 2 : 4096;
        fs->table = yr_alloc(sizeof(YEntry) * fs->cap);
        fs->used = 0;
        for (u32 i = 0; i < oldCap; i++) if (old[i].key) { *slot(fs, old[i].key) = old[i]; fs->used++; }
        free(old);
    }
    YEntry *s = slot(fs, e.key);
    if (s->key) { free(s->key); fs->used--; }   // a later archive replaces the entry
    *s = e;
    fs->used++;
}

// Decodes the stored length byte with the swap table most games share, which only seeds the search below.
static int swapped_length(u8 v) {
    static const u8 t[] = { 0x03, 0x48, 0x06, 0x35, 0x0C, 0x10, 0x11, 0x19, 0x1C, 0x1E, 0x09, 0x0B,
                            0x0D, 0x13, 0x15, 0x1B, 0x20, 0x23, 0x26, 0x29, 0x2C, 0x2F, 0x2E, 0x32 };
    for (size_t i = 0; i < sizeof t; i++) if (t[i] == v) return t[i ^ 1];
    return v;
}

static bool name_matches(const u8 *raw, int len, u8 key, u32 hash) {
    u8 buf[256];
    for (int i = 0; i < len; i++) buf[i] = raw[i] ^ key;
    return yr_murmur2(buf, (size_t)len) == hash || yr_crc32(buf, (size_t)len) == hash;
}

// The name's length: the swap table's answer, the raw byte, then every length, whichever decodes to the stored hash.
static int name_length(const u8 *e, const u8 *end, u8 key) {
    u32 hash = yr_le32(e);
    u8 v = e[4] ^ 0xff;
    int tries[2] = { swapped_length(v), v };
    for (int t = 0; t < 2; t++)
        if (e + 5 + tries[t] <= end && name_matches(e + 5, tries[t], key, hash)) return tries[t];
    for (int n = 1; n < 256 && e + 5 + n <= end; n++) if (name_matches(e + 5, n, key, hash)) return n;
    return -1;
}

// The name XOR byte, found from the first entry: names end in a dot and a three-letter extension.
static int find_name_key(const u8 *e, const u8 *end) {
    u32 hash = yr_le32(e);
    for (int n = 4; n < 256 && e + 5 + n <= end; n++) {
        u8 key = e[5 + n - 4] ^ '.';
        if (name_matches(e + 5, n, key, hash)) return key;
    }
    return -1;
}

static bool scan(YFS *fs, u16 arc, const u8 *dir, size_t dirSize, u32 count, int tail, bool commit) {
    const u8 *p = dir, *end = dir + dirSize;
    int key = count ? find_name_key(p, end) : 0;
    if (key < 0) return false;
    for (u32 i = 0; i < count; i++) {
        if (p + 5 > end) return false;
        int n = name_length(p, end, (u8)key);
        if (n < 0) return false;
        const u8 *q = p + 5 + n;
        if (q + 10 + tail > end) return false;
        if (commit) {
            char name[256], k[256];
            for (int j = 0; j < n; j++) name[j] = (char)(p[5 + j] ^ key);
            name[n] = 0;
            yr_key(k, sizeof k, name);
            YEntry e = { yr_strdup(k), arc, q[1], yr_le32(q + 2), yr_le32(q + 6), 0 };
            e.offset = tail == 12 ? yr_le64(q + 10) : yr_le32(q + 10);
            put(fs, e);
        }
        p = q + 10 + tail;
    }
    return p == end;
}

static bool add_archive(YR *y, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    u8 head[32];
    if (fread(head, 1, 32, f) != 32 || memcmp(head, "YPF", 4)) { fclose(f); return false; }
    u32 count = yr_le32(head + 8), dirSize = yr_le32(head + 12);
    if (count == 0) { fclose(f); return true; }
    if (dirSize < 32 || dirSize > (64u << 20)) { fclose(f); return false; }
    // The directory size counts either from the start of the file or from the directory; both are tried.
    u8 *dir = yr_alloc(dirSize);
    size_t got = fread(dir, 1, dirSize, f);
    YFS *fs = y->fs;
    u16 arc = (u16)fs->arcCount;
    bool ok = false;
    // The tail after the sizes: a 64-bit offset and a checksum, a 32-bit offset and a checksum, or a bare offset.
    size_t sizes[2] = { dirSize - 32, got };
    static const int tails[] = { 12, 8, 4 };
    for (int t = 0; t < 3 && !ok; t++)
        for (int s = 0; s < 2 && !ok; s++)
            if (sizes[s] <= got && scan(fs, arc, dir, sizes[s], count, tails[t], false)) {
                scan(fs, arc, dir, sizes[s], count, tails[t], true);
                ok = true;
            }
    free(dir);
    if (!ok) {
        yr_log(y, "archive %s: directory not understood\n", path);
        fclose(f);
        return false;
    }
    fs->arcs = yr_realloc(fs->arcs, sizeof(YArchive) * (size_t)(fs->arcCount + 1));
    fs->arcs[fs->arcCount++] = (YArchive){ yr_strdup(path), f };
    yr_log(y, "archive %s: %u entries\n", path, count);
    return true;
}

static bool has_ext(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);
    return n > e && !strcasecmp(name + n - e, ext);
}

// Update archives (update1.ypf and on) replace files of the others, so they load last, in name order.
static int archive_order(const void *a, const void *b) {
    const char *x = strrchr(*(char *const *)a, '/'), *z = strrchr(*(char *const *)b, '/');
    x = x ? x + 1 : *(char *const *)a;
    z = z ? z + 1 : *(char *const *)b;
    bool ux = !strncasecmp(x, "update", 6), uz = !strncasecmp(z, "update", 6);
    if (ux != uz) return ux ? 1 : -1;
    return strcasecmp(x, z);
}

static void collect(const char *dir, char ***list, int *n) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!has_ext(e->d_name, ".ypf")) continue;
        *list = yr_realloc(*list, sizeof(char *) * (size_t)(*n + 1));
        (*list)[(*n)++] = yr_path(dir, e->d_name);
    }
    closedir(d);
}

bool yr_fs_open(YR *y) {
    y->fs = yr_alloc(sizeof(YFS));
    char **list = NULL;
    int n = 0;
    collect(y->gameDir, &list, &n);
    char *pac = yr_find_file(y->gameDir, "pac");
    if (pac) { collect(pac, &list, &n); free(pac); }
    qsort(list, (size_t)n, sizeof(char *), archive_order);
    for (int i = 0; i < n; i++) { add_archive(y, list[i]); free(list[i]); }
    free(list);
    return true;
}

void yr_fs_close(YR *y) {
    YFS *fs = y->fs;
    if (!fs) return;
    for (int i = 0; i < fs->arcCount; i++) { fclose(fs->arcs[i].f); free(fs->arcs[i].path); }
    free(fs->arcs);
    for (u32 i = 0; i < fs->cap; i++) free(fs->table[i].key);
    free(fs->table);
    free(fs);
    y->fs = NULL;
}

void yr_file_free(YFile *f) {
    free(f->data);
    f->data = NULL;
    f->size = 0;
}

static bool read_entry(YR *y, const YEntry *e, YFile *out) {
    YArchive *a = &y->fs->arcs[e->arc];
    u8 *raw = yr_alloc(e->csize + 1);
    if (fseeko(a->f, (off_t)e->offset, SEEK_SET) != 0 || fread(raw, 1, e->csize, a->f) != e->csize) {
        free(raw);
        return false;
    }
    if (!e->packed) {
        out->data = raw;
        out->size = e->csize;
        return true;
    }
    u8 *data = yr_alloc((size_t)e->size + 1);
    uLongf size = e->size;
    int r = uncompress(data, &size, raw, e->csize);
    free(raw);
    if (r != Z_OK) {
        free(data);
        return false;
    }
    out->data = data;
    out->size = size;
    return true;
}

static const YEntry *find_entry(YR *y, const char *key) {
    YFS *fs = y->fs;
    if (!fs->cap) return NULL;
    YEntry *s = slot(fs, key);
    return s->key ? s : NULL;
}

// Looks one exact name up: the archives first, then a loose file in the game folder or its data folder.
static bool read_one(YR *y, const char *name, YFile *out, bool probe) {
    char key[1024];
    yr_key(key, sizeof key, name);
    const char *k = key;
    while (k[0] == '.' && k[1] == '/') k += 2;
    if (!strncmp(k, "data/", 5) && !find_entry(y, k)) k += 5;
    const YEntry *e = find_entry(y, k);
    if (e) return probe ? true : read_entry(y, e, out);
    const char *roots[2] = { y->gameDir, NULL };
    char *data = yr_find_file(y->gameDir, "data");
    roots[1] = data;
    bool ok = false;
    for (int i = 0; i < 2 && !ok; i++) {
        if (!roots[i]) continue;
        char *path = yr_find_file(roots[i], k);
        if (!path) continue;
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            ok = true;
            if (!probe) {
                out->data = yr_read_file(path, &out->size);
                ok = out->data != NULL;
            }
        }
        free(path);
    }
    free(data);
    return ok;
}

static bool lookup(YR *y, const char *name, const char *const *exts, YFile *out, bool probe) {
    if (!name || !*name) return false;
    if (read_one(y, name, out, probe)) return true;
    if (!exts) return false;
    char buf[1024];
    for (int i = 0; exts[i]; i++) {
        snprintf(buf, sizeof buf, "%s%s", name, exts[i]);
        if (read_one(y, buf, out, probe)) return true;
    }
    return false;
}

bool yr_fs_read(YR *y, const char *name, const char *const *exts, YFile *out) {
    out->data = NULL;
    out->size = 0;
    return lookup(y, name, exts, out, false);
}

bool yr_fs_exists(YR *y, const char *name, const char *const *exts) {
    YFile none = { 0 };
    return lookup(y, name, exts, &none, true);
}
