#include "age.h"

Script *script_load(Store *st, u32 id, char *err, size_t errSize) {
    size_t size = 0;
    u8 *d = store_read(st, id, &size);
    if (!d) { snprintf(err, errSize, "script %#x can't be read", id); return NULL; }
    if (size < 0x3c || memcmp(d, "SYS4", 4)) { free(d); snprintf(err, errSize, "resource %#x is not a script", id); return NULL; }
    Script *s = ag_alloc(sizeof *s);
    s->id = id;
    const CatEntry *e = store_entry(st, id);
    if (e) snprintf(s->name, sizeof s->name, "%s", e->name);
    for (int i = 0; i < 13; i++) s->hdr[i] = rd32(d + 8 + i * 4);
    s->count = (u32)((size - 0x3c) / 4);
    s->w = ag_alloc(sizeof(u32) * (s->count + 16));
    for (u32 i = 0; i < s->count; i++) s->w[i] = rd32(d + 0x3c + i * 4);
    s->codeEnd = s->hdr[8] < s->count ? s->hdr[8] : s->count;
    u32 t1 = s->hdr[8], n1 = s->hdr[7];
    if (t1 + n1 <= s->count) {
        s->t1 = s->w + t1;
        s->t1Count = n1;
    }
    free(d);
    return s;
}

void script_free(Script *s) {
    if (!s) return;
    free(s->w);
    free(s);
}

const char *script_string(const Script *s, u32 off) {
    static char buf[4][4096];
    static int which;
    char *o = buf[which = (which + 1) & 3];
    size_t n = 0;
    for (u32 i = off; i < s->count && n < sizeof buf[0] - 1; i++) {
        u32 v = s->w[i];
        for (int k = 0; k < 4; k++) {
            u8 c = (u8)~(v >> (k * 8));
            if (!c || n >= sizeof buf[0] - 1) { o[n] = 0; return o; }
            o[n++] = (char)c;
        }
    }
    o[n] = 0;
    return o;
}
