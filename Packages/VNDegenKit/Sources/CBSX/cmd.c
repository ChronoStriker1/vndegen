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

static const char *voice_key(const char *name, char *key) {
    snprintf(key, 3, "%s", name);
    return key;
}

void bx_cmd(BX *b, int op) {
    char key[4];
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
        Obj *bg = bx_obj_find(b, OK_BG, 0);
        Obj *o = bg && !strncmp(bg->name, s2, strlen(bg->name)) ? bg : bx_obj_by_id(b, a1);
        if (!o) o = bg;
        if (o) bx_obj_face(b, o, s2);
        else bx_log(b, "face %s for missing layer %d", s2, a1);
        break;
    }
    case 0x22: {
        Obj *o = bx_obj_by_id(b, a1);
        if (o) bx_obj_remove(b, o);
        break;
    }
    case 0x23:
        if (bx_arg(b, 3).str) bx_obj_keyframes(b, a1, s3);
        else if (!bx_obj_move(b, a1, a4, a5, a3 * 100)) bx_log(b, "move of missing picture %d", a1);
        break;
    case 0x24:
        b->nextBlend = a1;
        break;
    case 0x26:
        if (a1 == 0) b->winShown = b->winLayer = a2 != 0;
        else if (a1 == 1) b->menuLayer = a2 != 0;
        else if (a1 == 2) b->infoLayer = a2 != 0;
        break;
    case 0x27:
        bx_transition(b, a1, a2, NULL);
        if (b->transActive) b->th.wait = W_TRANS;
        break;
    case 0x28:
        bx_transition(b, a1, a2, s3);
        if (b->transActive) b->th.wait = W_TRANS;
        break;
    case 0x2a:
        bx_shake(b, a1, a2);
        break;
    case 0x2b:
        bx_flash(b, a1, a2, a3, a4);
        break;
    case 0x2c: {
        bool loop;
        const char *name = bgm_name(s2, &loop);
        b->bgmIndex = bx_arg(b, 2).v;
        if (strcmp(name, b->bgmName) || !bx_snd_playing(b, CH_BGM)) {
            snprintf(b->bgmName, sizeof b->bgmName, "%s", name);
            bx_snd_play(b, CH_BGM, RX_BGM, bx_bgm_track(b, name), loop, a1 * 100);
        }
        break;
    }
    case 0x2d:
        b->bgmName[0] = 0;
        bx_snd_stop(b, CH_BGM, a1 * 100);
        break;
    case 0x2e:
        bx_snd_loop(b, CH_BGS, s1, bx_res_find(&b->arc, RX_BGS, s1) ? RX_BGS : RX_SE, s1);
        break;
    case 0x2f:
        bx_snd_loop_stop(b, CH_BGS, s1, 0);
        break;
    case 0x30:
        if (s1 && bx_res_find(&b->arc, RX_VOICE, s1)) bx_snd_loop(b, CH_BGV, voice_key(s1, key), RX_VOICE, s1);
        break;
    case 0x31:
        bx_snd_loop_stop(b, CH_BGV, s1 ? voice_key(s1, key) : NULL, 0);
        break;
    case 0x32:
        bx_snd_play(b, a2 ? CH_SE2 : CH_SE, RX_SE, s1, false, 0);
        break;
    case 0x35:
        if (a1 == 0) wait_ms(b, (u32)a2 * 100);
        else if (a1 == 1) {
            if (!b->skipMode) b->th.wait = W_CLICK;
        } else if (a1 == 2) {
            if (bx_anim_busy(b)) b->th.wait = W_ANIM;
        }
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
