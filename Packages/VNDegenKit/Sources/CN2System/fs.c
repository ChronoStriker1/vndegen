// Game files by script path: loose files in the game folder first, then the archive named by the path's first
// folder ("cg/bg/x.jpg" is "bg/x.jpg" in cg.npa). Windows ignores case, so both lookups do too.
#include "n2.h"
#include "n2_casepath.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static char *norm(const char *path) {
    while (*path == '.' && (path[1] == '/' || path[1] == '\\')) path += 2;
    while (*path == '/' || *path == '\\') path++;
    char *p = n2_strdup(path);
    for (char *q = p; *q; q++)
        if (*q == '\\') *q = '/';
    return p;
}

bool fs_init(N2Fs *fs, const char *gameDir, char *err, size_t errSize) {
    memset(fs, 0, sizeof *fs);
    fs->gameDir = n2_strdup(gameDir);
    DIR *d = opendir(gameDir);
    if (!d) {
        snprintf(err, errSize, "can't read the game folder");
        return false;
    }
    struct dirent *de;
    while ((de = readdir(d)) && fs->archiveCount < 16) {
        size_t n = strlen(de->d_name);
        if (n < 5 || !n2_streq_ci(de->d_name + n - 4, ".npa")) continue;
        char *path = n2_fmt("%s/%s", gameDir, de->d_name);
        Npa *a = npa_open(path, err, errSize);
        free(path);
        if (!a) {
            closedir(d);
            fs_free(fs);
            return false;
        }
        fs->archives[fs->archiveCount] = a;
        fs->archiveNames[fs->archiveCount] = n2_strndup(de->d_name, n - 4);
        fs->archiveCount++;
    }
    closedir(d);
    return true;
}

void fs_free(N2Fs *fs) {
    for (int i = 0; i < fs->archiveCount; i++) {
        npa_close(fs->archives[i]);
        free(fs->archiveNames[i]);
    }
    free(fs->gameDir);
    memset(fs, 0, sizeof *fs);
}

static u8 *read_loose(N2Fs *fs, const char *rel, size_t *size) {
    char *path = n2_fmt("%s/%s", fs->gameDir, rel);
    char real[4096];
    u8 *data = NULL;
    if (casepath_resolve(path, real, sizeof real) == 1) {
        FILE *f = fopen(real, "rb");
        struct stat st;
        if (f && fstat(fileno(f), &st) == 0 && S_ISREG(st.st_mode)) {
            data = n2_alloc((size_t)st.st_size + 1);
            if (fread(data, 1, (size_t)st.st_size, f) != (size_t)st.st_size) {
                free(data);
                data = NULL;
            } else {
                *size = (size_t)st.st_size;
            }
        }
        if (f) fclose(f);
    }
    free(path);
    return data;
}

static const NpaEntry *find_archived(N2Fs *fs, const char *rel, Npa **arc) {
    const char *slash = strchr(rel, '/');
    if (slash) {
        for (int i = 0; i < fs->archiveCount; i++) {
            size_t n = strlen(fs->archiveNames[i]);
            if (n == (size_t)(slash - rel) && strncasecmp(rel, fs->archiveNames[i], n) == 0) {
                const NpaEntry *e = npa_find(fs->archives[i], slash + 1);
                if (e) {
                    *arc = fs->archives[i];
                    return e;
                }
            }
        }
    }
    // Some paths name an entry with its folder ("nss/boot.nss" is "boot.nss" in nss.npa, "system.ini" in system.npa).
    for (int i = 0; i < fs->archiveCount; i++) {
        const NpaEntry *e = npa_find(fs->archives[i], rel);
        if (!e && slash) e = npa_find(fs->archives[i], slash + 1);
        if (e) {
            *arc = fs->archives[i];
            return e;
        }
    }
    return NULL;
}

u8 *fs_read(N2Fs *fs, const char *path, size_t *size) {
    char *rel = norm(path);
    u8 *data = read_loose(fs, rel, size);
    if (!data) {
        Npa *arc = NULL;
        const NpaEntry *e = find_archived(fs, rel, &arc);
        if (e) data = npa_read(arc, e, size);
    }
    free(rel);
    return data;
}

bool fs_exists(N2Fs *fs, const char *path) {
    char *rel = norm(path);
    char *full = n2_fmt("%s/%s", fs->gameDir, rel);
    char real[4096];
    bool found = casepath_resolve(full, real, sizeof real) == 1;
    Npa *arc;
    if (!found) found = find_archived(fs, rel, &arc) != NULL;
    free(full);
    free(rel);
    return found;
}
