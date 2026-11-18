#include "ipac.h"

#include <sys/stat.h>
#include <time.h>

static char *slot_path(IP *t, int slot) { return ip_fmt("%s/_DAT%04d.DAT", t->saveDir, slot); }

bool save_exists(IP *t, int slot) {
    char *p = slot_path(t, slot);
    bool ok = ip_file_exists(p);
    free(p);
    return ok;
}

bool save_info(IP *t, int slot, SlotInfo *info) {
    char *p = slot_path(t, slot);
    size_t n;
    u8 *d = ip_read_file(p, &n);
    struct stat st;
    bool have = d && stat(p, &st) == 0;
    free(p);
    if (!have) {
        free(d);
        return false;
    }
    memset(info, 0, sizeof *info);
    if (n >= 8 && !memcmp(d, "IEL1", 4)) {
        u8 *raw = lzss_decode(d + 8, n - 8, 0x200);
        snprintf(info->scene, sizeof info->scene, "%.*s", 0x19, (const char *)raw + 0x146);
        free(raw);
    }
    free(d);
    time_t mt = st.st_mtime;
    struct tm tm;
    localtime_r(&mt, &tm);
    info->month = tm.tm_mon + 1;
    info->day = tm.tm_mday;
    info->hour = tm.tm_hour;
    info->minute = tm.tm_min;
    return true;
}

void load_system(IP *t) { (void)t; }

void save_system(IP *t) { (void)t; }

bool save_slot(IP *t, int slot) {
    ip_log(t, "save %d not built yet", slot);
    return false;
}

bool load_slot(IP *t, int slot) {
    ip_log(t, "load %d not built yet", slot);
    return false;
}
