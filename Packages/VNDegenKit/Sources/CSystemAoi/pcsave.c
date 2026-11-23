#include "sa_private.h"

enum {
    IMAGE_SIZE = 1128076,
    VM_AT = 1119076,
    PC_C_VARS = 1000,
    PC_F_VARS = 5000,
    TOWN_AT = 1010852,
    CELL_SIZE = 20,
    STOCK_AT = 1004728,
    STOCK_ENTRIES = 256,
    MEMBER_AT = 86008,
    MEMBER_LEVEL = 12,
    MEMBER_EXP = 16,
    MEMBER_HP = 28,
    MEMBER_MP = 32,
    MEMBER_SP = 80,
    MEMBER_TRAINED = 84,
    MEMBER_EQUIP = 1212,
};

static int at(const u8 *img, size_t off) { return (int)rd32(img + off); }

bool sa_pcsave_read(SA *s, const char *path) {
    size_t n, size;
    u8 *file = sa_read_file(path, &n);
    u8 *img = file ? vnsystemaoi_unpack(file, n, &size) : NULL;
    free(file);
    if (!img || size != IMAGE_SIZE) {
        sa_log(s, "pc save %s: %s", path, img ? "unexpected size" : "unreadable");
        free(img);
        return false;
    }
    memset(s->vm.c, 0, sizeof s->vm.c);
    memset(s->vm.f, 0, sizeof s->vm.f);
    for (int i = 0; i < PC_C_VARS; i++) s->vm.c[i] = at(img, VM_AT + (size_t)i * 4);
    memcpy(s->vm.f, img + VM_AT + PC_C_VARS * 4, PC_F_VARS);
    for (int c = 0; c < TOWN_CELLS; c++) {
        size_t o = TOWN_AT + (size_t)c * CELL_SIZE;
        Facility *f = &s->town.cell[c];
        f->type = at(img, o);
        f->level = f->type ? at(img, o + 8) : 0;
        f->exp = f->type ? at(img, o + 12) : 0;
    }
    s->town.stage = s->vm.c[C_TOWN_STAGE];
    Party *p = &s->party;
    for (int i = 0; i < PARTY_SLOTS; i++) sa_party_place(p, &s->vfs, i, 0);
    p->benchCount = 0;
    int slot = PARTY_FRONT + 1;
    sa_party_place(p, &s->vfs, slot, at(img, MEMBER_AT));
    p->level[slot] = at(img, MEMBER_AT + MEMBER_LEVEL);
    p->exp[slot] = at(img, MEMBER_AT + MEMBER_EXP);
    p->hp[slot] = at(img, MEMBER_AT + MEMBER_HP);
    p->mp[slot] = at(img, MEMBER_AT + MEMBER_MP);
    p->sp[slot] = at(img, MEMBER_AT + MEMBER_SP);
    for (int k = 0; k < 10; k++) p->trained[slot][k] = at(img, MEMBER_AT + MEMBER_TRAINED + (size_t)k * 4);
    p->equip[slot] = at(img, MEMBER_AT + MEMBER_EQUIP);
    p->rosterCount = 1;
    p->roster[0] = p->unit[slot];
    p->items = 0;
    for (int i = 0; i < STOCK_ENTRIES; i++) {
        size_t o = STOCK_AT + (size_t)i * 12;
        int id = at(img, o), count = at(img, o + 4) + at(img, o + 8);
        if (!id) break;
        if (count > 0) sa_party_add_item(p, id, count);
    }
    free(img);
    sa_log(s, "pc save %s: turn %d, read", path, s->vm.c[C_TURN]);
    return true;
}
