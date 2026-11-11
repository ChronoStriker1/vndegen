// Files the game writes: SAVE and LOAD data, and files made with FILEACT, all inside the save folder.
#include "yr.h"
#include "vm.h"
#include <sys/stat.h>

// The game names its files relative to its own folder, often as "savedata\\x.dat". Only names inside the folder are
// allowed: absolute paths, drives and ".." are refused.
char *yr_save_path(YR *y, const char *name) {
    if (!name || !*name || name[0] == '/' || name[0] == '\\' || strchr(name, ':')) return NULL;
    char key[1024];
    snprintf(key, sizeof key, "%s", name);
    for (char *p = key; *p; p++) if (*p == '\\') *p = '/';
    if (strstr(key, "..")) return NULL;
    const char *rel = key;
    if (!strncasecmp(rel, "savedata/", 9)) rel += 9;
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
