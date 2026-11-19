#include "vm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static YR *init(u32 version) {
    YR *y = yr_alloc(sizeof(YR));
    yr_sysvars_init(y, version);
    return y;
}

int main(void) {
    static const int aligned[][2] = {
        { 53, 53 }, { 54, 54 }, { 55, 55 }, { 56, 56 }, { 57, 60 }, { 58, 61 }, { 59, 62 },
        { 115, 122 }, { 116, 123 }, { 120, 127 }, { 140, 147 }, { 237, 244 }, { 238, 245 },
    };
    YR *v491 = init(491), *v481 = init(481);
    for (size_t i = 0; i < sizeof aligned / sizeof aligned[0]; i++) {
        const YSysVar *a = &v491->sys[aligned[i][0]], *b = &v481->sys[aligned[i][1]];
        if (a->kind != b->kind || a->arg != b->arg) {
            fprintf(stderr, "491 id %d is not 481 id %d\n", aligned[i][0], aligned[i][1]);
            return 1;
        }
        assert(b->kind != S_OTHER);
    }
    YR *v495 = init(495);
    assert(v495->sys[60].kind == v481->sys[60].kind && v495->sys[127].kind == S_CHR);
    puts("sysvar: ok");
    return 0;
}
