#include "bsx.h"

static void wait_ms(BX *b, u32 ms) {
    if (!ms || b->skipMode) return;
    b->th.wait = W_TIME;
    b->th.until = b->now + ms;
}

static const char *bgm_name(const char *s, bool *loop) {
    *loop = true;
    if (*s == '*') {
        *loop = false;
        s++;
    }
    return s;
}

void bx_cmd(BX *b, int op) {
    const char *s1 = bx_arg_str(b, 1), *s2 = bx_arg_str(b, 2), *s3 = bx_arg_str(b, 3);
    i32 a1 = bx_arg_int(b, 1), a2 = bx_arg_int(b, 2), a3 = bx_arg_int(b, 3), a4 = bx_arg_int(b, 4), a5 = bx_arg_int(b, 5);
    if (b->trace) bx_log(b, "cmd %02x %d|%s %d|%s %d|%s %d %d", op, a1, s1, a2, s2, a3, s3, a4, a5);
    switch (op) {
    case 0x1e:
        if (bx_arg(b, 1).str) bx_obj_put(b, a2, 0, s1, 0, 0);
        else bx_log(b, "picture command without a name");
        break;
    case 0x1f:
        bx_obj_put(b, a2, 0, s1, a3, a4);
        break;
    case 0x21: {
        Obj *o = bx_obj_find(b, a1 == 1 ? OK_BG : OK_CHAR, a1 == 1 ? 0 : a1);
        if (o) bx_obj_face(b, o, s2);
        else bx_log(b, "face %s for missing layer %d", s2, a1);
        break;
    }
    case 0x22: {
        Obj *o = bx_obj_find(b, OK_CHAR, a1);
        if (o) bx_obj_remove(b, o);
        break;
    }
    case 0x23:
        if (bx_arg(b, 3).str) bx_log(b, "animation %s on %d not supported", s3, a1);
        else if (!bx_obj_move(b, OK_CHAR, a1, a4, a5, a3 * 100)) bx_log(b, "move of missing character %d", a1);
        break;
    case 0x24:
        bx_log(b, "cmd 24 %d %d %d", a1, a2, a3);
        break;
    case 0x26:
        if (a1 == 0) b->winShown = a2 != 0;
        else bx_log(b, "cmd 26 %d %d", a1, a2);
        break;
    case 0x27:
        bx_transition(b, a1, a2, NULL);
        if (b->transActive) b->th.wait = W_TRANS;
        break;
    case 0x28:
        bx_transition(b, a1, a2, s3);
        if (b->transActive) b->th.wait = W_TRANS;
        break;
    case 0x2b:
        bx_flash(b, a1, a2, a3, a4);
        break;
    case 0x2c: {
        bool loop;
        const char *name = bgm_name(s2, &loop);
        if (strcmp(name, b->bgmName) || !bx_snd_playing(b, CH_BGM)) {
            snprintf(b->bgmName, sizeof b->bgmName, "%s", name);
            bx_snd_play(b, CH_BGM, RX_BGM, name, loop, a1 * 100);
        }
        break;
    }
    case 0x2d:
        b->bgmName[0] = 0;
        bx_snd_stop(b, CH_BGM, a1 * 100);
        break;
    case 0x2e:
        bx_snd_play(b, CH_BGS, RX_BGS, s1, true, 0);
        break;
    case 0x2f:
        bx_snd_stop(b, CH_BGS, 0);
        break;
    case 0x31:
        bx_snd_stop(b, CH_SE, 0);
        bx_snd_stop(b, CH_SE2, 0);
        break;
    case 0x32:
        bx_snd_play(b, a2 ? CH_SE2 : CH_SE, RX_SE, s1, false, 0);
        break;
    case 0x35:
        if (a1 == 0) wait_ms(b, (u32)a2 * 100);
        else if (a1 == 1 && !b->skipMode) b->th.wait = W_CLICK;
        else bx_log(b, "wait %d %d", a1, a2);
        break;
    case 0x37:
        bx_system(b, s1);
        break;
    default:
        bx_log(b, "command %02x not supported (%d|%s %d|%s %d %d %d)", op, a1, s1, a2, s2, a3, a4, a5);
        break;
    }
}
