#include "ipac.h"

#include <ctype.h>

#define MAX_STEPS 200000

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static const char *word(const char *p, char *out, size_t size) {
    p = skip_ws(p);
    size_t n = 0;
    while (*p && *p != ' ' && *p != '\t') {
        int k = ip_sjis_len((const u8 *)p);
        for (int i = 0; i < k && *p; i++) {
            if (n + 1 < size) out[n++] = *p;
            p++;
        }
    }
    out[n] = 0;
    return p;
}

static i32 *var_ref(IP *t, const char *s) {
    if ((s[0] == 'F' || s[0] == 'f' || s[0] == 'W' || s[0] == 'w') && isdigit((u8)s[1])) {
        int n = atoi(s + 1);
        if (n < 0 || n >= IP_VARS) return NULL;
        return s[0] == 'F' || s[0] == 'f' ? &t->vf[n] : &t->vw[n];
    }
    return NULL;
}

static i32 value(IP *t, const char *s) {
    i32 *v = var_ref(t, s);
    if (v) return *v;
    if (ip_ieq(s, "CLK")) return (i32)t->clk;
    if (ip_ieq(s, "RCLK")) {
        bool r = t->rclick;
        t->rclick = false;
        return r ? 1 : 0;
    }
    if (s[0] == 'x' || s[0] == 'X') return (i32)strtoul(s + 1, NULL, 16);
    return (i32)strtol(s, NULL, 10);
}

static void split_expr(const char *e, char *lhs, char *op, char *rhs) {
    e = skip_ws(e);
    size_t n = 0;
    while (*e && (isalnum((u8)*e) || *e == '_') && n < 31) lhs[n++] = *e++;
    lhs[n] = 0;
    e = skip_ws(e);
    n = 0;
    while (*e && strchr("=!<>+-*/%&|", *e) && n < 3) op[n++] = *e++;
    op[n] = 0;
    e = skip_ws(e);
    n = 0;
    while (*e && *e != ' ' && *e != '\t' && n < 31) rhs[n++] = *e++;
    rhs[n] = 0;
}

static bool cond(IP *t, const char *e) {
    char l[32], op[4], r[32];
    split_expr(e, l, op, r);
    i32 a = value(t, l), b = value(t, r);
    if (!strcmp(op, "=") || !strcmp(op, "==")) return a == b;
    if (!strcmp(op, "!") || !strcmp(op, "!=")) return a != b;
    if (!strcmp(op, "<")) return a < b;
    if (!strcmp(op, ">")) return a > b;
    if (!strcmp(op, "<=")) return a <= b;
    if (!strcmp(op, ">=")) return a >= b;
    if (!strcmp(op, "&")) return (a & b) != 0;
    ip_log(t, "unknown condition %s", e);
    return false;
}

static void set_var(IP *t, const char *e) {
    char l[32], op[4], r[32];
    split_expr(e, l, op, r);
    i32 *v = var_ref(t, l);
    if (!v) {
        ip_log(t, "SET on unknown variable %s", l);
        return;
    }
    i32 b = value(t, r), n = *v;
    switch (op[0]) {
    case '=': n = b; break;
    case '+': n += b; break;
    case '-': n -= b; break;
    case '*': n *= b; break;
    case '/': if (b) n /= b; break;
    case '%': if (b) n %= b; break;
    default: ip_log(t, "unknown SET %s", e); break;
    }
    *v = n & 0xff;
}

static void skip_block(IP *t) {
    if (t->pc < t->sc->count && t->sc->st[t->pc].kind == S_OPEN) t->pc = script_block_end(t->sc, t->pc) + 1;
    else t->pc++;
}

void vm_start(IP *t, const char *script, const char *label) {
    Script *s = script_load(t, script);
    if (!s) {
        char *u = ip_sjis_to_utf8(script);
        snprintf(t->problem, sizeof t->problem, "The script %s is missing.", u);
        ip_log(t, "%s", t->problem);
        free(u);
        t->wait = W_QUIT;
        return;
    }
    int pc = 0;
    if (label && *label) {
        pc = script_label(s, label);
        if (pc < 0) {
            char *u = ip_sjis_to_utf8(label);
            ip_log(t, "label %s not found", u);
            free(u);
            pc = 0;
        }
    }
    t->sc = s;
    t->pc = pc;
}

