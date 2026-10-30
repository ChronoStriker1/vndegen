// Save files in the PC engine's format, so saves move between VNDegen and Windows both ways.
//
// The layout was read from saves System39.exe wrote under Wine for test scenarios built with AliceSoft's SDK, one
// setting changed at a time (docs/system3-cleanroom.md). QD n writes <prefix>S<letter>.ASD, letter A for slot 1.
//   0x000  "System3.8 SaveData(c)ALICE-SOFT\0"
//   0x020  00 02 35 00, then zeros
//   0x040  u8 choice font size, choice colour, choice background, choice frame,
//          message font size, message colour, message background, message frame
//   0x050  u32 page + 1, u32 address after the QD command: where LD resumes
//   0x060  128 choice windows, 0x460 128 message windows: u16 x, y, width, height
//   0x860  u32 section offsets: [0] call stack, [1] strings, [4 + p] variable page p (page 0 the normal variables)
//   stack      u32 size, 12 zero bytes, entries bottom first, each ending in a tag byte:
//              label call u32 address ee; page call u16 page + 1, u32 address ff; pushed variable u32 index, u16 value dd
//   strings    u32 size, u32 count, 8 zero bytes, count NUL-terminated strings from string 0
//   page p     u32 size in bytes, u32 p, 8 zero bytes, the values: 65537 for page 0, and each array page DC created
//              with its save flag set
//   tail       256 bytes of ff, n zero bytes, u32 n, "INFS"; n is 1 per label call and 2 per page call on the stack
#include "vm.h"
#include <sys/stat.h>
#include <time.h>

static const char magic[32] = "System3.8 SaveData(c)ALICE-SOFT";

typedef struct Buf { u8 *d; size_t n, cap; } Buf;
static void put(Buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->d = realloc(b->d, b->cap); if (!b->d) abort(); }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}
static void put8(Buf *b, u8 v) { put(b, &v, 1); }
static void put16(Buf *b, u16 v) { u8 x[2] = { (u8)v, (u8)(v >> 8) }; put(b, x, 2); }
static void put32(Buf *b, u32 v) { u8 x[4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) }; put(b, x, 4); }
static void zeros(Buf *b, size_t n) { while (n--) put8(b, 0); }
static void set32(Buf *b, size_t at, u32 v) { b->d[at] = (u8)v; b->d[at + 1] = (u8)(v >> 8); b->d[at + 2] = (u8)(v >> 16); b->d[at + 3] = (u8)(v >> 24); }

static char *slot_path(S3 *s, int slot) {
    if (slot < 1 || slot > 26) return NULL;
    char name[96];
    snprintf(name, sizeof name, "%sS%c.ASD", s->prefix, 'A' + slot - 1);
    char *found = s3_find_file(s->saveDir, name);
    return found ? found : s3_path(s->saveDir, name);
}

static Buf build(S3 *s) {
    Buf b = {0};
    put(&b, magic, 32);
    put8(&b, 0); put8(&b, 2); put8(&b, 0x35); put8(&b, 0);
    zeros(&b, 0x40 - b.n);
    u8 fonts[8] = { (u8)s->menuFontSize, (u8)s->menuColor, (u8)s->menuBg, (u8)s->menuFrame,
                    (u8)s->fontSize, (u8)s->msgColor, (u8)s->msgBg, (u8)s->msgFrame };
    put(&b, fonts, 8);
    zeros(&b, 0x50 - b.n);
    put32(&b, (u32)s->page + 1);
    put32(&b, s->pc);
    zeros(&b, 0x60 - b.n);
    for (int i = 0; i < 128; i++) { S3Win w = s->selWin[i]; put16(&b, (u16)w.x); put16(&b, (u16)w.y); put16(&b, (u16)w.w); put16(&b, (u16)w.h); }
    for (int i = 0; i < 128; i++) { S3Win w = s->msgWin[i]; put16(&b, (u16)w.x); put16(&b, (u16)w.y); put16(&b, (u16)w.w); put16(&b, (u16)w.h); }
    size_t table = b.n;
    zeros(&b, 0x1000 - b.n);

    // Call stack.
    set32(&b, table, (u32)b.n);
    size_t stackAt = b.n;
    zeros(&b, 16);
    u32 tailCount = 0;
    for (int i = 0; i < s->sp; i++) {
        S3Frame f = s->stack[i];
        if (f.kind == S3_FRAME_LABEL) { put32(&b, f.pc); put8(&b, 0xee); tailCount += 1; }
        else if (f.kind == S3_FRAME_PAGE) { put16(&b, (u16)(f.page + 1)); put32(&b, f.pc); put8(&b, 0xff); tailCount += 2; }
        else if (f.kind == S3_FRAME_PUSH) { put32(&b, f.var); put16(&b, f.value); put8(&b, 0xdd); }
    }
    set32(&b, stackAt, (u32)(b.n - stackAt - 16));

    // Strings 0 to strCount.
    set32(&b, table + 4, (u32)b.n);
    size_t strAt = b.n;
    int count = s->strCount + 1;
    put32(&b, 0);
    put32(&b, (u32)count);
    zeros(&b, 8);
    for (int i = 0; i < count; i++) {
        const char *t = i < S3_MAX_STRINGS && s->str[i] ? s->str[i] : "";
        put(&b, t, strlen(t) + 1);
    }
    set32(&b, strAt, (u32)(b.n - strAt - 16));

    // Variable pages: the normal variables, then the arrays DC made with their save flag set.
    set32(&b, table + 16, (u32)b.n);
    put32(&b, 65537 * 2);
    put32(&b, 0);
    zeros(&b, 8);
    for (u32 i = 0; i < 65536; i++) put16(&b, s->vars[i]);
    put16(&b, 0);
    for (int p = 1; p < 256; p++) {
        S3ArrayPage *a = &s->arr[p];
        if (!a->v || !a->save) continue;
        set32(&b, table + 16 + (size_t)p * 4, (u32)b.n);
        put32(&b, a->n * 2);
        put32(&b, (u32)p);
        zeros(&b, 8);
        for (u32 i = 0; i < a->n; i++) put16(&b, a->v[i]);
    }

    for (int i = 0; i < 256; i++) put8(&b, 0xff);
    zeros(&b, tailCount);
    put32(&b, tailCount);
    put(&b, "INFS", 4);
    return b;
}

