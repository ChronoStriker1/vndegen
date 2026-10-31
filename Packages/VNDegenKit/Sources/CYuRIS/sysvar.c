// System variables: the ones whose names start with an underscore, ids below 1000. Their ids differ between engine
// versions; sysvar_tables.c holds each version's names and ids, read from scripts compiled with the SDK compilers.
#include "yr.h"
#include "vm.h"
#include <math.h>
#include <time.h>

typedef struct YSysName { const char *name; u16 id; } YSysName;
typedef struct YSysTable { u32 minVersion, maxVersion; const YSysName *names; int count; } YSysTable;
extern const YSysTable yr_sys_tables[];
extern const int yr_sys_table_count;

// Button codes. Keys use their Windows virtual-key codes; mouse buttons and the wheel follow at 0x100.
enum { CODE_MOUSE = 0x100, CODE_PAD = 0x120 };

int yr_code_for_vk(int vk) { return vk > 0 && vk < 0x100 ? vk : 0; }
int yr_code_mouse(int button) { return CODE_MOUSE + button; }

static int key_vk(const char *k) {
    if (!k[1] && ((k[0] >= 'A' && k[0] <= 'Z') || (k[0] >= '0' && k[0] <= '9'))) return k[0];
    if (!strncmp(k, "NUM", 3) && k[3] >= '0' && k[3] <= '9' && !k[4]) return 0x60 + k[3] - '0';
    if (k[0] == 'F' && k[1] >= '1' && k[1] <= '9') {
        int n = atoi(k + 1);
        if (n >= 1 && n <= 24) return 0x70 + n - 1;
    }
    static const struct { const char *n; int vk; } names[] = {
        { "BS", 0x08 }, { "TAB", 0x09 }, { "SPACE", 0x20 }, { "ENTER", 0x0d }, { "SHIFT", 0x10 }, { "CTRL", 0x11 },
        { "ALT", 0x12 }, { "LEFT", 0x25 }, { "RIGHT", 0x27 }, { "UP", 0x26 }, { "DOWN", 0x28 }, { "INSERT", 0x2d },
        { "DELETE", 0x2e }, { "HELP", 0x2f }, { "SCROLL", 0x91 }, { "NUMLOCK", 0x90 }, { "ESC", 0x1b },
        { "PAGEUP", 0x21 }, { "PAGEDOWN", 0x22 }, { "HOME", 0x24 }, { "END", 0x23 }, { "NUMADD", 0x6b },
        { "NUMSUB", 0x6d }, { "NUMMUL", 0x6a }, { "NUMDIV", 0x6f }, { "NUMDEC", 0x6e }, { "HYPHEN", 0xbd },
        { "CIRCUMFLEX", 0xde }, { "YEN", 0xdc }, { "AT", 0xc0 }, { "LEFTBRACKET", 0xdb }, { "RIGHTBRACKET", 0xdd },
        { "SEMICOLON", 0xbb }, { "COLON", 0xba }, { "COMMA", 0xbc }, { "PERIOD", 0xbe }, { "SLASH", 0xbf },
        { "BACKSLASH", 0xe2 },
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcmp(k, names[i].n)) return names[i].vk;
    return 0;
}

static int mouse_index(const char *k) {
    static const char *const m[] = { "L", "R", "M", "U", "D" };
    for (int i = 0; i < 5; i++) if (!strcmp(k, m[i])) return i;
    return -1;
}

