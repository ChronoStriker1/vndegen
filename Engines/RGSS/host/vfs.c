// The libretro VFS interface (version 3) on POSIX files. The mkxp-z core does all its file access through it, so this
// is where a game's paths are matched to the disk ignoring case, as on Windows.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "casepath.h"
#include "libretro.h"

struct retro_vfs_file_handle {
    int fd;
    char *path;
};

struct retro_vfs_dir_handle {
    DIR *dir;
    struct dirent *entry;
    char *path;
    bool hidden;
};

// The path as it is on disk, whatever case the game names it in, written into `out`. A path that doesn't exist keeps
// the game's spelling below its deepest existing folder. A path too long for `out` is used as written.
static const char *on_disk(const char *path, char out[PATH_MAX]) {
    return casepath_resolve(path, out, PATH_MAX) < 0 ? path : out;
}

static const char *vfs_get_path(struct retro_vfs_file_handle *h) { return h->path; }

// A write to a file that exists in another case replaces that file rather than creating a second one beside it.
static struct retro_vfs_file_handle *vfs_open(const char *game_path, unsigned mode, unsigned hints) {
    (void)hints;
    char buffer[PATH_MAX];
    const char *path = on_disk(game_path, buffer);
    int flags;
    if ((mode & RETRO_VFS_FILE_ACCESS_READ_WRITE) == RETRO_VFS_FILE_ACCESS_READ_WRITE) flags = O_RDWR | O_CREAT;
    else if (mode & RETRO_VFS_FILE_ACCESS_WRITE) flags = O_WRONLY | O_CREAT;
    else flags = O_RDONLY;
    if ((mode & RETRO_VFS_FILE_ACCESS_WRITE) && !(mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING)) flags |= O_TRUNC;
    int fd = open(path, flags | O_CLOEXEC, 0644);
    if (fd < 0) return NULL;
    if (flags & O_CREAT) casepath_changed(path);
    struct retro_vfs_file_handle *h = malloc(sizeof *h);
    h->fd = fd;
    h->path = strdup(path);
    return h;
}

static int vfs_close(struct retro_vfs_file_handle *h) {
    int r = close(h->fd);
    free(h->path);
    free(h);
    return r == 0 ? 0 : -1;
}

static int64_t vfs_size(struct retro_vfs_file_handle *h) {
    struct stat st;
    return fstat(h->fd, &st) == 0 ? st.st_size : -1;
}

static int64_t vfs_truncate(struct retro_vfs_file_handle *h, int64_t length) { return ftruncate(h->fd, length) == 0 ? 0 : -1; }
static int64_t vfs_tell(struct retro_vfs_file_handle *h) { return lseek(h->fd, 0, SEEK_CUR); }

static int64_t vfs_seek(struct retro_vfs_file_handle *h, int64_t offset, int position) {
    int whence = position == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR : position == RETRO_VFS_SEEK_POSITION_END ? SEEK_END : SEEK_SET;
    return lseek(h->fd, offset, whence);
}

static int64_t vfs_read(struct retro_vfs_file_handle *h, void *buf, uint64_t len) {
    uint64_t done = 0;
    while (done < len) {
        ssize_t n = read(h->fd, (char *)buf + done, len - done);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return -1;
        if (n == 0) break;
        done += (uint64_t)n;
    }
    return (int64_t)done;
}

static int64_t vfs_write(struct retro_vfs_file_handle *h, const void *buf, uint64_t len) {
    uint64_t done = 0;
    while (done < len) {
        ssize_t n = write(h->fd, (const char *)buf + done, len - done);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return -1;
        done += (uint64_t)n;
    }
    return (int64_t)done;
}

static int vfs_flush(struct retro_vfs_file_handle *h) { return fsync(h->fd) == 0 ? 0 : -1; }
static int vfs_remove(const char *game_path) {
    char buffer[PATH_MAX];
    const char *path = on_disk(game_path, buffer);
    if (unlink(path) != 0 && rmdir(path) != 0) return -1;
    casepath_changed(path);
    return 0;
}

// Two paths that differ only in case name the same file, which rename leaves as it is.
static int vfs_rename(const char *game_from, const char *game_to) {
    char from_buffer[PATH_MAX], to_buffer[PATH_MAX];
    const char *from = on_disk(game_from, from_buffer), *to = on_disk(game_to, to_buffer);
    if (rename(from, to) != 0) return -1;
    casepath_changed(from);
    casepath_changed(to);
    return 0;
}

static int vfs_stat(const char *game_path, int32_t *size) {
    char buffer[PATH_MAX];
    struct stat st;
    if (stat(on_disk(game_path, buffer), &st) != 0) return 0;
    if (size) *size = (int32_t)st.st_size;
    return RETRO_VFS_STAT_IS_VALID | (S_ISDIR(st.st_mode) ? RETRO_VFS_STAT_IS_DIRECTORY : 0) |
           (S_ISCHR(st.st_mode) ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);
}

// A folder that exists in another case exists, so this returns -2 for it.
static int vfs_mkdir(const char *game_dir) {
    char buffer[PATH_MAX];
    const char *dir = on_disk(game_dir, buffer);
    if (mkdir(dir, 0755) != 0) return errno == EEXIST ? -2 : -1;
    casepath_changed(dir);
    return 0;
}

static struct retro_vfs_dir_handle *vfs_opendir(const char *game_path, bool hidden) {
    char buffer[PATH_MAX];
    const char *path = on_disk(game_path, buffer);
    DIR *d = opendir(path);
    if (!d) return NULL;
    struct retro_vfs_dir_handle *h = calloc(1, sizeof *h);
    h->dir = d;
    h->path = strdup(path);
    h->hidden = hidden;
    return h;
}

static bool vfs_readdir(struct retro_vfs_dir_handle *h) {
    while ((h->entry = readdir(h->dir))) {
        const char *n = h->entry->d_name;
        if (!strcmp(n, ".") || !strcmp(n, "..")) continue;
        if (!h->hidden && n[0] == '.') continue;
        return true;
    }
    return false;
}

static const char *vfs_dirent_get_name(struct retro_vfs_dir_handle *h) { return h->entry ? h->entry->d_name : NULL; }

static bool vfs_dirent_is_dir(struct retro_vfs_dir_handle *h) {
    if (!h->entry) return false;
    if (h->entry->d_type != DT_UNKNOWN && h->entry->d_type != DT_LNK) return h->entry->d_type == DT_DIR;
    size_t len = strlen(h->path) + strlen(h->entry->d_name) + 2;
    char *full = malloc(len);
    snprintf(full, len, "%s/%s", h->path, h->entry->d_name);
    struct stat st;
    bool dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
    free(full);
    return dir;
}

static int vfs_closedir(struct retro_vfs_dir_handle *h) {
    closedir(h->dir);
    free(h->path);
    free(h);
    return 0;
}

struct retro_vfs_interface vndegen_posix_vfs = {
    vfs_get_path, vfs_open, vfs_close, vfs_size, vfs_tell, vfs_seek, vfs_read, vfs_write, vfs_flush, vfs_remove, vfs_rename,
    vfs_truncate, vfs_stat, vfs_mkdir, vfs_opendir, vfs_readdir, vfs_dirent_get_name, vfs_dirent_is_dir, vfs_closedir,
};
