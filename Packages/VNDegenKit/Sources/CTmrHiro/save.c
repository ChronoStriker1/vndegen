#include "th.h"

static char *slot_path(TH *t, int slot) { return th_fmt("%s/savedata.%03d", t->saveDir, slot); }

bool save_exists(TH *t, int slot) {
    char *p = slot_path(t, slot);
    char real[2048];
    bool ok = th_path_resolve(p, real, sizeof real) == 1;
    free(p);
    return ok;
}

bool save_slot(TH *t, int slot) {
    (void)t;
    (void)slot;
    return false;
}

bool load_slot(TH *t, int slot) {
    (void)t;
    (void)slot;
    return false;
}

void save_system(TH *t) { (void)t; }

void load_system(TH *t) { (void)t; }
