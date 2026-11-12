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

static void window(YCall *c) {
    YR *y = c->y;
    if (yc_int_or(c, "NO", 0) != 0) return;
    char *cap = yc_str(c, "CAPTION");
    if (cap) {
        char *u = yr_to_utf8(y->enc, cap);
        snprintf(y->title, sizeof y->title, "%s", u);
        free(u);
        free(cap);
    }
    i64 fs;
    if (yc_int(c, "FULLSCREEN", &fs)) y->fullscreen = fs != 0;
}

static void windowinfo(YCall *c) {
    YR *y = c->y;
    YRef out;
    if (!yc_ref(c, "LET", &out)) return;
    i64 no = yc_int_or(c, "NO", 0);
    YVal r = yv_int(0);
    if (no != 0) r = yv_int(0);
    else if (yc_int_or(c, "CAPTION", 0)) r = yv_str(y->title);
    else if (yc_int_or(c, "SX", 0) || yc_int_or(c, "WSX", 0)) r = yv_int(y->width);
    else if (yc_int_or(c, "SY", 0) || yc_int_or(c, "WSY", 0)) r = yv_int(y->height);
    else if (yc_int_or(c, "FULLSCREEN", 0) || yc_int_or(c, "FULLSCREEN2", 0)) r = yv_int(y->fullscreen);
    else if (yc_int_or(c, "EXIST", 0) || yc_int_or(c, "E", 0) || yc_int_or(c, "ACTIVE", 0)) r = yv_int(1);
    else if (yc_int_or(c, "FULLSCREENBUTTON", 0)) r = yv_int(1);
    yr_ref_set(y, c->t, &out, &r);
    yv_free(&r);
}

static void fileact(YCall *c) {
    YR *y = c->y;
    if (yc_int_or(c, "DELETE", 0)) {
        char *file = yc_str(c, "FILE");
        char *path = file ? yr_save_path(y, file) : NULL;
        if (path) remove(path);
        free(path);
        free(file);
    } else if (yc_int_or(c, "CREATE", 0)) {
        char *folder = yc_str(c, "FOLDER");
        if (folder) yr_save_mkdir(y, folder);
        free(folder);
    } else if (yc_has(c, "ID")) {
        yc_unknown(c, "FILEACT layer export");
    }
}

static void set_result(YCall *c, YVal v) {
    YRef out;
    if (yc_ref(c, "LET", &out)) yr_ref_set(c->y, c->t, &out, &v);
    yv_free(&v);
}

static void set_argv(YR *y, char **list, int n) {
    for (int i = 0; i < y->argc; i++) free(y->argv[i]);
    free(y->argv);
    y->argv = list;
    y->argc = n;
}

static int product(YCall *c) {
    YR *y = c->y;
    i64 com = yc_int_or(c, "COM", 0);
    if (com == 21 || com == 24) {
        char *folder = yc_str(c, "PSTR");
        char **list = NULL;
        int n = folder ? yr_fs_list(y, folder, &list) : 0;
        if (com == 21) {
            size_t skip = strlen(folder ? folder : "");
            for (int i = 0; i < n; i++) {
                char *s = yr_strdup(list[i] + (strlen(list[i]) >= skip ? skip : 0));
                free(list[i]);
                list[i] = s;
            }
        }
        set_argv(y, list, n);
        free(folder);
        return YR_NEXT;
    }
    set_result(c, yv_int(com == 29 ? 1 : 0));
    return YR_NEXT;
}

int yr_cmd_system(YCall *c, int id) {
    YR *y = c->y;
    switch (id) {
    case C_WAIT: return wait_cmd(c);
    case C_DIALOG: dialog(c); return YR_NEXT;
    case C_FILEINFO: fileinfo(c); return YR_NEXT;
    case C_WINDOW: window(c); return YR_NEXT;
    case C_WINDOWINFO: windowinfo(c); return YR_NEXT;
    case C_FILEACT: fileact(c); return YR_NEXT;
    case C_FPS: {
        i64 f;
        if (yc_int(c, "SET", &f) && f >= 1 && f <= 256) y->fps = (int)f;
        return YR_NEXT;
    }
    case C_INPUT: case C_MOUSE: case C_SYSTEM: case C_ERROR: case C_DEBUGLIST: case C_EMOTE: case C_TRAY:
    case C_WINDOWEND: case C_MENU: case C_MENUACT: case C_MENUEND:
        return YR_NEXT;
    case C_PRODUCT: return product(c);
    case C_SYSTEMINFO: case C_EMOTEINFO: case C_MENUINFO: case C_REGINFO:
        set_result(c, yv_int(0));
        return YR_NEXT;
    case C_END:
        yr_log(y, "END\n");
        y->quit = true;
        return YR_STOP;
    default: {
        if (id >= 0 && id < C_COUNT && !y->unknownSeen[id]) {
            y->unknownSeen[id] = true;
            yc_unknown(c, "command");
        }
        return YR_NEXT;
    }
    }
}