static void jump(IP *t, const char *target) {
    t->inGame = strncmp(target, "TITLE", 5) != 0;
    char script[128], label[128] = "";
    const char *colon = strchr(target, ':');
    if (colon) {
        snprintf(script, sizeof script, "%.*s", (int)(colon - target), target);
        snprintf(label, sizeof label, "%s", colon + 1);
    } else snprintf(script, sizeof script, "%s", target);
    vm_start(t, script, label);
}

static void goto_label(IP *t, const char *label) {
    char l[128];
    word(label, l, sizeof l);
    int pc = script_label(t->sc, l);
    if (pc < 0) {
        char *u = ip_sjis_to_utf8(l);
        ip_log(t, "label %s not found", u);
        free(u);
        return;
    }
    t->pc = pc;
}

static int args_int(const char *p, int *out, int max) {
    int n = 0;
    char w[64];
    while (n < max) {
        p = word(p, w, sizeof w);
        if (!w[0]) break;
        out[n++] = w[0] == 'x' || w[0] == 'X' ? (int)strtoul(w + 1, NULL, 16) : atoi(w);
    }
    return n;
}

static void obj_load(IP *t, const char *a) {
    char n[16], name[96];
    a = word(a, n, sizeof n);
    word(a, name, sizeof name);
    int i = atoi(n);
    if (i < 0 || i >= IP_OBJS) return;
    Obj *o = &t->obj[i];
    if (!strcmp(name, "!") || !name[0]) {
        memset(o, 0, sizeof *o);
        return;
    }
    o->img = image_get(t, PK_PARTS, name);
    snprintf(o->name, sizeof o->name, "%s", name);
    o->alpha = 255;
    o->frame = 0;
    o->animFrames = 0;
    size_t len;
    u8 *d = store_load(&t->store, PK_PARTS, name, ".IAN", &len);
    if (d && len >= 16 && !memcmp(d, "IANM", 4)) {
        int frames = (int)rd32(d + 4) + 1;
        if (frames > IP_ANIM) frames = IP_ANIM;
        for (int k = 0; k < frames && 16 + (size_t)k * 160 + 24 <= len; k++) {
            o->animX[k] = (i16)rd32(d + 16 + (size_t)k * 160 + 16);
            o->animY[k] = (i16)rd32(d + 16 + (size_t)k * 160 + 20);
            o->animFrames = k + 1;
        }
    }
    free(d);
}

static void config_add(IP *t, const char *a) {
    if (t->configCount >= 96) return;
    char menu[64], item[64];
    a = word(a, menu, sizeof menu);
    a = word(a, item, sizeof item);
    int v[6] = {0};
    args_int(a, v, 6);
    t->config[t->configCount++] = (ConfigItem){ip_strdup(menu), ip_strdup(item), v[0], v[1], v[2], v[3], v[4], v[5]};
}

