// Internal declarations shared by the YU-RIS runtime's source files.
#ifndef YR_H
#define YR_H

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "vnyuris.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;

typedef struct YR YR;

// ---- util.c ----
void *yr_alloc(size_t n);                 // zeroed, aborts on failure
void *yr_realloc(void *p, size_t n);
char *yr_strdup(const char *s);
char *yr_strndup(const char *s, size_t n);
char *yr_path(const char *dir, const char *name);
u8 *yr_read_file(const char *path, size_t *size);
bool yr_write_file(const char *path, const void *data, size_t size);
u16 yr_le16(const u8 *p);
u32 yr_le32(const u8 *p);
u64 yr_le64(const u8 *p);
u32 yr_murmur2(const u8 *data, size_t len);
u32 yr_crc32(const u8 *data, size_t len);
// ASCII-only lowercase copy with backslashes turned to slashes, for file and label keys.
void yr_key(char *out, size_t size, const char *s);
// Finds dir/name whatever the case of name, as Windows does. Returns a malloc'd path or NULL.
char *yr_find_file(const char *dir, const char *name);
void yr_log(YR *y, const char *fmt, ...);

// Text encodings. Strings stay in the game's own bytes; these step through them by character.
enum { YR_SJIS = 0, YR_UTF8 = 1 };
int yr_char_len(int enc, const u8 *p);              // bytes in the character at p (1 at a NUL)
int yr_char_decode(int enc, const u8 *p, u32 *cp);  // as yr_char_len, with the Unicode code point
char *yr_to_utf8(int enc, const char *s);           // malloc'd
int yr_sjis_decode(const u8 *p, u32 *cp);

// ---- fs.c: .ypf archives and loose files ----
typedef struct YFile { u8 *data; size_t size; } YFile;
bool yr_fs_open(YR *y);
void yr_fs_close(YR *y);
// Reads a game file by its script name (such as "cg/bg/bg01.png" or "ysbin\\ysc.ybn"). The extension may be left
// out, as games do for CGs and sounds; exts lists the ones to try then. Returns false when missing.
bool yr_fs_read(YR *y, const char *name, const char *const *exts, YFile *out);
bool yr_fs_exists(YR *y, const char *name, const char *const *exts);
void yr_file_free(YFile *f);

// ---- script.c: ysc, yst, ysv, ysl, yst_list, yscf ----
typedef struct YArg {
    u16 key;          // keyword index in the command's table entry
    u16 type;         // low byte: value type (0 none, 1 int, 2 float, 3 string); high byte: assignment operator
    u32 size;
    const u8 *data;   // postfix expression, or raw text for WORD
} YArg;

typedef struct YIns {
    u16 cmd;          // index in the game's command table (y->cmds)
    u8 argc;
    u32 arg;          // first argument in the script's args
    u32 jump;         // IF and ELSE: the next ELSE or the IFEND; IFBLEND: its IFEND; LOOP and LOOPEND: the partner
    u32 line;
} YIns;

typedef struct YScript {
    char *name;       // source name from yst_list, in the game's encoding
    u8 *raw;
    YIns *ins;
    u32 count;
    YArg *args;
    u32 argCount;
    bool loaded;
} YScript;

typedef struct YCmdInfo {
    char *name;
    int id;           // YCmd
    int keyCount;
    char **keys;
} YCmdInfo;

typedef struct YLabel { char *name; u32 ip; u16 script; } YLabel;

// Commands the runtime knows by name. The table in ysc.ybn differs between engine versions.
typedef enum YCmd {
    C_UNKNOWN = 0,
    C_CDDA, C_CG, C_CGACT, C_CGEND, C_CGINFO, C_CLIPACT, C_CLIPINFO, C_CSV, C_CSVINFO, C_DEBUGLIST, C_DIALOG, C_ELSE,
    C_EMOTE, C_EMOTEINFO, C_END, C_ERROR, C_ERRORINFO, C_F_FLT, C_F_INT, C_F_STR, C_F_VAR, C_FILEACT, C_FILEINFO,
    C_FLASH, C_FLASHEND, C_FLASHINFO, C_FLT, C_FONT, C_FONTINFO, C_FPS, C_G_FLT, C_G_FLT2, C_G_FLT3, C_G_FLT4, C_G_INT,
    C_G_INT2, C_G_INT3, C_G_INT4, C_G_STR, C_G_STR2, C_G_STR3, C_G_STR4, C_G_VAR, C_GO, C_GOSUB, C_IF, C_IFBLEND,
    C_IFBREAK, C_IFCONTINUE, C_IFEND, C_INPUT, C_INT, C_LABEL, C_LABELINFO, C_LET, C_LOAD, C_LOOP, C_LOOPBREAK,
    C_LOOPCONTINUE, C_LOOPEND, C_MACRO, C_MATH, C_MATH3D, C_MENU, C_MENUACT, C_MENUEND, C_MENUINFO, C_MIDI, C_MIDIEND,
    C_MIDIINFO, C_MOUSE, C_MOVIE, C_MOVIEEND, C_MOVIEINFO, C_PLUG, C_PREP, C_REGINFO, C_RETURN, C_RETURNBREAK,
    C_RETURNCODE, C_S_FLT, C_S_INT, C_S_STR, C_S_VAR, C_SAVE, C_SAVEEND, C_SAVEINFO, C_SOUND, C_SOUNDEND, C_SOUNDINFO,
    C_STR, C_SYSTEM, C_SYSTEMINFO, C_TASK, C_TASKEND, C_TASKINFO, C_TEXT, C_TEXTINFO, C_TRAY, C_VAR, C_VARACT,
    C_VARINFO, C_WAIT, C_WINDOW, C_WINDOWEND, C_WINDOWINFO, C_WORD, C_YMV, C_YMVEND, C_YMVINFO, C_TEXTOUT, C__IFDEF,
    C__IFNDEF, C__IFEND, C_PRODUCT, C_SYSTEMMODE, C_COMPILEMODE, C_RELEASEMODE, C_PROJECTFOLDER,
    C_COUNT
} YCmd;
extern const char *const yr_cmd_names[C_COUNT];

