#include "br.h"

#include <time.h>


void sys_call(BR *e, int script) {
    if (e->systemMode) return;
    sx_free(e->gameSnap);
    e->gameSnap = state_capture(e, false);
    e->savedWait = e->wait;
    e->systemMode = true;
    e->wait = W_NONE;
    e->menu.open = false;
    snd_stop_voices(e);
    vm_start(e, script);
}

static void game_reset_display(BR *e) {
    layers_reset(e);
    for (int k = 0; k < OBJ_KINDS; k++) obj_clear(e, k);
    bg_set(e, -1, 0);
    msg_select(e, 0);
    msg_show(e, false);
}

void sys_return(BR *e) {
    if (e->nextScript == -1 && e->loadedState) {
        e->nextScript = -2;
        state_apply(e, e->loadedState, true);
        sx_free(e->loadedState);
        e->loadedState = NULL;
        sx_free(e->gameSnap);
        e->gameSnap = NULL;
        e->systemMode = false;
        return;
    }
    if (e->nextScript >= 0) {
        int n = e->nextScript;
        e->nextScript = -2;
        sx_free(e->gameSnap);
        e->gameSnap = NULL;
        e->systemMode = e->nextSystem;
        if (!e->systemMode) game_reset_display(e);
        vm_start(e, n);
        return;
    }
    if (e->gameSnap) {
        state_apply(e, e->gameSnap, false);
        sx_free(e->gameSnap);
        e->gameSnap = NULL;
        e->systemMode = false;
        return;
    }
    e->ended = true;
}

void sys_game_start(BR *e, int script, int sub) {
    e->nextScript = script;
    e->nextSystem = false;
    e->sp = 0;
}

static void info_words(BR *e, int slot, int wA, int wB, int sC) {
    Node *si = slot_info(e, slot);
    *vm_word(e, wA) = si ? sx_int(sx_cdr_value(sx_find(si, "sceneno")), 0) : 0;
    *vm_word(e, wB) = si ? sx_int(sx_cdr_value(sx_find(si, "savetime")), 0) : 0;
    const char *c = si ? sx_str(sx_cdr_value(sx_find(si, "comment"))) : "";
    str_set(e, sC, c ? c : "");
    e->reflag = si != NULL;
}

static bool load_slot(BR *e, int slot) {
    Node *st = slot_state(e, slot);
    if (!st) return false;
    sx_free(e->loadedState);
    e->loadedState = sx_copy(st);
    slot_state_release(st);
    return true;
}

bool sys_info(BR *e, int slot, int wA, int wB, int sC) {
    info_words(e, slot, wA, wB, sC);
    return e->reflag;
}

bool sys_load_slot(BR *e, int slot) { return load_slot(e, slot < 0 ? QUICK_SLOT : slot); }

void sys_format_time(BR *e, int dst, i32 when) {
    time_t t = (time_t)(u32)when;
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[64];
    snprintf(buf, sizeof buf, "%02d/%02d/%02d:%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    str_set(e, dst, buf);
}

void sys_title(BR *e) {
    sx_free(e->gameSnap);
    e->gameSnap = NULL;
    game_reset_display(e);
    e->nextScript = e->cfg.titleScript;
    e->nextSystem = true;
    e->sp = 0;
}