static void cmd(IP *t, const char *s) {
    char name[32];
    const char *a = word(s, name, sizeof name);
    a = skip_ws(a);
    int v[8] = {0};
    if (!strcmp(name, "IF")) {
        if (!cond(t, a)) skip_block(t);
    } else if (!strcmp(name, "SET")) {
        set_var(t, a);
    } else if (!strcmp(name, "GOTO")) {
        goto_label(t, a);
    } else if (!strcmp(name, "GOTO_CLICK")) {
        if (!t->click) {
            goto_label(t, a);
            t->wait = W_FRAME;
        } else t->click = false;
    } else if (!strcmp(name, "WAIT")) {
        args_int(a, v, 1);
        t->wait = W_TIME;
        t->waitUntil = t->now + (u32)v[0] * 1000u / 60u;
        t->waitSkippable = true;
    } else if (!strcmp(name, "SPEED")) {
        args_int(a, v, 1);
        t->speed = v[0] > 0 ? v[0] : 4;
    } else if (!strcmp(name, "ALPHA")) {
        char r[96];
        word(a, r, sizeof r);
        if (!strcmp(r, "!")) t->ruleName[0] = 0;
        else snprintf(t->ruleName, sizeof t->ruleName, "%s", r);
    } else if (!strcmp(name, "CLICK")) {
        char r[96];
        word(a, r, sizeof r);
        if (!strcmp(r, "!")) {
            t->clickMap = NULL;
            t->clickName[0] = 0;
        } else {
            snprintf(t->clickName, sizeof t->clickName, "%s", r);
            t->clickMap = image_get(t, PK_BG, r);
        }
    } else if (!strcmp(name, "OBJ_LOAD")) {
        obj_load(t, a);
    } else if (!strcmp(name, "OBJ_POS")) {
        args_int(a, v, 4);
        if (v[0] >= 0 && v[0] < IP_OBJS) {
            t->obj[v[0]].x = v[1];
            t->obj[v[0]].y = v[2];
            t->obj[v[0]].alpha = clampi(v[3], 0, 255);
        }
    } else if (!strcmp(name, "OBJ_VIS")) {
        args_int(a, v, 2);
        if (v[0] >= 0 && v[0] < IP_OBJS) t->obj[v[0]].vis = v[1] != 0;
    } else if (!strcmp(name, "OBJ_MOVE")) {
        args_int(a, v, 4);
        if (v[0] >= 0 && v[0] < IP_OBJS) {
            t->obj[v[0]].mx = v[1];
            t->obj[v[0]].my = v[2];
            t->obj[v[0]].malpha = v[3];
        }
    } else if (!strcmp(name, "CLOSE")) {
        msg_clear(t);
        t->winOpen = false;
    } else if (!strcmp(name, "BGM")) {
        args_int(a, v, 1);
        if (v[0] == 255) snd_bgm_stop(t, 0);
        else snd_bgm(t, v[0], 0);
    } else if (!strcmp(name, "BGMFADE")) {
        args_int(a, v, 2);
        if (v[0] == 255) snd_bgm_stop(t, v[1]);
        else snd_bgm(t, v[0], v[1]);
    } else if (!strcmp(name, "SE")) {
        args_int(a, v, 1);
        if (v[0] == 255) snd_se_stop(t);
        else snd_se(t, v[0]);
    } else if (!strcmp(name, "MENU")) {
        args_int(a, v, 1);
        t->menuEnabled = v[0] != 0;
    } else if (!strcmp(name, "MODE")) {
        args_int(a, v, 2);
        if (v[0] == 1) ui_open(t, UI_LOAD);
        else if (v[0] == 0) ui_open(t, UI_SAVE);
        else if (v[0] == 3) ui_open(t, UI_CONFIG);
        else if (v[0] == 4) ui_open(t, UI_EXIT);
        else ip_log(t, "MODE %d", v[0]);
    } else if (!strcmp(name, "INITGAME")) {
        memset(t->vf, 0, sizeof t->vf);
    } else if (!strcmp(name, "INITRIREKI")) {
        for (int i = 0; i < t->logCount; i++) {
            free(t->logs[i].name);
            free(t->logs[i].text);
        }
        t->logCount = 0;
    } else if (!strcmp(name, "YURE")) {
        args_int(a, v, 2);
        t->shakeX = v[0];
        t->shakeY = v[1];
        t->shakeStart = t->now;
        t->shakeDur = 183;
    } else if (!strcmp(name, "SCENE_NAME")) {
        snprintf(t->sceneName, sizeof t->sceneName, "%s", a);
    } else if (!strcmp(name, "BG_POS")) {
        args_int(a, v, 2);
        t->bgX = v[0];
        t->bgY = v[1];
    } else if (!strcmp(name, "SIZE")) {
        args_int(a, v, 1);
        if (v[0] > 0) t->pitch = v[0];
    } else if (!strcmp(name, "GAMEINFO")) {
        char b[96];
        a = word(a, b, sizeof b);
        free(t->brand);
        t->brand = ip_strdup(b);
        a = skip_ws(a);
        if (*a) {
            free(t->title);
            t->title = ip_strdup(a);
        }
    } else if (!strcmp(name, "CONFIG")) {
        config_add(t, a);
    } else if (!strcmp(name, "VLIST")) {
        char p[16], sl[16], img[96];
        a = word(a, p, sizeof p);
        a = word(a, sl, sizeof sl);
        word(a, img, sizeof img);
        if (t->vlistCount < 256) t->vlist[t->vlistCount++] = (VListItem){atoi(p), atoi(sl), ip_strdup(img)};
    } else if (!strcmp(name, "LAYERMODE")) {
        args_int(a, v, 1);
        t->layerMode = v[0];
        if (!v[0]) {
            image_free(t->viewer);
            t->viewer = NULL;
        }
        t->dirty = true;
    } else if (!strcmp(name, "VCHECK")) {
        char pg[16], var[16];
        a = word(a, pg, sizeof pg);
        word(a, var, sizeof var);
        if (!cg_seen(t, atoi(pg), value(t, var))) skip_block(t);
    } else if (!strcmp(name, "SCHECK")) {
        char var[16];
        word(a, var, sizeof var);
        if (!scene_seen(t, value(t, var))) skip_block(t);
    } else if (!strcmp(name, "SCENE")) {
        args_int(a, v, 1);
        scene_mark(t, v[0]);
    } else if (!strcmp(name, "WORKBG")) {
        char nm[96];
        word(a, nm, sizeof nm);
        image_free(t->work);
        t->work = image_load(t, PK_BG, nm);
    } else if (!strcmp(name, "OBJ_ANIME")) {
        char n[16], var[16];
        a = word(a, n, sizeof n);
        word(a, var, sizeof var);
        int i = atoi(n);
        if (i >= 0 && i < IP_OBJS) t->obj[i].frame = value(t, var);
    } else if (!strcmp(name, "OBJ_DRAWW")) {
        args_int(a, v, 4);
        Obj *o = v[0] >= 0 && v[0] < IP_OBJS ? &t->obj[v[0]] : NULL;
        int fx, fy, fw, fh, dx, dy;
        if (o && o->img && t->work && frame_rect(o->img, o->animFrames, o->frame, &fx, &fy, &fw, &fh) && anim_pos(t, o, &dx, &dy))
            blit_part(t->work->px, t->work->w, t->work->h, o->img, fx, fy, fw, fh, dx + v[1], dy + v[2], clampi(v[3], 0, 255));
    } else if (!strcmp(name, "CHANGEW")) {
        if (t->work) {
            gfx_set_bg(t, "");
            image_free(t->workBg);
            t->workBg = t->work;
            t->work = NULL;
            t->bg = t->workBg;
            gfx_apply_now(t);
        }
    } else if (!strcmp(name, "BG_VISUAL")) {
        char pg[16], var[16];
        a = word(a, pg, sizeof pg);
        word(a, var, sizeof var);
        viewer_open(t, atoi(pg), value(t, var));
    } else if (!strcmp(name, "TITLE") || !strcmp(name, "DEBUGFILE") || !strcmp(name, "LABEL") || !strcmp(name, "PID") ||
               !strcmp(name, "VOICECHR") || !strcmp(name, "NAMECOLOR") || !strcmp(name, "SAVE_ID") || !strcmp(name, "URL") ||
               !strcmp(name, "MOVIE")) {
    } else {
        char *u = ip_sjis_to_utf8(s);
        ip_log(t, "%s:%d unhandled #%s", t->sc->name, t->sc->st[t->pc - 1].line, u);
        free(u);
    }
}

