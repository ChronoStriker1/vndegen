// Slot state: the threads, objects and backlog a slot save restores. Filled in below.
#include "n2.h"

Buf slot_state_write(N2 *n) {
    Buf b = {0};
    buf_str(&b, "VNDEGEN-N2 1\n");
    return b;
}

bool slot_state_read(N2 *n, const u8 *state, size_t size, const u8 *vars, size_t vsize) { return false; }
