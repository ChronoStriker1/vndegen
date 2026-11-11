// The compiled project in ysbin/: ysc.ybn (command table), yst_list.ybn (script names), ysl.ybn (labels),
// ysv.ybn (variables), yscfg.ybn (screen size) and yst00000.ybn on (one compiled script each).
#include "yr.h"
#include "vm.h"

const char *const yr_cmd_names[C_COUNT] = {
    [C_UNKNOWN] = "", [C_CDDA] = "CDDA", [C_CG] = "CG", [C_CGACT] = "CGACT", [C_CGEND] = "CGEND",
    [C_CGINFO] = "CGINFO", [C_CLIPACT] = "CLIPACT", [C_CLIPINFO] = "CLIPINFO", [C_CSV] = "CSV",
    [C_CSVINFO] = "CSVINFO", [C_DEBUGLIST] = "DEBUGLIST", [C_DIALOG] = "DIALOG", [C_ELSE] = "ELSE",
    [C_EMOTE] = "EMOTE", [C_EMOTEINFO] = "EMOTEINFO", [C_END] = "END", [C_ERROR] = "ERROR",
    [C_ERRORINFO] = "ERRORINFO", [C_F_FLT] = "F_FLT", [C_F_INT] = "F_INT", [C_F_STR] = "F_STR", [C_F_VAR] = "F_VAR",
    [C_FILEACT] = "FILEACT", [C_FILEINFO] = "FILEINFO", [C_FLASH] = "FLASH", [C_FLASHEND] = "FLASHEND",
    [C_FLASHINFO] = "FLASHINFO", [C_FLT] = "FLT", [C_FONT] = "FONT", [C_FONTINFO] = "FONTINFO", [C_FPS] = "FPS",
    [C_G_FLT] = "G_FLT", [C_G_FLT2] = "G_FLT2", [C_G_FLT3] = "G_FLT3", [C_G_FLT4] = "G_FLT4", [C_G_INT] = "G_INT",
    [C_G_INT2] = "G_INT2", [C_G_INT3] = "G_INT3", [C_G_INT4] = "G_INT4", [C_G_STR] = "G_STR", [C_G_STR2] = "G_STR2",
    [C_G_STR3] = "G_STR3", [C_G_STR4] = "G_STR4", [C_G_VAR] = "G_VAR", [C_GO] = "GO", [C_GOSUB] = "GOSUB",
    [C_IF] = "IF", [C_IFBLEND] = "IFBLEND", [C_IFBREAK] = "IFBREAK", [C_IFCONTINUE] = "IFCONTINUE",
    [C_IFEND] = "IFEND", [C_INPUT] = "INPUT", [C_INT] = "INT", [C_LABEL] = "LABEL", [C_LABELINFO] = "LABELINFO",
    [C_LET] = "LET", [C_LOAD] = "LOAD", [C_LOOP] = "LOOP", [C_LOOPBREAK] = "LOOPBREAK",
    [C_LOOPCONTINUE] = "LOOPCONTINUE", [C_LOOPEND] = "LOOPEND", [C_MACRO] = "MACRO", [C_MATH] = "MATH",
    [C_MATH3D] = "MATH3D", [C_MENU] = "MENU", [C_MENUACT] = "MENUACT", [C_MENUEND] = "MENUEND",
    [C_MENUINFO] = "MENUINFO", [C_MIDI] = "MIDI", [C_MIDIEND] = "MIDIEND", [C_MIDIINFO] = "MIDIINFO",
    [C_MOUSE] = "MOUSE", [C_MOVIE] = "MOVIE", [C_MOVIEEND] = "MOVIEEND", [C_MOVIEINFO] = "MOVIEINFO",
    [C_PLUG] = "PLUG", [C_PREP] = "PREP", [C_REGINFO] = "REGINFO", [C_RETURN] = "RETURN",
    [C_RETURNBREAK] = "RETURNBREAK", [C_RETURNCODE] = "RETURNCODE", [C_S_FLT] = "S_FLT", [C_S_INT] = "S_INT",
    [C_S_STR] = "S_STR", [C_S_VAR] = "S_VAR", [C_SAVE] = "SAVE", [C_SAVEEND] = "SAVEEND",
    [C_SAVEINFO] = "SAVEINFO", [C_SOUND] = "SOUND", [C_SOUNDEND] = "SOUNDEND", [C_SOUNDINFO] = "SOUNDINFO",
    [C_STR] = "STR", [C_SYSTEM] = "SYSTEM", [C_SYSTEMINFO] = "SYSTEMINFO", [C_TASK] = "TASK",
    [C_TASKEND] = "TASKEND", [C_TASKINFO] = "TASKINFO", [C_TEXT] = "TEXT", [C_TEXTINFO] = "TEXTINFO",
    [C_TRAY] = "TRAY", [C_VAR] = "VAR", [C_VARACT] = "VARACT", [C_VARINFO] = "VARINFO", [C_WAIT] = "WAIT",
    [C_WINDOW] = "WINDOW", [C_WINDOWEND] = "WINDOWEND", [C_WINDOWINFO] = "WINDOWINFO", [C_WORD] = "WORD",
    [C_YMV] = "YMV", [C_YMVEND] = "YMVEND", [C_YMVINFO] = "YMVINFO", [C_TEXTOUT] = "_", [C__IFDEF] = "_IFDEF",
    [C__IFNDEF] = "_IFNDEF", [C__IFEND] = "_IFEND", [C_PRODUCT] = "PRODUCT", [C_SYSTEMMODE] = "SYSTEMMODE",
    [C_COMPILEMODE] = "COMPILEMODE", [C_RELEASEMODE] = "RELEASEMODE", [C_PROJECTFOLDER] = "PROJECTFOLDER",
};

