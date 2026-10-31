// System commands: WAIT, END, and the window, input and file commands.
#include "yr.h"
#include "vm.h"

static int wait_cmd(YCall *c) {
    i64 frames = 0, ms = 0;
    bool hasFrames = yc_int(c, "FRAME", &frames), hasTime = yc_int(c, "TIME", &ms);
    if (hasTime && ms > 0) c->t->waitUntil = c->y->clock + (u64)ms;
    if (hasFrames && frames > 1) c->t->waitFrames = (int)frames - 1;
    return YR_YIELD;
}

static void dialog(YCall *c) {
    YR *y = c->y;
    static const char *const keys[] = { "CAPTION", "STR", "STR2", "STR3", "STR4" };
    yr_log(y, "DIALOG");
    for (int i = 0; i < 5; i++) {
        char *s = yc_str(c, keys[i]);
        if (!s) continue;
        char *u = yr_to_utf8(y->enc, s);
        yr_log(y, " %s=\"%s\"", keys[i], u);
        free(u);
        free(s);
    }
    yr_log(y, "\n");
    // Yes-or-no and OK-or-cancel dialogs answer as if the first button were pressed.
    YRef out;
    if (yc_ref(c, "LET", &out)) {
        YVal r = yv_int(1);
        yr_ref_set(y, c->t, &out, &r);
    }
}

static const char *const anyExt[] = { ".png", ".jpg", ".bmp", ".ogg", ".wav", ".ybn", ".txt", ".csv", NULL };

static void fileinfo(YCall *c) {
    YR *y = c->y;
    YRef out;
    if (!yc_ref(c, "LET", &out)) return;
    char *file = yc_str(c, "FILE");
    if (!file) file = yc_str(c, "CGFILE");
    if (!file) file = yc_str(c, "SOUNDFILE");
    char *folder = yc_str(c, "FOLDER");
    YVal r = yv_int(0);
    if (yc_int_or(c, "EXIST", 0)) {
        if (file) r = yv_int(yr_fs_exists(y, file, anyExt) || yr_save_exists(y, file));
        else if (folder) r = yv_int(1);
    } else if (yc_int_or(c, "SIZE", 0) && file) {
        YFile f;
        if (yr_fs_read(y, file, anyExt, &f)) { r = yv_int((i64)f.size); yr_file_free(&f); }
    } else yc_unknown(c, "FILEINFO form");
    yr_ref_set(y, c->t, &out, &r);
    free(file);
    free(folder);
}

int yr_cmd_system(YCall *c, int id) {
    YR *y = c->y;
    switch (id) {
    case C_WAIT: return wait_cmd(c);
    case C_DIALOG: dialog(c); return YR_NEXT;
    case C_FILEINFO: fileinfo(c); return YR_NEXT;
    case C_END:
        yr_log(y, "END\n");
        y->quit = true;
        return YR_STOP;
    default: {
        static bool seen[C_COUNT];
        if (id >= 0 && id < C_COUNT && !seen[id]) {
            seen[id] = true;
            yc_unknown(c, "command");
        }
        return YR_NEXT;
    }
    }
}
