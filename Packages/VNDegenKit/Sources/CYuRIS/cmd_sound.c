// Placeholder, replaced as the sound commands land.
#include "yr.h"
#include "vm.h"

int yr_cmd_sound(YCall *c, int id) {
    static bool seen[C_COUNT];
    if (id >= 0 && id < C_COUNT && !seen[id]) {
        seen[id] = true;
        yc_unknown(c, "command");
    }
    return YR_NEXT;
}