static bool read_ysbin(YR *y, const char *name, YFile *out) {
    char path[64];
    snprintf(path, sizeof path, "ysbin/%s", name);
    return yr_fs_read(y, path, NULL, out);
}

static bool load_commands(YR *y) {
    YFile f;
    if (!read_ysbin(y, "ysc.ybn", &f)) return false;
    bool ok = f.size >= 16 && !memcmp(f.data, "YSCM", 4);
    u32 n = ok ? yr_le32(f.data + 8) : 0;
    const u8 *p = f.data + 16, *end = f.data + f.size;
    y->cmds = yr_alloc(sizeof(YCmdInfo) * (n ? n : 1));
    for (u32 i = 0; ok && i < n; i++) {
        YCmdInfo *c = &y->cmds[i];
        const u8 *z = memchr(p, 0, (size_t)(end - p));
        if (!z || z + 2 > end) { ok = false; break; }
        c->name = yr_strndup((const char *)p, (size_t)(z - p));
        p = z + 1;
        c->keyCount = *p++;
        c->keys = yr_alloc(sizeof(char *) * (size_t)(c->keyCount ? c->keyCount : 1));
        for (int k = 0; k < c->keyCount; k++) {
            z = memchr(p, 0, (size_t)(end - p));
            if (!z || z + 3 > end) { ok = false; break; }
            c->keys[k] = yr_strndup((const char *)p, (size_t)(z - p));
            p = z + 3;
        }
        for (int id = 1; id < C_COUNT; id++)
            if (!strcmp(c->name, yr_cmd_names[id])) { c->id = id; break; }
    }
    y->cmdCount = ok ? n : 0;
    yr_file_free(&f);
    return ok;
}

static bool load_list(YR *y) {
    YFile f;
    if (!read_ysbin(y, "yst_list.ybn", &f)) return false;
    bool ok = f.size >= 12 && !memcmp(f.data, "YSTL", 4);
    u32 n = ok ? yr_le32(f.data + 8) : 0;
    // Each entry: index, name length, name, then a tail of 16 or 20 bytes depending on the version.
    int tail = 0;
    static const int tails[] = { 20, 16, 12, 24 };
    for (int t = 0; ok && t < 4 && !tail; t++) {
        size_t o = 12;
        u32 i = 0;
        for (; i < n && o + 8 <= f.size; i++) {
            u32 idx = yr_le32(f.data + o), len = yr_le32(f.data + o + 4);
            if (idx != i || len > 1024) break;
            o += 8 + len + (size_t)tails[t];
        }
        if (i == n && o == f.size) tail = tails[t];
    }
    if (!tail) ok = false;
    y->scripts = yr_alloc(sizeof(YScript) * (n ? n : 1));
    size_t o = 12;
    for (u32 i = 0; ok && i < n; i++) {
        u32 len = yr_le32(f.data + o + 4);
        y->scripts[i].name = yr_strndup((const char *)f.data + o + 8, len);
        o += 8 + len + (size_t)tail;
    }
    y->scriptCount = ok ? n : 0;
    yr_file_free(&f);
    return ok;
}