bool yr_script_open(YR *y);
void yr_script_close(YR *y);
// Loads a script's instructions on first use.
YScript *yr_script(YR *y, u16 index);
const YLabel *yr_label(YR *y, const char *name);
// Keyword name of an argument.
const char *yr_arg_key(YR *y, const YIns *ins, const YArg *a);

// ---- values and variables (var.c) ----
enum { YT_NONE = 0, YT_INT = 1, YT_FLT = 2, YT_STR = 3 };

typedef struct YVal {
    u8 t;
    i64 i;
    double f;
    char *s;          // owned when t == YT_STR
} YVal;

void yv_free(YVal *v);
YVal yv_int(i64 i);
YVal yv_flt(double f);
YVal yv_str(const char *s);        // copies
YVal yv_str_own(char *s);          // takes
YVal yv_copy(const YVal *v);
i64 yv_as_int(const YVal *v);
double yv_as_flt(const YVal *v);
// The value as a string, malloc'd: numbers in decimal.
char *yv_as_str(const YVal *v);

typedef struct YVar {
    u8 t;
    u8 ndim;
    u8 scope;         // 1 global, 2 local or static, 3 file (newer engines)
    u8 declared;
    u16 script;
    u32 dims[8];
    u32 count;        // elements; 1 for a scalar
    i64 *ints;
    double *flts;
    char **strs;      // NULL entries are empty strings
    YVal init;        // initial value from ysv
    u8 *initExpr;     // string variables: the initial value as an expression, evaluated at start
    u32 initExprSize;
    u32 initDims[8];
    u8 initNdim;
} YVar;

bool yr_vars_open(YR *y, const u8 *ysv, size_t size, u32 version);
// Evaluates the string initial values, which are expressions that may read other variables.
void yr_vars_init_strings(YR *y);
void yr_vars_close(YR *y);
YVar *yr_var(YR *y, u32 id);
// (Re)declares a variable with its type, dims and initial value, as INT[], STR[] and the like do.
void yr_var_declare(YR *y, YVar *v, u8 t, u8 ndim, const u32 *dims, const YVal *init);
// Element index for the given subscripts, or -1 when out of range.
long yr_var_index(const YVar *v, int n, const i64 *sub);
YVal yr_var_get(const YVar *v, long index);
void yr_var_set(YVar *v, long index, const YVal *val);

// ---- sysvar.c: system variables (ids below 1000) ----
typedef enum YSys {
    S_NONE = 0,
    S_TIME, S_OS_TIME, S_OS_VERSION, S_OS_DXVERSION, S_OS_SOUND, S_OS_MEMTOTAL, S_OS_MEMAVAIL, S_MEDIA_READONLY, S_PAI,
    S_DEBUGMODE, S_ARGV, S_ARGC, S_LC, S_PINT, S_PFLT, S_PSTR, S_PFLG, S_RINT, S_RFLT, S_RSTR, S_RFLG, S_SYSTEM_PATH,
    S_SAVE_PATH, S_APPDATA_PATH, S_PERSONAL_PATH, S_PROJECT_PATH, S_SYSTEM_VERSION, S_SCREEN_COLOR, S_SCREEN_SIZE_X,
    S_SCREEN_SIZE_Y, S_WINDOW_EXEC_MAX, S_WINDOW_EXEC_CLOSE, S_WINDOW_EXEC_CAPTION, S_TEXT, S_TEXT_FLAG, S_TEXT_NUM,
    S_MENUCODE, S_MOUSE_X, S_MOUSE_Y, S_MOUSE_X2, S_MOUSE_Y2, S_PAD_NUMBER, S_INP, S_KEY_ANY, S_KEYLAST, S_MOUSELAST,
    S_CHR, S_DXMODE, S_TINT, S_TFLT, S_TSTR, S_VARTYPE,
    S_KEY,            // @_KEY_<name>: frames held, or released as a negative count
    S_KEYNUM,         // @_KEYNUM_<name>: presses
    S_CODE,           // @_CODE_<name>: button code for @_INP
    S_MOUSE,          // @_MOUSE_L, R, M, U, D
    S_MOUSENUM,
    S_CODE_MOUSE,
    S_PAD, S_PADNUM, S_CODE_PAD,
    S_OTHER
} YSys;

typedef struct YSysVar { YSys kind; int arg; } YSysVar;   // arg: button code for the key and mouse families

// Whether reads and writes go through sysvar.c; the rest live in ordinary variable storage.
bool yr_sys_dynamic(const YSysVar *s);
// Fills y->sys for the engine version from the tables built with the SDK compilers.
void yr_sysvars_init(YR *y, u32 version);
// Button codes, as @_CODE_* gives them and @_INP takes.
enum { YR_CODE_COUNT = 512 };
int yr_code_for_vk(int vk);
int yr_code_mouse(int button);    // 0 left, 1 right, 2 middle, 3 wheel up, 4 wheel down

#endif