bool s3_save_write(S3 *s, int slot) {
    char *path = slot_path(s, slot);
    if (!path) return false;
    Buf b = build(s);
    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(b.d, 1, b.n, f) == b.n;
    if (f && fclose(f) != 0) ok = false;
    free(b.d);
    s3_log(s, "saved slot %d: %s", slot, ok ? path : "failed");
    free(path);
    return ok;
}

typedef struct Save {
    u8 *d;
    size_t n;
    u32 stack, strings, vars;
} Save;

static bool read_save(S3 *s, int slot, Save *sv) {
    memset(sv, 0, sizeof *sv);
    char *path = slot_path(s, slot);
    if (!path) return false;
    sv->d = s3_read_file(path, &sv->n);
    free(path);
    if (!sv->d || sv->n < 0x1000 || memcmp(sv->d, magic, 16) != 0) { free(sv->d); sv->d = NULL; return false; }
    sv->stack = s3_le32(sv->d + 0x860);
    sv->strings = s3_le32(sv->d + 0x864);
    sv->vars = s3_le32(sv->d + 0x870);
    if (sv->vars + 16 + 131072 > sv->n || sv->strings + 16 > sv->n || sv->stack + 16 > sv->n) { free(sv->d); sv->d = NULL; return false; }
    return true;
}