static u32 label_hash(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) {
        u8 c = (u8)*s;
        if (c >= 'a' && c <= 'z') c = (u8)(c - 'a' + 'A');
        h = (h ^ c) * 16777619u;
    }
    return h;
}

static bool load_labels(YR *y) {
    YFile f;
    if (!read_ysbin(y, "ysl.ybn", &f)) return false;
    bool ok = f.size >= 12 + 1024 && !memcmp(f.data, "YSLB", 4);
    u32 n = ok ? yr_le32(f.data + 8) : 0;
    y->labels = yr_alloc(sizeof(YLabel) * (n ? n : 1));
    y->labelCap = 1;
    while (y->labelCap < n * 2) y->labelCap <<= 1;
    y->labelIndex = yr_alloc(sizeof(int) * y->labelCap);
    for (u32 i = 0; i < y->labelCap; i++) y->labelIndex[i] = -1;
    size_t o = 12 + 1024;
    for (u32 i = 0; ok && i < n; i++) {
        if (o + 1 > f.size) { ok = false; break; }
        u8 len = f.data[o];
        if (o + 1 + len + 12 > f.size) { ok = false; break; }
        YLabel *l = &y->labels[i];
        l->name = yr_strndup((const char *)f.data + o + 1, len);
        l->ip = yr_le32(f.data + o + 1 + len + 4);
        l->script = yr_le16(f.data + o + 1 + len + 8);
        o += 1 + len + 12;
        u32 h = label_hash(l->name) & (y->labelCap - 1);
        while (y->labelIndex[h] >= 0) h = (h + 1) & (y->labelCap - 1);
        y->labelIndex[h] = (int)i;
    }
    y->labelCount = ok ? n : 0;
    yr_file_free(&f);
    return ok;
}

const YLabel *yr_label(YR *y, const char *name) {
    if (!y->labelCap) return NULL;
    u32 h = label_hash(name) & (y->labelCap - 1);
    while (y->labelIndex[h] >= 0) {
        const YLabel *l = &y->labels[y->labelIndex[h]];
        if (!strcasecmp(l->name, name)) return l;
        h = (h + 1) & (y->labelCap - 1);
    }
    return NULL;
}

static void load_config(YR *y) {
    YFile f;
    y->width = 800;
    y->height = 600;
    if (!read_ysbin(y, "yscfg.ybn", &f)) return;
    if (f.size >= 24 && !memcmp(f.data, "YSCF", 4)) {
        u32 w = yr_le32(f.data + 16), h = yr_le32(f.data + 20);
        if (w >= 160 && w <= 8192 && h >= 120 && h <= 8192) { y->width = (int)w; y->height = (int)h; }
        // The window caption is the last field: a 16-bit length and the text.
        for (size_t o = 24; o + 2 <= f.size; o++)
            if (o + 2 + yr_le16(f.data + o) == f.size && yr_le16(f.data + o) < sizeof y->title) {
                memcpy(y->title, f.data + o + 2, yr_le16(f.data + o));
                y->title[yr_le16(f.data + o)] = 0;
                break;
            }
    }
    yr_file_free(&f);
}

// XORs each of the four sections after the header with the key, restarting the key at each section.
static void decrypt(u8 *d, size_t size, u32 key) {
    size_t o = 32;
    for (int s = 0; s < 4; s++) {
        u32 n = yr_le32(d + 12 + 4 * s);
        if (o + n > size) return;
        for (u32 i = 0; i < n; i++) d[o + i] ^= (u8)(key >> (8 * (i & 3)));
        o += n;
    }
}

