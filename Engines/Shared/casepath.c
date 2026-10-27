// glibc hides lstat, strdup, and dev_t under strict C. Apple's headers show them anyway.
#ifndef __APPLE__
#define _DEFAULT_SOURCE
#endif

#include "casepath.h"

#include <dirent.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __APPLE__
#define MTIME(st) ((st).st_mtimespec)
#else
#define MTIME(st) ((st).st_mtim)
#endif

// A folder's names as last read, and the folder's identity and time then. A listing whose folder has changed since is
// read again.
struct listing {
    char *dir;
    dev_t dev;
    ino_t ino;
    struct timespec mtime;
    char **names;
    size_t count;
};

// Games touch a few hundred folders at most. When the cache is full, the oldest listing makes room.
enum { max_listings = 256 };
static struct listing listings[max_listings];
static size_t next_slot;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void release(struct listing *l) {
    for (size_t i = 0; i < l->count; i++) free(l->names[i]);
    free(l->names);
    free(l->dir);
    memset(l, 0, sizeof *l);
}

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

// Reads `dir` into `l`. False when it can't be listed, as with a folder the sandbox hides.
static int read_listing(struct listing *l, const char *dir, const struct stat *st) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    size_t capacity = 64, count = 0;
    char **names = malloc(capacity * sizeof *names);
    struct dirent *e;
    while (names && (e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (count == capacity) {
            char **grown = realloc(names, (capacity *= 2) * sizeof *names);
            if (!grown) break;
            names = grown;
        }
        if (!(names[count] = strdup(e->d_name))) break;
        count++;
    }
    closedir(d);
    if (!names) return 0;
    qsort(names, count, sizeof *names, by_name);
    *l = (struct listing){strdup(dir), st->st_dev, st->st_ino, MTIME(*st), names, count};
    return 1;
}

static int same_time(struct timespec a, struct timespec b) { return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec; }

static int fold(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int equal_folded(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (fold((unsigned char)a[i]) != fold((unsigned char)b[i])) return 0;
    return 1;
}

// Looks up the `n` bytes at `name` in the listing of `dir` and overwrites them with the name on disk. Folding ASCII
// case never changes a name's length, so the match fits in place.
static int match(const char *dir, const struct stat *st, char *name, size_t n) {
    pthread_mutex_lock(&lock);
    struct listing *l = NULL;
    for (size_t i = 0; i < max_listings; i++) {
        if (listings[i].dir && !strcmp(listings[i].dir, dir)) {
            l = &listings[i];
            break;
        }
    }
    if (l && (l->dev != st->st_dev || l->ino != st->st_ino || !same_time(l->mtime, MTIME(*st)))) release(l);
    if (!l || !l->dir) {
        if (!l) {
            l = &listings[next_slot];
            next_slot = (next_slot + 1) % max_listings;
            release(l);
        }
        if (!read_listing(l, dir, st)) {
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }
    const char *found = NULL;
    for (size_t i = 0; i < l->count && !found; i++)
        if (strlen(l->names[i]) == n && !memcmp(l->names[i], name, n)) found = l->names[i];
    for (size_t i = 0; i < l->count && !found; i++)
        if (strlen(l->names[i]) == n && equal_folded(l->names[i], name, n)) found = l->names[i];
    if (found) memcpy(name, found, n);
    pthread_mutex_unlock(&lock);
    return found != NULL;
}

// stat of the first `end` bytes of `path`: the root for a leading slash with nothing after it, the current folder for
// none at all.
static int stat_prefix(char *path, size_t end, struct stat *st) {
    if (end == 0) return stat(path[0] == '/' ? "/" : ".", st);
    char saved = path[end];
    path[end] = '\0';
    int r = stat(path, st);
    path[end] = saved;
    return r;
}

int casepath_resolve(const char *path, char *out, size_t size) {
    size_t length = strlen(path);
    if (length >= size) return -1;
    memcpy(out, path, length + 1);
    struct stat st;
    if (lstat(out, &st) == 0) return 1;

    // The deepest folder that exists as written. Nothing above it is listed, so a sandbox that hides a game folder's
    // parents doesn't matter.
    size_t base = length;
    for (;;) {
        while (base > 0 && out[base - 1] != '/') base--;
        if (base == 0) break;
        while (base > 1 && out[base - 2] == '/') base--;
        if (stat_prefix(out, base - 1, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (base == 1) base = 0;
            break;
        }
        base--;
    }
    if (base == 0 && stat_prefix(out, 0, &st) != 0) return 0;

    size_t start = base;
    for (;;) {
        while (out[start] == '/') start++;
        if (!out[start]) return 1;
        size_t end = start;
        while (out[end] && out[end] != '/') end++;
        size_t n = end - start;
        int dots = (n == 1 && out[start] == '.') || (n == 2 && out[start] == '.' && out[start + 1] == '.');
        if (!dots) {
            char dir[start + 2];
            if (start == 0) strcpy(dir, ".");
            else if (start == 1) strcpy(dir, "/");
            else {
                memcpy(dir, out, start - 1);
                dir[start - 1] = '\0';
            }
            if (!match(dir, &st, out + start, n)) return 0;
        }
        if (!out[end]) return 1;
        if (stat_prefix(out, end, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
        start = end;
    }
}

void casepath_changed(const char *path) {
    const char *slash = strrchr(path, '/');
    size_t n = !slash ? 1 : slash == path ? 1 : (size_t)(slash - path);
    char dir[n + 1];
    if (!slash) strcpy(dir, ".");
    else if (slash == path) strcpy(dir, "/");
    else {
        memcpy(dir, path, n);
        dir[n] = '\0';
    }
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < max_listings; i++)
        if (listings[i].dir && !strcmp(listings[i].dir, dir)) release(&listings[i]);
    pthread_mutex_unlock(&lock);
}
