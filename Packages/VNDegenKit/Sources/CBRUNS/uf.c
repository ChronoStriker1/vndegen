#include "br.h"

static i32 A(BR *e, Node *call, int i) { return i + 2 < call->n ? vm_eval(e, call->k[i + 2]) : 0; }

static const char *S(BR *e, Node *call, int i) {
    if (i + 2 >= call->n) return "";
    Node *a = call->k[i + 2];
    if (a->t == N_STR) return a->s;
    return str_get(e, vm_eval(e, a));
}

static void trace_call(BR *e, int id, Node *call) {
    if (!e->trace) return;
    char buf[1024];
    int len = snprintf(buf, sizeof buf, "uf %d", id);
    for (int i = 2; i < call->n && len < (int)sizeof buf - 64; i++) {
        Node *a = call->k[i];
        if (a->t == N_STR) {
            char *u = br_sjis_to_utf8(a->s);
            len += snprintf(buf + len, sizeof buf - (size_t)len, " \"%s\"", u);
            free(u);
        } else len += snprintf(buf + len, sizeof buf - (size_t)len, " %d", vm_eval(e, a));
    }
    br_log(e, "%s", buf);
}

static void unknown(BR *e, int id) {
    if (id >= 0 && id < 10000 && !e->unknownSeen[id]) {
        e->unknownSeen[id] = 1;
        br_log(e, "unhandled user func %d", id);
    }
}

const char *str_get(BR *e, int id) {
    if (id < 0 || id >= BR_STRINGS || !e->str[id]) return "";
    return e->str[id];
}

void str_set(BR *e, int id, const char *s) {
    if (id < 0 || id >= BR_STRINGS) return;
    free(e->str[id]);
    e->str[id] = br_strdup(s ? s : "");
}

static void wait_ms(BR *e, int ms) {
    e->wait = W_TIME;
    e->waitUntil = e->now + (u32)(ms > 0 ? ms : 0);
}

static void obj_range(BR *e, int kind, int a, int b) {
    if (b < a) b = a;
    for (int i = a; i <= b; i++) obj_delete(e, kind, i);
}

static void event_wait(BR *e, int mask, int timeout, int flag) {
    e->wait = W_EVENT;
    e->eventMode = mask;
    e->eventTimeout = timeout;
    e->eventStart = e->now;
    e->eventFlag = flag;
    e->reflag = 0;
}