// The key: the first argument entry's data offset is always 0, so its encrypted offset field is the key itself.
static bool find_key(YR *y) {
    for (u32 i = 0; i < y->scriptCount; i++) {
        char name[32];
        snprintf(name, sizeof name, "yst%05u.ybn", i);
        YFile f;
        if (!read_ysbin(y, name, &f)) continue;
        bool found = false;
        if (f.size >= 32 && !memcmp(f.data, "YSTB", 4)) {
            u32 code = yr_le32(f.data + 12), args = yr_le32(f.data + 16);
            if (args >= 12 && 32 + (size_t)code + 12 <= f.size) {
                y->key = yr_le32(f.data + 32 + code + 8);
                y->scriptVersion = yr_le32(f.data + 4);
                found = true;
            }
        }
        yr_file_free(&f);
        if (found) return true;
    }
    return false;
}

const char *yr_arg_key(YR *y, const YIns *ins, const YArg *a) {
    const YCmdInfo *c = &y->cmds[ins->cmd];
    return a->key < c->keyCount ? c->keys[a->key] : "";
}

// Links IF, ELSE, IFBLEND and IFEND, and LOOP and LOOPEND, by nesting.
static void link_blocks(YR *y, YScript *s) {
    u32 ifs[256], loops[256], pending[256][32];
    int ifTop = 0, loopTop = 0, pendingCount[256];
    for (u32 i = 0; i < s->count; i++) {
        YIns *in = &s->ins[i];
        int id = y->cmds[in->cmd].id;
        switch (id) {
        case C_IF:
            if (ifTop < 256) { pendingCount[ifTop] = 0; ifs[ifTop++] = i; }
            break;
        case C_ELSE:
            if (ifTop) { s->ins[ifs[ifTop - 1]].jump = i; ifs[ifTop - 1] = i; }
            break;
        case C_IFBLEND:
            if (ifTop && pendingCount[ifTop - 1] < 32) pending[ifTop - 1][pendingCount[ifTop - 1]++] = i;
            break;
        case C_IFEND:
            if (ifTop) {
                ifTop--;
                s->ins[ifs[ifTop]].jump = i;
                for (int k = 0; k < pendingCount[ifTop]; k++) s->ins[pending[ifTop][k]].jump = i;
            }
            break;
        case C_LOOP:
            if (loopTop < 256) loops[loopTop++] = i;
            break;
        case C_LOOPEND:
            if (loopTop) { loopTop--; s->ins[loops[loopTop]].jump = i; in->jump = loops[loopTop]; }
            break;
        }
    }
}

YScript *yr_script(YR *y, u16 index) {
    if (index >= y->scriptCount) return NULL;
    YScript *s = &y->scripts[index];
    if (s->loaded) return s->ins ? s : NULL;
    s->loaded = true;
    char name[32];
    snprintf(name, sizeof name, "yst%05u.ybn", index);
    YFile f;
    if (!read_ysbin(y, name, &f)) return NULL;
    if (f.size < 32 || memcmp(f.data, "YSTB", 4)) { yr_file_free(&f); return NULL; }
    u32 count = yr_le32(f.data + 8), code = yr_le32(f.data + 12), args = yr_le32(f.data + 16);
    u32 data = yr_le32(f.data + 20), lines = yr_le32(f.data + 24);
    if (32 + (size_t)code + args + data + lines > f.size || code < count * 4) { yr_file_free(&f); return NULL; }
    decrypt(f.data, f.size, y->key);
    s->raw = f.data;
    s->count = count;
    s->ins = yr_alloc(sizeof(YIns) * (count ? count : 1));
    s->argCount = args / 12;
    s->args = yr_alloc(sizeof(YArg) * (s->argCount ? s->argCount : 1));
    const u8 *argp = f.data + 32 + code, *datap = argp + args;
    for (u32 i = 0; i < s->argCount; i++) {
        const u8 *a = argp + 12 * i;
        YArg *g = &s->args[i];
        g->key = yr_le16(a);
        g->type = yr_le16(a + 2);
        g->size = yr_le32(a + 4);
        u32 off = yr_le32(a + 8);
        if ((u64)off + g->size > data) g->size = 0;
        g->data = datap + off;
    }
    u32 next = 0;
    for (u32 i = 0; i < count; i++) {
        const u8 *c = f.data + 32 + 4 * i;
        YIns *in = &s->ins[i];
        in->cmd = c[0] < y->cmdCount ? c[0] : 0;
        in->argc = c[1];
        in->arg = next;
        if (next + in->argc > s->argCount) in->argc = (u8)(s->argCount - next);
        next += in->argc;
        in->line = lines >= count * 4 ? yr_le32(datap + data + 4 * i) : 0;
    }
    link_blocks(y, s);
    return s;
}