static int slot_of(char c) {
    switch (c) {
    case 'L': case 'K': return SP_L;
    case 'C': case 'X': return SP_C;
    case 'R': case 'E': return SP_R;
    default: return -1;
    }
}

static void sprite(IP *t, const char *s) {
    char cmd = s[0];
    const char *arg = skip_ws(s + 1);
    int p = slot_of(cmd);
    if (p < 0) {
        ip_log(t, "sprite command %c not handled", cmd);
        return;
    }
    if (strlen(arg) == 1 && slot_of(arg[0]) >= 0 && (arg[0] == 'L' || arg[0] == 'C' || arg[0] == 'R')) {
        if (gfx_move_sprite(t, slot_of(arg[0]), p)) t->wait = W_MOVE;
        return;
    }
    gfx_set_sprite(t, p, strcmp(arg, "!") ? arg : NULL);
    if (cmd == 'X' || cmd == 'K' || cmd == 'E') {
        gfx_apply_now(t);
        return;
    }
    t->wait = W_TRANS;
    gfx_begin_transition(t);
}

void vm_run(IP *t) {
    int steps = 0;
    while (t->wait == W_NONE && t->sc && !t->quit && t->uiMode == UI_NONE) {
        if (++steps > MAX_STEPS) {
            ip_log(t, "script %s ran %d steps without waiting", t->sc->name, MAX_STEPS);
            t->wait = W_FRAME;
            return;
        }
        if (t->pc >= t->sc->count) {
            if (ip_ieq(t->sc->name, "seenINIT")) {
                vm_start(t, "TITLE", NULL);
                continue;
            }
            ip_log(t, "script %s ended at %d of %d, last line %d", t->sc->name, t->pc, t->sc->count, t->sc->count ? t->sc->st[t->sc->count - 1].line : 0);
            t->wait = W_QUIT;
            t->quit = true;
            return;
        }
        Stmt *st = &t->sc->st[t->pc++];
        if (t->trace) {
            char *u = ip_sjis_to_utf8(st->s);
            ip_log(t, "%d:%d %s", st->line, st->kind, u);
            free(u);
        }
        if (t->pendingText && st->kind != S_TEXT) {
            t->pc--;
            msg_show(t);
            return;
        }
        switch (st->kind) {
        case S_BLANK:
        case S_OPEN:
        case S_CLOSE:
        case S_LABEL:
            break;
        case S_CMD:
            cmd(t, st->s);
            break;
        case S_CASE:
            if (atoi(st->s) != t->sel) skip_block(t);
            break;
        case S_JUMP:
            jump(t, st->s);
            break;
        case S_BG:
            cg_mark(t, st->s);
            msg_clear(t);
            gfx_set_bg(t, st->s);
            t->wait = W_TRANS;
            gfx_begin_transition(t);
            break;
        case S_SPRITE:
            sprite(t, st->s);
            break;
        case S_VOICE:
            snprintf(t->voice, sizeof t->voice, "%s", st->s);
            break;
        case S_NAME:
            if (!t->pendingText && !t->pendingName[0]) {
                t->pendOff = st->off;
                t->pendLine = st->line - 1;
            }
            snprintf(t->pendingName, sizeof t->pendingName, "%s", st->s);
            break;
        case S_TEXT:
            if (!t->pendingText && !t->pendingName[0]) {
                t->pendOff = st->off;
                t->pendLine = st->line - 1;
            }
            if (t->pendingText) {
                char *j = ip_fmt("%s\n%s", t->pendingText, st->s);
                free(t->pendingText);
                t->pendingText = j;
            } else t->pendingText = ip_strdup(st->s);
            break;
        case S_CHOICE:
            t->choiceCount = 0;
            while (t->pc < t->sc->count && t->sc->st[t->pc].kind == S_TEXT) {
                if (t->choiceCount < IP_CHOICES) t->choices[t->choiceCount++].text = t->sc->st[t->pc].s;
                t->pc++;
            }
            t->choiceHover = -1;
            t->wait = W_CHOICE;
            break;
        }
    }
}