bool uf_call(BR *e, int id, Node *call) {
    trace_call(e, id, call);
    Obj *o;
    switch (id) {
    case 1000: layer_new(e, A(e, call, 0), A(e, call, 1)); return true;
    case 1006: {
        i32 v[31];
        int n = call->n - 2 < 31 ? call->n - 2 : 31;
        for (int i = 0; i < n; i++) v[i] = A(e, call, i);
        layers_set(e, v[2], v, n);
        return true;
    }
    case 1007: layers_transition(e, A(e, call, 2), A(e, call, 3)); return true;
    case 1008: {
        Node tmp = {.t = N_INT};
        Node *src = call->n > 4 && call->k[4]->t == N_STR ? call->k[4] : &tmp;
        if (src == &tmp) tmp.i = A(e, call, 2);
        e->reflag = layers_load(e, A(e, call, 0), A(e, call, 1), src, A(e, call, 3), A(e, call, 4));
        return true;
    }
    case 1014: layers_clear_pics(e, A(e, call, 1)); return true;
    case 1015: layers_freeze(e); return true;
    case 1016: layers_visible(e, A(e, call, 2), A(e, call, 3) != 0); return true;
    case 1020: return true;
    case 5200: {
        int n = A(e, call, 0);
        e->rng = e->rng * 1103515245u + 12345u;
        e->reflag = n > 0 ? (int)((e->rng >> 8) % (u32)n) : 0;
        return true;
    }
    case 3001: snd_stop(e, CH_MUSIC + A(e, call, 0), A(e, call, 1)); return true;
    case 3002: snd_volume(e, CH_MUSIC + A(e, call, 0), A(e, call, 1), A(e, call, 2)); return true;
    case 3003: snd_music(e, A(e, call, 0), A(e, call, 1), A(e, call, 2)); return true;
    case 3010: snd_se(e, A(e, call, 0), A(e, call, 1)); return true;
    case 3011: snd_stop(e, CH_SE + A(e, call, 0), 0); return true;
    case 3020: snd_voice(e, A(e, call, 0), A(e, call, 1)); return true;
    case 3021: snd_stop(e, CH_VOICE + A(e, call, 0), 0); return true;
    case 3023: {
        int ch = A(e, call, 0);
        if (ch >= 0 && ch < 10) e->voiceNum[ch] = A(e, call, 1);
        return true;
    }
    case 3024: {
        int ch = A(e, call, 0);
        if (ch >= 0 && ch < 10) {
            snd_voice(e, ch, e->voiceNum[ch]);
            if (!e->systemMode) history_voice_mark(e, ch, e->voiceNum[ch]);
        }
        return true;
    }
    case 2000: {
        int img = A(e, call, 0), ms = A(e, call, 6), mode = A(e, call, 7);
        bg_set(e, img, A(e, call, 5) ? ms : 0);
        if (mode == 2 && ms > 0) e->wait = W_FADE;
        return true;
    }
    case 2020: msg_select(e, A(e, call, 0)); return true;
    case 2021: msg_show(e, A(e, call, 0) != 0); return true;
    case 2022: msg_append(e, str_get(e, A(e, call, 0))); return true;
    case 2043:
    case 2044: menu_builtin(e, id, A(e, call, 0), A(e, call, 1)); return true;
    case 2080: e->reflag = 1; return true;
    case 2031: choice_start(e, call); return true;
    case 2050: obj_clear(e, OBJ_IMG); return true;
    case 2120: obj_clear(e, OBJ_BTN); return true;
    case 2053:
    case 2123:
        obj_create(e, id == 2053 ? OBJ_IMG : OBJ_BTN, A(e, call, 0), A(e, call, 1), A(e, call, 2), A(e, call, 3), A(e, call, 4), A(e, call, 5), A(e, call, 6), A(e, call, 7));
        return true;
    case 2054:
    case 2124: obj_range(e, id == 2054 ? OBJ_IMG : OBJ_BTN, A(e, call, 0), A(e, call, 1)); return true;
    case 2056:
    case 2126:
        if ((o = obj_get(e, id == 2056 ? OBJ_IMG : OBJ_BTN, A(e, call, 0)))) {
            o->x = A(e, call, 1);
            o->y = A(e, call, 2);
        }
        return true;
    case 2129:
        if ((o = obj_get(e, OBJ_BTN, A(e, call, 0)))) o->alpha = A(e, call, 1);
        return true;
    case 2060:
    case 2130:
        if ((o = obj_get(e, id == 2060 ? OBJ_IMG : OBJ_BTN, A(e, call, 0)))) o->disabled = A(e, call, 1) == 0;
        return true;
    case 2061:
    case 2131:
        if ((o = obj_get(e, id == 2061 ? OBJ_IMG : OBJ_BTN, A(e, call, 0)))) {
            if (A(e, call, 1) == 1) o->hoverEvent = A(e, call, 2);
            else o->event = A(e, call, 2);
        }
        return true;
    case 2063:
    case 2133:
        if ((o = obj_get(e, id == 2063 ? OBJ_IMG : OBJ_BTN, A(e, call, 0)))) {
            free(o->text);
            o->text = br_strdup(S(e, call, 1));
        }
        return true;
    case 2150: overlay_set(e, 0, 0, 0); return true;
    case 2151: {
        u32 rgb = (u32)A(e, call, 0) | (u32)A(e, call, 1) << 8 | (u32)A(e, call, 2) << 16;
        int ms = A(e, call, 4);
        overlay_set(e, rgb, A(e, call, 3), ms);
        if (A(e, call, 5) == 2 && ms > 0) e->wait = W_FADE;
        return true;
    }
    case 4000: event_wait(e, A(e, call, 0), A(e, call, 1), A(e, call, 2)); return true;
    case 4001: e->reflag = 1; return true;
    case 4110: {
        int f = A(e, call, 0);
        e->reflag = f >= 0 && f < BR_FLAGS ? e->flag[f] : 0;
        return true;
    }
    case 4111: {
        int f = A(e, call, 0);
        if (f >= 0 && f < BR_FLAGS && e->flag[f] != A(e, call, 1)) {
            e->flag[f] = A(e, call, 1);
            e->sysDirty = true;
        }
        return true;
    }
    case 5003: {
        char buf[2048] = "";
        size_t len = 0;
        for (int i = 1; i + 2 < call->n; i++) {
            Node *a = call->k[i + 2];
            const char *s = a->t == N_STR ? a->s : NULL;
            if (!s) {
                int v = vm_eval(e, a);
                if (v < 0) continue;
                s = str_get(e, v);
            }
            size_t l = strlen(s);
            if (len + l >= sizeof buf) break;
            memcpy(buf + len, s, l);
            len += l;
            buf[len] = 0;
        }
        str_set(e, A(e, call, 0), buf);
        return true;
    }
    case 5004: {
        char buf[32];
        int width = A(e, call, 2), pad = A(e, call, 3);
        if (width > 0 && pad) snprintf(buf, sizeof buf, "%0*d", width, A(e, call, 1));
        else if (width > 0) snprintf(buf, sizeof buf, "%*d", width, A(e, call, 1));
        else snprintf(buf, sizeof buf, "%d", A(e, call, 1));
        str_set(e, A(e, call, 0), buf);
        return true;
    }
    case 6000: e->quit = true; return true;
    case 6002: sys_game_start(e, A(e, call, 0), A(e, call, 1)); return true;
    case 6007: sys_title(e); return true;
    case 4200: sys_info(e, A(e, call, 0), A(e, call, 1), A(e, call, 2), A(e, call, 3)); return true;
    case 4201: e->reflag = save_slot(e, A(e, call, 0), S(e, call, 3)); return true;
    case 4202: e->reflag = sys_load_slot(e, A(e, call, 0)); return true;
    case 4203: e->reflag = slot_comment(e, A(e, call, 0), S(e, call, 1)); return true;
    case 4204: e->reflag = save_slot(e, QUICK_SLOT, ""); return true;
    case 4205: e->reflag = sys_load_slot(e, -1); return true;
    case 4300: return true;
    case 4316: e->reflag = history_height(e); return true;
    case 4322: e->reflag = history_layout(e, A(e, call, 0), A(e, call, 1), A(e, call, 2), A(e, call, 3), A(e, call, 4), A(e, call, 5), A(e, call, 6), A(e, call, 7)); return true;
    case 4312: {
        int ch;
        history_voice(e, A(e, call, 0), &ch);
        e->reflag = ch;
        return true;
    }
    case 4313: {
        int ch;
        e->reflag = history_voice(e, A(e, call, 0), &ch);
        return true;
    }
    case 4314: e->reflag = 100; return true;
    case 3022: return true;
    case 2127:
        if ((o = obj_get(e, OBJ_BTN, A(e, call, 0)))) {
            o->w = A(e, call, 1);
            o->h = A(e, call, 2);
        }
        return true;
    case 5005: sys_format_time(e, A(e, call, 0), A(e, call, 1)); return true;
    case 7001: e->reflag = 0; return true;
    case 7002: return true;
    default: unknown(e, id); return true;
    }
}