static YSysVar classify(const char *name) {
    static const struct { const char *n; YSys k; } plain[] = {
        { "_TIME", S_TIME }, { "_OS_TIME", S_OS_TIME }, { "_OS_VERSION", S_OS_VERSION },
        { "_OS_DXVERSION", S_OS_DXVERSION }, { "_OS_SOUND", S_OS_SOUND }, { "_OS_MEMTOTAL", S_OS_MEMTOTAL },
        { "_OS_MEMAVAIL", S_OS_MEMAVAIL }, { "_MEDIA_READONLY", S_MEDIA_READONLY }, { "_PAI", S_PAI },
        { "_DEBUGMODE", S_DEBUGMODE }, { "_ARGV", S_ARGV }, { "_ARGC", S_ARGC }, { "_LC", S_LC }, { "_PINT", S_PINT },
        { "_PFLT", S_PFLT }, { "_PSTR", S_PSTR }, { "_PFLG", S_PFLG }, { "_RINT", S_RINT }, { "_RFLT", S_RFLT },
        { "_RSTR", S_RSTR }, { "_RFLG", S_RFLG }, { "_SYSTEM_PATH", S_SYSTEM_PATH }, { "_SAVE_PATH", S_SAVE_PATH },
        { "_APPDATA_PATH", S_APPDATA_PATH }, { "_PERSONAL_PATH", S_PERSONAL_PATH }, { "_PROJECT_PATH", S_PROJECT_PATH },
        { "_SYSTEM_VERSION", S_SYSTEM_VERSION }, { "_SCREEN_COLOR", S_SCREEN_COLOR },
        { "_SCREEN_SIZE_X", S_SCREEN_SIZE_X }, { "_SCREEN_SIZE_Y", S_SCREEN_SIZE_Y },
        { "_WINDOW_EXEC_MAX", S_WINDOW_EXEC_MAX }, { "_WINDOW_EXEC_CLOSE", S_WINDOW_EXEC_CLOSE },
        { "_WINDOW_EXEC_CAPTION", S_WINDOW_EXEC_CAPTION }, { "_TEXT", S_TEXT }, { "_TEXT_FLAG", S_TEXT_FLAG },
        { "_TEXT_NUM", S_TEXT_NUM }, { "_MENUCODE", S_MENUCODE }, { "_MOUSE_X", S_MOUSE_X }, { "_MOUSE_Y", S_MOUSE_Y },
        { "_MOUSE_X2", S_MOUSE_X2 }, { "_MOUSE_Y2", S_MOUSE_Y2 }, { "_PAD_NUMBER", S_PAD_NUMBER }, { "_INP", S_INP },
        { "_KEY_ANY", S_KEY_ANY }, { "_KEYLAST", S_KEYLAST }, { "_MOUSELAST", S_MOUSELAST }, { "_CHR", S_CHR },
        { "_DXMODE", S_DXMODE }, { "_TINT", S_TINT }, { "_TFLT", S_TFLT }, { "_TSTR", S_TSTR },
        { "_VARTYPE", S_VARTYPE },
    };
    for (size_t i = 0; i < sizeof plain / sizeof plain[0]; i++)
        if (!strcmp(name, plain[i].n)) return (YSysVar){ plain[i].k, 0 };
    int m;
    if (!strncmp(name, "_CODE_MOUSE_", 12) && (m = mouse_index(name + 12)) >= 0) return (YSysVar){ S_CODE_MOUSE, m };
    if (!strncmp(name, "_MOUSENUM_", 10) && (m = mouse_index(name + 10)) >= 0) return (YSysVar){ S_MOUSENUM, m };
    if (!strncmp(name, "_MOUSE_", 7) && (m = mouse_index(name + 7)) >= 0) return (YSysVar){ S_MOUSE, m };
    if (!strncmp(name, "_CODE_PAD_", 10)) return (YSysVar){ S_CODE_PAD, 0 };
    if (!strncmp(name, "_PADNUM_", 8)) return (YSysVar){ S_PADNUM, 0 };
    if (!strncmp(name, "_PAD_", 5)) return (YSysVar){ S_PAD, 0 };
    int vk;
    if (!strncmp(name, "_CODE_", 6) && (vk = key_vk(name + 6))) return (YSysVar){ S_CODE, vk };
    if (!strncmp(name, "_KEYNUM_", 8) && (vk = key_vk(name + 8))) return (YSysVar){ S_KEYNUM, vk };
    if (!strncmp(name, "_KEY_", 5) && (vk = key_vk(name + 5))) return (YSysVar){ S_KEY, vk };
    return (YSysVar){ S_OTHER, 0 };
}

void yr_sysvars_init(YR *y, u32 version) {
    for (int i = 0; i < 1000; i++) y->sys[i] = (YSysVar){ S_OTHER, 0 };
    const YSysTable *best = NULL;
    for (int i = 0; i < yr_sys_table_count; i++) {
        const YSysTable *t = &yr_sys_tables[i];
        if (version >= t->minVersion && version <= t->maxVersion) { best = t; break; }
    }
    if (!best) best = &yr_sys_tables[yr_sys_table_count - 1];
    for (int i = 0; i < best->count; i++) {
        u16 id = best->names[i].id;
        if (id < 1000) y->sys[id] = classify(best->names[i].name + 1);   // skip the sigil
    }
    yr_log(y, "system variables: table for versions %u to %u\n", best->minVersion, best->maxVersion);
}

// ---- reading and writing ----

static YValues *params(YTask *t) { return t && t->depth ? &t->frames[t->depth - 1].params : NULL; }
static YValues *results(YTask *t) { return t && t->depth ? &t->frames[t->depth - 1].results : NULL; }

