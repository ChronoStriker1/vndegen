#include "sa.h"

#include <ctype.h>
#include <dirent.h>

static u32 hash_name(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u32)tolower((u8)*s)) * 16777619u;
    return h;
}

static void rehash(Vfs *v) {
    free(v->hash);
    v->hashSize = 1024;
    while (v->hashSize < v->count * 2) v->hashSize *= 2;
    v->hash = sa_alloc(sizeof(int) * (size_t)v->hashSize);
    for (int i = 0; i < v->hashSize; i++) v->hash[i] = -1;
    for (int i = 0; i < v->count; i++) {
        u32 h = hash_name(v->e[i].name) & (u32)(v->hashSize - 1);
        while (v->hash[h] >= 0) h = (h + 1) & (u32)(v->hashSize - 1);
        v->hash[h] = i;
    }
}

const VfsEntry *sa_vfs_find(const Vfs *v, const char *name) {
    if (!v->hashSize || !name || !*name) return NULL;
    u32 h = hash_name(name) & (u32)(v->hashSize - 1);
    while (v->hash[h] >= 0) {
        const VfsEntry *e = &v->e[v->hash[h]];
        if (sa_ieq(e->name, name)) return e;
        h = (h + 1) & (u32)(v->hashSize - 1);
    }
    return NULL;
}

static bool add_archive(Vfs *v, const char *dir, const char *file) {
    if (v->arcs == SA_ARCS) return false;
    char *path = sa_fmt("%s/%s", dir, file);
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return false;
    u8 hdr[16];
    if (fread(hdr, 1, 16, f) != 16 || hdr[0] != 'V' || hdr[1] != 'F' || rd16(hdr + 2) < 0x200) {
        fclose(f);
        return false;
    }
    u32 count = rd16(hdr + 4), esz = rd16(hdr + 6);
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    if (!count || esz < 0x17 || rd32(hdr + 12) != (u32)fileSize) {
        fclose(f);
        return false;
    }
    size_t idxSize = (size_t)count * esz;
    u8 *idx = sa_alloc(idxSize + 8);
    fseek(f, 16, SEEK_SET);
    bool ok = fread(idx, 1, idxSize + 8, f) == idxSize + 8;
    u32 nameChars = ok ? rd32(idx + idxSize) : 0;
    u8 *names = sa_alloc((size_t)nameChars * 2 + 2);
    ok = ok && fread(names, 1, (size_t)nameChars * 2, f) == (size_t)nameChars * 2;
    if (!ok) {
        free(idx);
        free(names);
        fclose(f);
        return false;
    }
    int arc = v->arcs++;
    v->f[arc] = f;
    v->arcName[arc] = sa_strdup(file);
    for (u32 i = 0; i < count; i++) {
        const u8 *e = idx + (size_t)i * esz;
        u32 no = rd32(e);
        VfsEntry ent = {0};
        ent.arc = arc;
        ent.offset = rd32(e + 0xa);
        ent.size = rd32(e + 0xe);
        if (no >= nameChars || (long)ent.offset + (long)ent.size > fileSize) continue;
        ent.name = sa_utf16_to_utf8(names + (size_t)no * 2, (int)(nameChars - no));
        if (!ent.name[0] || sa_vfs_find(v, ent.name)) {
            free(ent.name);
            continue;
        }
        if (v->count == v->cap) {
            v->cap = v->cap ? v->cap * 2 : 4096;
            v->e = sa_realloc(v->e, sizeof(VfsEntry) * (size_t)v->cap);
        }
        v->e[v->count++] = ent;
        if (v->count * 2 > v->hashSize) rehash(v);
    }
    free(idx);
    free(names);
    return true;
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

bool sa_vfs_open(Vfs *v, const char *dir) {
    memset(v, 0, sizeof *v);
    char real[2048];
    if (sa_path_resolve(dir, real, sizeof real) < 1) return false;
    DIR *d = opendir(real);
    if (!d) return false;
    char **files = NULL;
    int n = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        size_t len = strlen(de->d_name);
        if (len < 5 || !sa_ieq(de->d_name + len - 4, ".vfs")) continue;
        files = sa_realloc(files, sizeof(char *) * (size_t)(n + 1));
        files[n++] = sa_strdup(de->d_name);
    }
    closedir(d);
    qsort(files, (size_t)n, sizeof(char *), cmp_str);
    for (int i = 0; i < n; i++) {
        add_archive(v, real, files[i]);
        free(files[i]);
    }
    free(files);
    return v->count > 0;
}

void sa_vfs_close(Vfs *v) {
    for (int i = 0; i < v->arcs; i++) {
        fclose(v->f[i]);
        free(v->arcName[i]);
    }
    for (int i = 0; i < v->count; i++) free(v->e[i].name);
    free(v->e);
    free(v->hash);
    memset(v, 0, sizeof *v);
}

u8 *sa_vfs_read(Vfs *v, const char *name, size_t *size) {
    const VfsEntry *e = sa_vfs_find(v, name);
    if (!e) return NULL;
    u8 *d = sa_alloc((size_t)e->size + 1);
    FILE *f = v->f[e->arc];
    if (fseek(f, (long)e->offset, SEEK_SET) || fread(d, 1, e->size, f) != e->size) {
        free(d);
        return NULL;
    }
    if (size) *size = e->size;
    return d;
}