bool s3_save_load(S3 *s, int slot) {
    Save sv;
    if (!read_save(s, slot, &sv)) return false;
    const u8 *d = sv.d;
    s->menuFontSize = d[0x40]; s->menuColor = d[0x41]; s->menuBg = d[0x42]; s->menuFrame = d[0x43];
    s->fontSize = d[0x44]; s->msgColor = d[0x45]; s->msgBg = d[0x46]; s->msgFrame = d[0x47];
    for (int i = 0; i < 128; i++) {
        const u8 *w = d + 0x60 + i * 8, *m = d + 0x460 + i * 8;
        s->selWin[i] = (S3Win){ s3_le16(w), s3_le16(w + 2), s3_le16(w + 4), s3_le16(w + 6), s->selWin[i].save };
        s->msgWin[i] = (S3Win){ s3_le16(m), s3_le16(m + 2), s3_le16(m + 4), s3_le16(m + 6), s->msgWin[i].save };
    }
    for (u32 i = 0; i < 65536; i++) s->vars[i] = s3_le16(d + sv.vars + 16 + i * 2);

    u32 count = s3_le32(d + sv.strings + 4);
    size_t p = sv.strings + 16;
    if (count > 0 && count <= S3_MAX_STRINGS) s->strCount = (int)count - 1;
    for (u32 i = 0; i < count && p < sv.n; i++) {
        const u8 *e = memchr(d + p, 0, sv.n - p);
        if (!e) break;
        if (i < S3_MAX_STRINGS) {
            free(s->str[i]);
            s->str[i] = NULL;
            if (e > d + p) {
                s->str[i] = s3_alloc((size_t)s->strLen + 1);
                snprintf(s->str[i], (size_t)s->strLen + 1, "%s", (const char *)d + p);
            }
        }
        p = (size_t)(e - d) + 1;
    }

    for (int page = 1; page < 256; page++) {
        u32 a = s3_le32(d + 0x870 + (size_t)page * 4);
        if (!a || a + 16 > sv.n) continue;
        u32 size = s3_le32(d + a);
        if (s3_le32(d + a + 4) != (u32)page || size % 2 || a + 16 + size > sv.n) continue;
        S3ArrayPage *ap = &s->arr[page];
        u32 n = size / 2;
        if (!ap->v || ap->n != n) { free(ap->v); ap->v = s3_alloc((size_t)n * 2 + 2); ap->n = n; }
        ap->save = true;
        for (u32 i = 0; i < n; i++) ap->v[i] = s3_le16(d + a + 16 + i * 2);
    }

    // The call stack, then resume where the save was made.
    s->sp = 0;
    u32 stackSize = s3_le32(d + sv.stack);
    const u8 *q = d + sv.stack + 16, *end = q + stackSize;
    while (q < end && s->sp < S3_MAX_FRAMES) {
        // Entries end in their tag, so read the tag first by the entry's size.
        if (q + 5 <= end && q[4] == 0xee) { s->stack[s->sp++] = (S3Frame){ .kind = S3_FRAME_LABEL, .pc = s3_le32(q) }; q += 5; }
        else if (q + 7 <= end && q[6] == 0xff) { s->stack[s->sp++] = (S3Frame){ .kind = S3_FRAME_PAGE, .page = (u16)(s3_le16(q) - 1), .pc = s3_le32(q + 2) }; q += 7; }
        else if (q + 7 <= end && q[6] == 0xdd) { s->stack[s->sp++] = (S3Frame){ .kind = S3_FRAME_PUSH, .var = (u16)s3_le32(q), .value = s3_le16(q + 4) }; q += 7; }
        else break;
    }
    u32 page = s3_le32(d + 0x50), pc = s3_le32(d + 0x54);
    free(sv.d);
    s->menuCount = 0;
    s->menuBuild = -1;
    if (page < 1 || !s3_goto_page(s, (int)page - 1, pc)) return false;
    s3_var_set(s, 0, 0);
    s3_log(s, "loaded slot %d: page %u at 0x%x", slot, page - 1, pc);
    return true;
}

bool s3_save_copy(S3 *s, int to, int from) {
    char *src = slot_path(s, from), *dst = slot_path(s, to);
    size_t n;
    u8 *d = src ? s3_read_file(src, &n) : NULL;
    bool ok = false;
    if (d && dst) {
        FILE *f = fopen(dst, "wb");
        ok = f && fwrite(d, 1, n, f) == n;
        if (f && fclose(f) != 0) ok = false;
    }
    free(d); free(src); free(dst);
    return ok;
}

// QP: writes count variables from `from` into an existing save, in place.
bool s3_save_write_part(S3 *s, int slot, const S3LV *from, int count) {
    Save sv;
    if (!read_save(s, slot, &sv)) return false;
    u32 base = from->var;
    for (int i = 0; i < count && base + (u32)i < 65536; i++) {
        u16 v = s3_lv_get(s, from, i);
        size_t at = sv.vars + 16 + (size_t)(base + (u32)i) * 2;
        sv.d[at] = (u8)v;
        sv.d[at + 1] = (u8)(v >> 8);
    }
    char *path = slot_path(s, slot);
    FILE *f = path ? fopen(path, "wb") : NULL;
    bool ok = f && fwrite(sv.d, 1, sv.n, f) == sv.n;
    if (f && fclose(f) != 0) ok = false;
    free(path);
    free(sv.d);
    return ok;
}

// LP: reads count variables from a save into `to`.
bool s3_save_load_part(S3 *s, int slot, const S3LV *to, int count) {
    Save sv;
    if (!read_save(s, slot, &sv)) return false;
    u32 base = to->var;
    for (int i = 0; i < count && base + (u32)i < 65536; i++)
        s3_lv_set(s, to, i, s3_le16(sv.d + sv.vars + 16 + (size_t)(base + (u32)i) * 2));
    free(sv.d);
    return true;
}

// LT: the save's modification time as year, month, day, hour, minute, second, weekday.
bool s3_save_time(S3 *s, int slot, u16 out[7]) {
    char *path = slot_path(s, slot);
    struct stat st;
    bool ok = path && stat(path, &st) == 0;
    free(path);
    if (!ok) return false;
    struct tm tm;
    time_t t = st.st_mtime;
    localtime_r(&t, &tm);
    u16 v[7] = { (u16)(tm.tm_year + 1900), (u16)(tm.tm_mon + 1), (u16)tm.tm_mday, (u16)tm.tm_hour, (u16)tm.tm_min, (u16)tm.tm_sec, (u16)tm.tm_wday };
    memcpy(out, v, sizeof v);
    return true;
}
