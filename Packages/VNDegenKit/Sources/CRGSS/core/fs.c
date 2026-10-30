// The game's files. Windows ignores case, so a game asking for "Graphics/System/Window.PNG" gets
// "graphics/system/window.png" on a case-sensitive volume. Every lookup goes through rgss_resolve_case, which uses
// VNDegen's shared casepath_resolve.
#include "archive.h"
#include "rgss_casepath.h"
#include "rgss.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *const rgss_image_extensions[] = {".png", ".jpg", ".jpeg", ".bmp", NULL};
const char *const rgss_audio_extensions[] = {".ogg", ".mp3", ".wav", ".mid", ".midi", ".wma", NULL};

static struct {
    char saveDir[1024];
    char gameDir[1024];
    RGSSArchive *archive;
    char rtps[4][1024];
    int rtpCount;
} fs;

static bool is_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

bool rgss_resolve_case(const char *base, const char *relative, char *out, size_t length) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", base, relative);
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    int found = casepath_resolve(path, out, length);
    if (found < 0) snprintf(out, length, "%s", path);
    return found == 1;
}

void rgss_fs_open(const char *saveDir, const char *gameDir, const char *archive, const char *const *rtps, int rtpCount) {
    rgss_fs_close();
    snprintf(fs.saveDir, sizeof fs.saveDir, "%s", saveDir ? saveDir : gameDir);
    snprintf(fs.gameDir, sizeof fs.gameDir, "%s", gameDir);
    fs.archive = archive ? rgss_archive_open(archive) : NULL;
    if (archive && !fs.archive) rgss_log("could not open the archive %s", archive);
    fs.rtpCount = 0;
    for (int i = 0; i < rtpCount && i < 4; i++) snprintf(fs.rtps[fs.rtpCount++], sizeof fs.rtps[0], "%s", rtps[i]);
}

void rgss_fs_close(void) {
    rgss_archive_close(fs.archive);
    fs.archive = NULL;
}

static bool read_file(const char *path, uint8_t **data, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *buffer = malloc(length > 0 ? (size_t)length : 1);
    if (!buffer || fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);
    *data = buffer;
    *size = (size_t)length;
    return true;
}

// Looks up one candidate name in every source in order. `disk` gets the path of a file found on disk.
static bool find_one(const char *name, char *disk, size_t diskLength, long *entry) {
    *entry = -1;
    disk[0] = 0;
    const char *folders[2] = {fs.saveDir, fs.gameDir};
    for (int i = 0; i < 2; i++) {
        if (i == 1 && !strcmp(fs.saveDir, fs.gameDir)) break;
        if (rgss_resolve_case(folders[i], name, disk, diskLength) && is_file(disk)) return true;
    }
    if (fs.archive && (*entry = rgss_archive_find(fs.archive, name)) >= 0) {
        disk[0] = 0;
        return true;
    }
    for (int i = 0; i < fs.rtpCount; i++) {
        if (rgss_resolve_case(fs.rtps[i], name, disk, diskLength) && is_file(disk)) return true;
    }
    disk[0] = 0;
    return false;
}

// Tries the path as given, then with each extension. RGSS treats "Title" and "Title.png" alike.
static bool find(const char *path, const char *const *extensions, char *disk, size_t diskLength, long *entry,
                 const char **foundExtension) {
    if (find_one(path, disk, diskLength, entry)) {
        if (foundExtension) {
            const char *dot = strrchr(path, '.');
            *foundExtension = dot && !strchr(dot, '/') && !strchr(dot, '\\') ? dot : "";
        }
        return true;
    }
    for (int i = 0; extensions && extensions[i]; i++) {
        char name[1024];
        snprintf(name, sizeof name, "%s%s", path, extensions[i]);
        if (find_one(name, disk, diskLength, entry)) {
            if (foundExtension) *foundExtension = extensions[i];
            return true;
        }
    }
    return false;
}

bool rgss_fs_read(const char *path, const char *const *extensions, uint8_t **data, size_t *size,
                  const char **foundExtension) {
    char disk[2048];
    long entry;
    if (!find(path, extensions, disk, sizeof disk, &entry, foundExtension)) return false;
    if (disk[0]) return read_file(disk, data, size);
    return rgss_archive_read(fs.archive, entry, data, size);
}

bool rgss_fs_exists(const char *path, const char *const *extensions) {
    char disk[2048];
    long entry;
    return find(path, extensions, disk, sizeof disk, &entry, NULL);
}

bool rgss_fs_disk_path(const char *path, char *out, size_t length) {
    const char *folders[2] = {fs.saveDir, fs.gameDir};
    for (int i = 0; i < 2; i++) {
        if (rgss_resolve_case(folders[i], path, out, length)) return true;
    }
    rgss_fs_write_path(path, out, length);
    return false;
}

void rgss_fs_write_path(const char *path, char *out, size_t length) {
    // Reuse the case of any folders that already exist in the save folder.
    char folder[1024];
    snprintf(folder, sizeof folder, "%s", path);
    for (char *p = folder; *p; p++) if (*p == '\\') *p = '/';
    char *slash = strrchr(folder, '/');
    if (!slash) {
        snprintf(out, length, "%s/%s", fs.saveDir, folder);
        if (rgss_resolve_case(fs.saveDir, folder, out, length)) return;
        snprintf(out, length, "%s/%s", fs.saveDir, folder);
        return;
    }
    *slash = 0;
    char resolved[2048];
    rgss_resolve_case(fs.saveDir, folder, resolved, sizeof resolved);
    char file[2048];
    snprintf(file, sizeof file, "%s/%s", resolved, slash + 1);
    char existing[2048];
    if (rgss_resolve_case(resolved, slash + 1, existing, sizeof existing)) snprintf(out, length, "%s", existing);
    else snprintf(out, length, "%s", file);
}

const char *rgss_fs_save_dir(void) { return fs.saveDir; }
const char *rgss_fs_game_dir(void) { return fs.gameDir; }

bool rgss_fs_in_save_dir(const char *path) {
    size_t n = strlen(fs.saveDir);
    return strncmp(path, fs.saveDir, n) == 0 && path[n] == '/';
}
