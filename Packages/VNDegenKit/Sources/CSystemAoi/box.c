#include "sa.h"

u8 sa_box_key(u32 o) {
    const u32 c = o - 0x5CC8E9D7u;
    u32 v1 = c + (0xA3371629u >> ((o & 0xF) + 1)) - (0x5CC8E9D7u << (31 - (o & 0xF)));
    u32 t = c + (v1 << (31 - ((o >> 4) & 0xF))) + (v1 >> (((o >> 4) & 0xF) + 1));
    u32 v5 = c + (t << (31 - ((o >> 8) & 0xF))) + (t >> (((o >> 8) & 0xF) + 1));
    u32 v6 = c + (v5 << (31 - ((o >> 12) & 0xF))) + (v5 >> (((o >> 12) & 0xF) + 1));
    u32 v7 = c + (v6 << (31 - ((o >> 16) & 0xF))) + (v6 >> (((o >> 16) & 0xF) + 1));
    u32 v8 = (o >> 20) & 0xF;
    t = c + (v7 << (31 - v8)) + (v7 >> (v8 + 1));
    u32 v9 = c + (t << (31 - ((o >> 24) & 0xF))) + (t >> (((o >> 24) & 0xF) + 1));
    return (u8)((c + (v9 << (31 - (o >> 28))) + (v9 >> ((o >> 28) + 1))) >> (o & 0xF));
}

bool sa_box_parse(Box *b, const u8 *data, size_t size) {
    static const u8 magic[16] = {'A', 0, 'O', 0, 'I', 0, 'M', 0, 'Y', 0, '0', 0, '1', 0, 0, 0};
    memset(b, 0, sizeof *b);
    if (size < 0x18 || memcmp(data, magic, 16)) return false;
    u32 n = rd32be(data + 0x10);
    if (n > 4096 || 0x18 + (size_t)n * 0x28 > size) return false;
    b->e = sa_alloc(sizeof(BoxEntry) * (n ? n : 1));
    for (u32 i = 0; i < n; i++) {
        const u8 *r = data + 0x18 + (size_t)i * 0x28;
        u32 off = rd32be(r + 0x20), len = rd32be(r + 0x24);
        if ((size_t)off + len > size) continue;
        BoxEntry *e = &b->e[b->count++];
        char *nm = sa_utf16_to_utf8(r, 16);
        snprintf(e->name, sizeof e->name, "%s", nm);
        free(nm);
        e->size = len;
        e->data = sa_alloc((size_t)len + 1);
        for (u32 k = 0; k < len; k++) e->data[k] = data[off + k] ^ sa_box_key(off + k);
    }
    return true;
}

void sa_box_free(Box *b) {
    for (int i = 0; i < b->count; i++) free(b->e[i].data);
    free(b->e);
    memset(b, 0, sizeof *b);
}