// Text in scripts is Shift-JIS, except in games built with the engine's UTF-8 option, which nothing in the project
// files records. Shift-JIS text is almost never valid UTF-8, so the string data of a few scripts decides.
static void detect_encoding(YR *y) {
    long good = 0, bad = 0;
    for (u32 i = 0; i < y->scriptCount && i < 400 && good + bad < 4000; i++) {
        YScript *s = yr_script(y, (u16)i);
        if (!s) continue;
        for (u32 a = 0; a < s->argCount; a++) {
            // Only string literals: numbers in the token stream are binary.
            const u8 *d = s->args[a].data;
            u32 n = s->args[a].size, o = 0;
            while (o + 3 <= n) {
                u32 len = yr_le16(d + o + 1);
                if (o + 3 + len > n) break;
                if (d[o] == 'M') {
                    const u8 *p = d + o + 3, *end = p + len;
                    while (p < end) {
                        if (*p < 0x80) { p++; continue; }
                        int k = (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : (*p & 0xf8) == 0xf0 ? 4 : 0;
                        bool ok = k && p + k <= end;
                        for (int j = 1; ok && j < k; j++) ok = (p[j] & 0xc0) == 0x80;
                        if (ok) { good++; p += k; }
                        else { bad++; p++; }
                    }
                }
                o += 3 + len;
            }
        }
    }
    y->enc = good >= 20 && bad * 20 < good ? YR_UTF8 : YR_SJIS;
}

bool yr_script_open(YR *y) {
    if (!load_commands(y)) { snprintf(y->problem, sizeof y->problem, "ysbin/ysc.ybn is missing or unreadable"); return false; }
    if (!load_list(y)) { snprintf(y->problem, sizeof y->problem, "ysbin/yst_list.ybn is missing or unreadable"); return false; }
    if (!load_labels(y)) { snprintf(y->problem, sizeof y->problem, "ysbin/ysl.ybn is missing or unreadable"); return false; }
    load_config(y);
    if (!find_key(y)) { snprintf(y->problem, sizeof y->problem, "no readable script in ysbin"); return false; }
    YFile v;
    if (!read_ysbin(y, "ysv.ybn", &v) || !yr_vars_open(y, v.data, v.size, y->scriptVersion)) {
        yr_file_free(&v);
        snprintf(y->problem, sizeof y->problem, "ysbin/ysv.ybn is missing or unreadable");
        return false;
    }
    yr_file_free(&v);
    detect_encoding(y);
    if (y->enc == YR_UTF8) yr_log(y, "text is UTF-8\n");
    yr_log(y, "project: version %u, %u commands, %u scripts, %u labels, key %08x, screen %dx%d\n", y->scriptVersion,
           y->cmdCount, y->scriptCount, y->labelCount, y->key, y->width, y->height);
    return true;
}

void yr_script_close(YR *y) {
    for (u32 i = 0; i < y->cmdCount; i++) {
        free(y->cmds[i].name);
        for (int k = 0; k < y->cmds[i].keyCount; k++) free(y->cmds[i].keys[k]);
        free(y->cmds[i].keys);
    }
    free(y->cmds);
    for (u32 i = 0; i < y->scriptCount; i++) {
        free(y->scripts[i].name);
        free(y->scripts[i].raw);
        free(y->scripts[i].ins);
        free(y->scripts[i].args);
    }
    free(y->scripts);
    for (u32 i = 0; i < y->labelCount; i++) free(y->labels[i].name);
    free(y->labels);
    free(y->labelIndex);
    yr_vars_close(y);
}
