#include "ikura.h"

Script *script_load(IK *t, const char *name) {
    for (int i = 0; i < t->scriptCount; i++)
        if (ik_ieq(t->scripts[i]->name, name)) return t->scripts[i];
    size_t n;
    u8 *d = store_load(&t->store, AR_SNR, name, &n);
    if (!d || n < 16) {
        free(d);
        return NULL;
    }
    u32 labelBytes = rd32(d), codeSize = rd32(d + 4);
    if ((size_t)16 + labelBytes + codeSize > n) {
        free(d);
        return NULL;
    }
    Script *s = ik_alloc(sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name);
    s->data = d;
    s->size = n;
    s->labelCount = (int)(labelBytes / 4);
    s->labels = ik_alloc(sizeof(u32) * (size_t)(s->labelCount + 1));
    for (int i = 0; i < s->labelCount; i++) s->labels[i] = rd32(d + 16 + 4 * i);
    s->code = d + 16 + labelBytes;
    s->codeSize = codeSize;
    s->labelAt = ik_alloc(sizeof(i16) * (size_t)(codeSize + 1));
    for (u32 i = 0; i <= codeSize; i++) s->labelAt[i] = -1;
    for (int i = s->labelCount - 1; i >= 0; i--)
        if (s->labels[i] <= codeSize) s->labelAt[s->labels[i]] = (i16)i;
    s->readSlot = -1;
    for (u32 pc = 0; pc + 4 <= codeSize;) {
        u16 op = rd16(s->code + pc), len = rd16(s->code + pc + 2);
        if (len < 4 || pc + len > codeSize) break;
        if (op == 0x090a && len >= 8) {
            s->readSlot = (int)rd32(s->code + pc + 4);
            break;
        }
        pc += len;
    }
    t->scripts = ik_realloc(t->scripts, sizeof(Script *) * (size_t)(t->scriptCount + 1));
    t->scripts[t->scriptCount++] = s;
    return s;
}

void script_free(Script *s) {
    if (!s) return;
    free(s->labels);
    free(s->labelAt);
    free(s->data);
    free(s);
}