static int param_index(int n, const i64 *sub) {
    int i = n ? (int)sub[0] : 1;
    return i >= 0 && i < YR_PARAMS ? i : 0;
}

static i64 held_value(YR *y, int code) { return code > 0 && code < YR_CODE_COUNT ? y->held[code] : 0; }

bool yr_sys_dynamic(const YSysVar *s) {
    switch (s->kind) {
    case S_NONE: case S_OTHER: case S_WINDOW_EXEC_MAX: case S_WINDOW_EXEC_CLOSE: case S_WINDOW_EXEC_CAPTION:
    case S_TEXT_NUM: case S_DXMODE: case S_VARTYPE:
        return false;
    default:
        return true;
    }
}

YVal yr_sys_get(YR *y, YTask *t, const YSysVar *s, int n, const i64 *sub) {
    int idx = param_index(n, sub);
    switch (s->kind) {
    case S_TIME: {
        time_t now = (time_t)(y->startUnix + (i64)(y->clock / 1000));
        int which = n >= 2 ? (int)sub[1] : n == 1 ? (int)sub[0] % 8 : 0;
        if (n >= 2 && sub[0] == 1) now = (time_t)y->startUnix;
        struct tm tm;
        localtime_r(&now, &tm);
        i64 v[8] = { tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_wday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                     (i64)(y->clock % 1000) };
        return yv_int(which >= 0 && which < 8 ? v[which] : 0);
    }
    case S_OS_TIME: return yv_int((i64)y->clock);
    case S_OS_VERSION: return yv_int(90);
    case S_OS_DXVERSION: return yv_int(9);
    case S_OS_SOUND: return yv_int(1);
    case S_OS_MEMTOTAL: return yv_int((i64)4 << 30);
    case S_OS_MEMAVAIL: return yv_int((i64)2 << 30);
    case S_MEDIA_READONLY: return yv_int(0);
    case S_PAI: return yv_flt(M_PI);
    case S_DEBUGMODE: return yv_int(0);
    case S_ARGC: return yv_int(0);
    case S_ARGV: return yv_str("");
    case S_LC: return yv_int(t && t->loopDepth ? t->loops[t->loopDepth - 1].lc : 0);
    case S_PINT: { YValues *p = params(t); return yv_int(p ? p->i[idx] : 0); }
    case S_PFLT: { YValues *p = params(t); return yv_flt(p ? p->f[idx] : 0); }
    case S_PSTR: { YValues *p = params(t); return yv_str(p && p->s[idx] ? p->s[idx] : ""); }
    case S_PFLG: { YValues *p = params(t); return yv_int(p ? p->flag[idx] : 0); }
    case S_RINT: { YValues *p = results(t); return yv_int(p ? p->i[idx] : 0); }
    case S_RFLT: { YValues *p = results(t); return yv_flt(p ? p->f[idx] : 0); }
    case S_RSTR: { YValues *p = results(t); return yv_str(p && p->s[idx] ? p->s[idx] : ""); }
    case S_RFLG: { YValues *p = results(t); return yv_int(p ? p->flag[idx] : 0); }
    case S_SYSTEM_PATH: case S_PROJECT_PATH: return yv_str(".\\");
    case S_SAVE_PATH: return yv_str("savedata\\");
    case S_APPDATA_PATH: return yv_str("C:\\Users\\user\\AppData\\Roaming\\");
    case S_PERSONAL_PATH: return yv_str("C:\\Users\\user\\Documents\\");
    case S_SYSTEM_VERSION: return yv_int(y->scriptVersion);
    case S_SCREEN_COLOR: return yv_int(32);
    case S_SCREEN_SIZE_X: return yv_int(1920);
    case S_SCREEN_SIZE_Y: return yv_int(1080);
    case S_TEXT: return yv_str(y->text ? y->text : "");
    case S_TEXT_FLAG: return yv_str(y->textFlag ? y->textFlag : "");
    case S_MENUCODE: return yv_int(y->menuCode);
    case S_MOUSE_X: case S_MOUSE_X2: return yv_int(y->mouseX);
    case S_MOUSE_Y: case S_MOUSE_Y2: return yv_int(y->mouseY);
    case S_PAD_NUMBER: return yv_int(0);
    case S_INP: return yv_int(held_value(y, n ? (int)sub[0] : 0));
    case S_KEY_ANY: {
        for (int c = 1; c < CODE_MOUSE; c++) if (y->held[c] > 0) return yv_int(y->held[c]);
        return yv_int(-1);
    }
    case S_KEYLAST: return yv_int(y->lastKey);
    case S_MOUSELAST: return yv_int(y->lastMouse);
    case S_CHR: {
        // $_CHR(n): the byte n as a one-byte string; scripts build two-byte codes from two of them.
        char b[2] = { (char)(n ? sub[0] : 0), 0 };
        return yv_str(b);
    }
    case S_KEY: return yv_int(held_value(y, s->arg));
    case S_KEYNUM: return yv_int(s->arg < YR_CODE_COUNT ? y->pressCount[s->arg] : 0);
    case S_CODE: return yv_int(s->arg);
    case S_MOUSE: return yv_int(held_value(y, CODE_MOUSE + s->arg));
    case S_MOUSENUM: return yv_int(y->pressCount[CODE_MOUSE + s->arg]);
    case S_CODE_MOUSE: return yv_int(CODE_MOUSE + s->arg);
    case S_PAD: case S_PADNUM: return yv_int(-1);
    case S_CODE_PAD: return yv_int(CODE_PAD);
    case S_TINT: return yv_int(t ? t->tvals.i[idx] : 0);
    case S_TFLT: return yv_flt(t ? t->tvals.f[idx] : 0);
    case S_TSTR: return yv_str(t && t->tvals.s[idx] ? t->tvals.s[idx] : "");
    default: break;
    }
    return yv_int(0);
}