void uf_string(BR *e, Node *call) {
    if (e->trace) {
        Node *a = call->n > 1 ? call->k[1] : NULL;
        if (a && a->t == N_STR) {
            char *u = br_sjis_to_utf8(a->s);
            br_log(e, "text \"%s\"", u);
            free(u);
        } else if (a) br_log(e, "text %s", a->s ? a->s : "?");
    }
    msg_send(e, call);
}

static Obj *hit_button(BR *e) {
    static Obj *list[4096];
    int n = obj_sorted(e, list, 4096);
    for (int i = n - 1; i >= 0; i--) {
        Obj *o = list[i];
        if ((o->event < 0 && o->hoverEvent < 0) || o->disabled) continue;
        if (obj_hit(e, o, e->mouseX, e->mouseY)) return o;
    }
    return NULL;
}

static Obj *update_hover(BR *e, bool active) {
    Obj *h = active ? hit_button(e) : NULL;
    Obj *entered = NULL;
    for (int k = 0; k < OBJ_KINDS; k++)
        for (int i = 0; i < BR_OBJS; i++) {
            Obj *o = e->obj[k][i];
            if (!o) continue;
            bool hv = o == h;
            if (hv && !o->hover) {
                snd_se_hover(e);
                entered = o;
            }
            o->hover = hv;
            o->pressed = hv && e->lDown;
        }
    return entered;
}

void uf_tick(BR *e) {
    Obj *entered = update_hover(e, e->wait == W_EVENT && (e->eventMode & 4));
    switch (e->wait) {
    case W_TIME:
        if (e->now >= e->waitUntil) e->wait = W_NONE;
        break;
    case W_FADE:
        if (!overlay_busy(e) && !bg_busy(e) && !layers_busy(e)) e->wait = W_NONE;
        break;
    case W_EVENT: {
        if (e->eventMode & 4 && e->lPress) {
            Obj *o = hit_button(e);
            if (o && o->event >= 0) {
                e->reflag = o->event;
                e->wait = W_NONE;
                break;
            }
        }
        if (e->eventMode & 4 && entered && entered->hoverEvent >= 0) {
            e->reflag = entered->hoverEvent;
            e->wait = W_NONE;
            break;
        }
        if (e->eventMode & 0x4000 && e->keyEvent) {
            e->reflag = e->keyEvent;
            e->keyEvent = 0;
            e->wait = W_NONE;
            break;
        }
        if (e->eventMode & 0x2000 && (e->rPress || e->wheel)) {
            e->reflag = e->rPress ? 131072 + 33 : 131072 + ((e->wheel > 0 ? 255 : 1) << 8) + 8;
            e->rPress = false;
            e->wheel = 0;
            e->wait = W_NONE;
            break;
        }
        if (e->eventMode & 1 && (e->lPress || e->rPress)) {
            e->reflag = 0;
            e->wait = W_NONE;
            break;
        }
        if (e->eventTimeout >= 0 && e->now - e->eventStart >= (u32)e->eventTimeout) {
            e->reflag = 0;
            e->wait = W_NONE;
        }
        break;
    }
    case W_TEXT: msg_tick(e); break;
    case W_CHOICE: choice_tick(e); break;
    default: break;
    }
}