void yr_sys_set(YR *y, YTask *t, const YSysVar *s, int n, const i64 *sub, const YVal *v) {
    int idx = param_index(n, sub);
    switch (s->kind) {
    case S_TEXT: free(y->text); y->text = yv_as_str(v); break;
    case S_TEXT_FLAG: free(y->textFlag); y->textFlag = yv_as_str(v); break;
    case S_MENUCODE: y->menuCode = yv_as_int(v); break;
    case S_LC: if (t && t->loopDepth) t->loops[t->loopDepth - 1].lc = yv_as_int(v); break;
    case S_PINT: { YValues *p = params(t); if (p) p->i[idx] = yv_as_int(v); break; }
    case S_PFLT: { YValues *p = params(t); if (p) p->f[idx] = yv_as_flt(v); break; }
    case S_PSTR: { YValues *p = params(t); if (p) { free(p->s[idx]); p->s[idx] = yv_as_str(v); } break; }
    case S_RINT: { YValues *p = results(t); if (p) p->i[idx] = yv_as_int(v); break; }
    case S_RFLT: { YValues *p = results(t); if (p) p->f[idx] = yv_as_flt(v); break; }
    case S_RSTR: { YValues *p = results(t); if (p) { free(p->s[idx]); p->s[idx] = yv_as_str(v); } break; }
    case S_KEY: case S_MOUSE: {
        int code = s->kind == S_KEY ? s->arg : CODE_MOUSE + s->arg;
        if (code > 0 && code < YR_CODE_COUNT) y->held[code] = (int)yv_as_int(v);
        break;
    }
    case S_INP: {
        int code = n ? (int)sub[0] : 0;
        if (code > 0 && code < YR_CODE_COUNT) y->held[code] = (int)yv_as_int(v);
        break;
    }
    case S_KEYNUM: if (s->arg < YR_CODE_COUNT) y->pressCount[s->arg] = (int)yv_as_int(v); break;
    case S_MOUSENUM: y->pressCount[CODE_MOUSE + s->arg] = (int)yv_as_int(v); break;
    default: break;
    }
}

// Once per frame: a held button counts up from 1, a released one down from -1.
void yr_input_frame(YR *y) {
    for (int c = 1; c < YR_CODE_COUNT; c++) {
        if (y->down[c] || y->pressedThisFrame[c]) {
            y->held[c] = y->held[c] > 0 ? y->held[c] + 1 : 1;
        } else {
            y->held[c] = y->held[c] < 0 ? y->held[c] - 1 : -1;
        }
        if (y->held[c] > 1000000) y->held[c] = 1000000;
        if (y->held[c] < -1000000) y->held[c] = -1000000;
        y->pressedThisFrame[c] = false;
    }
    // The wheel counts as pressed for the frame it turned.
    int up = CODE_MOUSE + 3, dn = CODE_MOUSE + 4;
    y->held[up] = y->wheel > 0 ? 1 : (y->held[up] > 0 ? -1 : y->held[up]);
    y->held[dn] = y->wheel < 0 ? 1 : (y->held[dn] > 0 ? -1 : y->held[dn]);
    if (y->wheel > 0) y->pressCount[up]++;
    if (y->wheel < 0) y->pressCount[dn]++;
    y->wheel = 0;
}
