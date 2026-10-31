// The script machine: tasks, control flow and the per-frame schedule.
//
// From the YU-RIS manual's task page: each task runs its script until a wait or displayed text, then rests until the
// next frame; tasks run one at a time in order of priority (Z, highest first; the system task starts at 100), and the
// screen is drawn once all of them rest.
#include "yr.h"
#include "vm.h"

enum { BUDGET = 4000000 };   // instructions one task may run in a frame before it is made to rest

void yr_fail(YR *y, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(y->problem, sizeof y->problem, fmt, ap);
    va_end(ap);
    yr_log(y, "problem: %s\n", y->problem);
}

static void values_free(YValues *v) {
    for (int i = 0; i < YR_PARAMS; i++) { free(v->s[i]); v->s[i] = NULL; }
}

static YFrame *push_frame(YTask *t) {
    if (t->depth == t->frameCap) {
        t->frameCap = t->frameCap ? t->frameCap * 2 : 16;
        t->frames = yr_realloc(t->frames, sizeof(YFrame) * (size_t)t->frameCap);
    }
    YFrame *f = &t->frames[t->depth++];
    memset(f, 0, sizeof *f);
    f->loopBase = t->loopDepth;
    return f;
}

YTask *yr_task_new(YR *y, const char *id, int z, u16 script, u32 ip) {
    YTask *t = yr_alloc(sizeof(YTask));
    t->id = yr_strdup(id ? id : "");
    t->z = z;
    t->alive = true;
    t->script = script;
    t->ip = ip;
    t->order = y->taskOrder++;
    t->enabled = true;
    push_frame(t);
    y->tasks = yr_realloc(y->tasks, sizeof(YTask *) * (size_t)(y->taskCount + 1));
    y->tasks[y->taskCount++] = t;
    return t;
}

void yr_task_free(YTask *t) {
    for (int i = 0; i < t->depth; i++) { values_free(&t->frames[i].params); values_free(&t->frames[i].results); }
    values_free(&t->tvals);
    free(t->frames);
    free(t->loops);
    free(t->id);
    free(t);
}

YTask *yr_task_find(YR *y, const char *id) {
    for (int i = 0; i < y->taskCount; i++)
        if (y->tasks[i]->alive && !strcasecmp(y->tasks[i]->id, id)) return y->tasks[i];
    return NULL;
}

static bool jump_label(YR *y, YTask *t, const char *name) {
    const YLabel *l = yr_label(y, name);
    if (!l || !yr_script(y, l->script)) {
        yr_log(y, "label not found: %s\n", name);
        return false;
    }
    t->script = l->script;
    t->ip = l->ip;
    return true;
}

bool yr_gosub(YR *y, YTask *t, const char *label, YScript *from, u32 retIp) {
    (void)from;
    u16 script = t->script;
    if (!yr_label(y, label)) { yr_log(y, "GOSUB: label not found: %s\n", label); return false; }
    YFrame *f = push_frame(t);
    f->retScript = script;
    f->retIp = retIp;
    return jump_label(y, t, label);
}

// ---- control flow ----

static bool cond_true(YCall *c, const YIns *in) {
    if (!in->argc) return true;
    const YArg *a = &c->s->args[in->arg];
    if (!a->size) return true;
    YVal v = yr_eval(c->y, c->t, a->data, a->size);
    bool r = v.t == YT_STR ? (v.s && *v.s) : v.t == YT_FLT ? v.f != 0 : v.i != 0;
    yv_free(&v);
    return r;
}

// From a failed IF or ELSE condition: the next ELSE whose condition holds, or past the IFEND.
static void branch_on(YCall *c, u32 from) {
    YScript *s = c->s;
    YR *y = c->y;
    u32 ip = from;
    for (int guard = 0; guard < 100000; guard++) {
        u32 next = s->ins[ip].jump;
        if (!next || next >= s->count) { c->t->ip = from + 1; return; }
        int id = y->cmds[s->ins[next].cmd].id;
        if (id == C_ELSE) {
            c->in = &s->ins[next];
            if (cond_true(c, &s->ins[next])) { c->t->ip = next + 1; return; }
            ip = next;
            continue;
        }
        c->t->ip = next + 1;   // the IFEND
        return;
    }
}

// The IFEND that closes the chain an ELSE or IFBLEND belongs to.
static u32 chain_end(YR *y, YScript *s, u32 ip) {
    for (int guard = 0; guard < 100000; guard++) {
        u32 next = s->ins[ip].jump;
        if (!next || next >= s->count) return ip;
        if (y->cmds[s->ins[next].cmd].id == C_IFEND) return next;
        ip = next;
    }
    return ip;
}

static int do_return(YCall *c) {
    YR *y = c->y;
    YTask *t = c->t;
    if (t->depth <= 1) { t->alive = false; return YR_STOP; }
    // RETURN's values go to the caller's level, where @_RINT and the like read them.
    YFrame *callee = &t->frames[t->depth - 1], *caller = &t->frames[t->depth - 2];
    YValues *r = &caller->results;
    values_free(r);
    memset(r, 0, sizeof *r);
    for (int k = 0; k < c->in->argc; k++) {
        const YArg *a = &c->s->args[c->in->arg + k];
        const char *key = yr_arg_key(y, c->in, a);
        int n = 1;
        char kind = 0;
        if (!strncasecmp(key, "RINT", 4)) { kind = 'i'; n = key[4] ? atoi(key + 4) : 1; }
        else if (!strncasecmp(key, "RFLT", 4)) { kind = 'f'; n = key[4] ? atoi(key + 4) : 1; }
        else if (!strncasecmp(key, "RSTR", 4)) { kind = 's'; n = key[4] ? atoi(key + 4) : 1; }
        if (!kind || n < 1 || n >= YR_PARAMS) continue;
        YVal v = yr_eval(y, t, a->data, a->size);
        if (kind == 'i') r->i[n] = yv_as_int(&v);
        else if (kind == 'f') r->f[n] = yv_as_flt(&v);
        else r->s[n] = yv_as_str(&v);
        r->flag[n] = 1;
        yv_free(&v);
    }
    t->loopDepth = callee->loopBase;
    t->script = callee->retScript;
    t->ip = callee->retIp;
    values_free(&callee->params);
    values_free(&callee->results);
    t->depth--;
    return YR_JUMPED;
}

static int do_gosub(YCall *c) {
    YR *y = c->y;
    YTask *t = c->t;
    char *label = NULL;
    YValues p;
    memset(&p, 0, sizeof p);
    for (int k = 0; k < c->in->argc; k++) {
        const YArg *a = &c->s->args[c->in->arg + k];
        const char *key = yr_arg_key(y, c->in, a);
        YVal v = yr_eval(y, t, a->data, a->size);
        if (!strcmp(key, "#")) { label = yv_as_str(&v); yv_free(&v); continue; }
        int n = 0;
        char kind = 0;
        if (!strncasecmp(key, "PINT", 4)) { kind = 'i'; n = key[4] ? atoi(key + 4) : 1; }
        else if (!strncasecmp(key, "PFLT", 4)) { kind = 'f'; n = key[4] ? atoi(key + 4) : 1; }
        else if (!strncasecmp(key, "PSTR", 4)) { kind = 's'; n = key[4] ? atoi(key + 4) : 1; }
        if (kind && n >= 1 && n < YR_PARAMS) {
            if (kind == 'i') p.i[n] = yv_as_int(&v);
            else if (kind == 'f') p.f[n] = yv_as_flt(&v);
            else { free(p.s[n]); p.s[n] = yv_as_str(&v); }
            p.flag[n] = 1;
        }
        yv_free(&v);
    }
    u32 ret = t->ip + 1;
    u16 script = t->script;
    if (!label || !yr_label(y, label)) {
        yr_log(y, "GOSUB: label not found: %s (script %u ip %u)\n", label ? label : "(none)", script, t->ip);
        free(label);
        values_free(&p);
        return YR_NEXT;
    }
    if (y->traceGosub && y->log) {
        yr_log(y, "GOSUB %s [task %s depth %d]", label, t->id, t->depth);
        for (int i = 1; i < YR_PARAMS; i++) {
            if (p.flag[i] && p.s[i]) { char *u = yr_to_utf8(y->enc, p.s[i]); yr_log(y, " s%d=\"%s\"", i, u); free(u); }
            else if (p.flag[i]) yr_log(y, " %d=%lld/%g", i, (long long)p.i[i], p.f[i]);
        }
        yr_log(y, "\n");
    }
    YFrame *f = push_frame(t);
    f->retScript = script;
    f->retIp = ret;
    f->params = p;
    jump_label(y, t, label);
    free(label);
    return YR_JUMPED;
}

static int do_go(YCall *c) {
    YTask *t = c->t;
    YVal v = c->in->argc ? yr_eval(c->y, t, c->s->args[c->in->arg].data, c->s->args[c->in->arg].size) : yv_str("");
    char *label = yv_as_str(&v);
    yv_free(&v);
    YFrame *f = &t->frames[t->depth - 1];
    bool ok = jump_label(c->y, t, label);
    free(label);
    if (!ok) return YR_NEXT;
    // Loops left by the jump end.
    while (t->loopDepth > f->loopBase) {
        YLoop *l = &t->loops[t->loopDepth - 1];
        if (l->script == t->script && t->ip > l->start && t->ip <= l->end) break;
        t->loopDepth--;
    }
    return YR_JUMPED;
}

static int do_loop(YCall *c) {
    YTask *t = c->t;
    i64 count = -1;
    const YArg *set = yc_arg(c, "SET");
    if (set && set->size) count = yc_int_or(c, "SET", -1);
    if (count == 0) { t->ip = c->in->jump + 1; return YR_JUMPED; }
    if (t->loopDepth == t->loopCap) {
        t->loopCap = t->loopCap ? t->loopCap * 2 : 16;
        t->loops = yr_realloc(t->loops, sizeof(YLoop) * (size_t)t->loopCap);
    }
    t->loops[t->loopDepth++] = (YLoop){ t->script, t->ip, c->in->jump, count < 0 ? -1 : count, 1 };
    return YR_NEXT;
}

static int do_loopend(YCall *c) {
    YTask *t = c->t;
    if (!t->loopDepth) return YR_NEXT;
    YLoop *l = &t->loops[t->loopDepth - 1];
    if (l->count < 0 || l->lc < l->count) {
        l->lc++;
        t->script = l->script;
        t->ip = l->start + 1;
        return YR_JUMPED;
    }
    t->loopDepth--;
    return YR_NEXT;
}

static int do_loopbreak(YCall *c, bool cont) {
    YTask *t = c->t;
    i64 lv = yc_int_or(c, "LV", 1);
    if (lv < 1) lv = 1;
    int base = t->frames[t->depth - 1].loopBase;
    if (t->loopDepth <= base) return YR_NEXT;
    while (lv > 1 && t->loopDepth - 1 > base) { t->loopDepth--; lv--; }
    YLoop *l = &t->loops[t->loopDepth - 1];
    t->script = l->script;
    if (cont) {
        t->ip = l->end;   // LOOPEND counts and repeats
    } else {
        t->ip = l->end + 1;
        t->loopDepth--;
    }
    return YR_JUMPED;
}

static int exec(YR *y, YTask *t, YScript *s) {
    const YIns *in = &s->ins[t->ip];
    YCall c = { y, t, s, in };
    int id = y->cmds[in->cmd].id;
    switch (id) {
    case C_IF:
        if (cond_true(&c, in)) return YR_NEXT;
        branch_on(&c, t->ip);
        return YR_JUMPED;
    case C_ELSE:
    case C_IFBLEND:
        t->ip = chain_end(y, s, t->ip) + 1;
        return YR_JUMPED;
    case C_IFEND: return YR_NEXT;
    case C_LOOP: return do_loop(&c);
    case C_LOOPEND: return do_loopend(&c);
    case C_LOOPBREAK: return do_loopbreak(&c, false);
    case C_LOOPCONTINUE: return do_loopbreak(&c, true);
    case C_GOSUB: return do_gosub(&c);
    case C_RETURN: case C_RETURNBREAK: return do_return(&c);
    case C_GO: return do_go(&c);
    case C_TASK: case C_TASKEND: case C_TASKINFO: return yr_cmd_task(&c, id);
    case C_LABEL: case C_PREP: case C__IFDEF: case C__IFNDEF: case C__IFEND: case C_MACRO: return YR_NEXT;
    case C_INT: case C_FLT: case C_STR: case C_VAR: case C_S_INT: case C_S_FLT: case C_S_STR: case C_S_VAR:
    case C_F_INT: case C_F_FLT: case C_F_STR: case C_F_VAR: case C_G_INT: case C_G_INT2: case C_G_INT3: case C_G_INT4:
    case C_G_FLT: case C_G_FLT2: case C_G_FLT3: case C_G_FLT4: case C_G_STR: case C_G_STR2: case C_G_STR3:
    case C_G_STR4: case C_G_VAR: case C_LET: case C_VARACT: case C_VARINFO: case C_MATH: case C_LABELINFO:
        return yr_cmd_vars(&c, id);
    case C_TEXTOUT: case C_WORD: case C_RETURNCODE: case C_TEXT: case C_TEXTINFO: case C_FONT: case C_FONTINFO:
        return yr_cmd_text(&c, id);
    case C_CG: case C_CGACT: case C_CGEND: case C_CGINFO:
        return yr_cmd_cg(&c, id);
    case C_SOUND: case C_SOUNDEND: case C_SOUNDINFO: case C_MOVIE: case C_MOVIEEND: case C_MOVIEINFO: case C_MIDI:
    case C_MIDIEND: case C_MIDIINFO: case C_CDDA: case C_YMV: case C_YMVEND: case C_YMVINFO:
        return yr_cmd_sound(&c, id);
    default:
        return yr_cmd_system(&c, id);
    }
}

// Runs one task until it rests.
static void run_task(YR *y, YTask *t) {
    y->cur = t;
    if (t->clearText) {
        t->clearText = false;
        free(y->text);
        free(y->textFlag);
        y->text = y->textFlag = NULL;
    }
    for (long n = 0; t->alive && !y->quit; n++) {
        if (n >= BUDGET) {
            yr_log(y, "task %s ran %d instructions in one frame; resting it\n", t->id, BUDGET);
            break;
        }
        YScript *s = yr_script(y, t->script);
        if (!s || t->ip >= s->count) {
            // Falling off the end of a file ends the task, as the manual says of the system task.
            yr_log(y, "task %s ended at the end of script %u\n", t->id, t->script);
            t->alive = false;
            break;
        }
        int r = exec(y, t, s);
        if (r == YR_NEXT) t->ip++;
        else if (r == YR_YIELD) { t->ip++; break; }
        else if (r == YR_STOP) break;
    }
    y->cur = NULL;
}

static int by_priority(const void *a, const void *b) {
    const YTask *x = *(YTask *const *)a, *z = *(YTask *const *)b;
    if (x->z != z->z) return x->z > z->z ? -1 : 1;
    return x->order < z->order ? -1 : x->order > z->order;
}

void yr_vm_step(YR *y) {
    yr_input_frame(y);
    int n = y->taskCount;
    YTask **order = yr_alloc(sizeof(YTask *) * (size_t)(n ? n : 1));
    memcpy(order, y->tasks, sizeof(YTask *) * (size_t)n);
    qsort(order, (size_t)n, sizeof(YTask *), by_priority);
    for (int i = 0; i < n && !y->quit; i++) {
        YTask *t = order[i];
        if (!t->alive || !t->enabled) continue;
        if (t->waitFrames > 0) { t->waitFrames--; continue; }
        if (y->clock < t->waitUntil) continue;
        run_task(y, t);
    }
    free(order);
    int k = 0;
    for (int i = 0; i < y->taskCount; i++) {
        if (y->tasks[i]->alive) y->tasks[k++] = y->tasks[i];
        else yr_task_free(y->tasks[i]);
    }
    y->taskCount = k;
    if (!yr_task_find(y, "SYSTEM") && !y->quit) {
        yr_log(y, "the system task ended\n");
        y->quit = true;
    }
}

bool yr_vm_start(YR *y) {
    // The system task starts at the label SYSTEM_START (as the 0.495 engine requires), or else at the top of
    // system_start.yst.
    const YLabel *start = yr_label(y, "SYSTEM_START");
    if (start && yr_script(y, start->script)) {
        yr_task_new(y, "SYSTEM", 100, start->script, start->ip);
        yr_log(y, "start: label SYSTEM_START in script %u\n", start->script);
        return true;
    }
    for (u32 i = 0; i < y->scriptCount; i++) {
        const char *n = y->scripts[i].name;
        size_t len = strlen(n);
        if (len >= 16 && !strcasecmp(n + len - 16, "system_start.yst")) {
            yr_task_new(y, "SYSTEM", 100, (u16)i, 0);
            yr_log(y, "start: script %u (%s)\n", i, n);
            return true;
        }
    }
    yr_fail(y, "the game's start script (system_start.yst) is missing");
    return false;
}

// ---- TASK, TASKEND, TASKINFO ----

static void take_values(YCall *c, YValues *v, const char *ki, const char *kf, const char *ks) {
    for (int k = 0; k < c->in->argc; k++) {
        const YArg *a = &c->s->args[c->in->arg + k];
        const char *key = yr_arg_key(c->y, c->in, a);
        size_t li = strlen(ki);
        char kind = !strncasecmp(key, ki, li) ? 'i' : !strncasecmp(key, kf, li) ? 'f' : !strncasecmp(key, ks, li) ? 's' : 0;
        if (!kind) continue;
        int n = key[li] ? atoi(key + li) : 1;
        if (n < 1 || n >= YR_PARAMS) continue;
        YVal x = yr_eval(c->y, c->t, a->data, a->size);
        if (kind == 'i') v->i[n] = yv_as_int(&x);
        else if (kind == 'f') v->f[n] = yv_as_flt(&x);
        else { free(v->s[n]); v->s[n] = yv_as_str(&x); }
        v->flag[n] = 1;
        yv_free(&x);
    }
}

int yr_cmd_task(YCall *c, int id) {
    YR *y = c->y;
    char *tid = yc_str(c, "ID");
    YTask *t = tid ? yr_task_find(y, tid) : c->t;
    switch (id) {
    case C_TASK: {
        char *label = yc_str(c, "#");
        if (label) {
            const YLabel *l = yr_label(y, label);
            if (!l) yr_log(y, "TASK: label not found: %s\n", label);
            else {
                if (t && t != c->t) t->alive = false;   // a new task replaces one of the same ID
                YTask *n = yr_task_new(y, tid ? tid : "", (int)yc_int_or(c, "Z", 1), l->script, l->ip);
                take_values(c, &n->tvals, "TINT", "TFLT", "TSTR");
                // A new task starts running at once, in this frame, after the tasks before it.
                free(label);
                free(tid);
                return YR_NEXT;
            }
            free(label);
        }
        if (t) {
            i64 e;
            if (yc_int(c, "E", &e)) t->enabled = e != 0;
            if (yc_has(c, "SCRIPTPOS")) yc_unknown(c, "TASK SCRIPTPOS");
        }
        break;
    }
    case C_TASKEND:
        if (t) t->alive = false;
        if (t == c->t) { free(tid); return YR_STOP; }
        break;
    case C_TASKINFO: {
        YRef out;
        if (!yc_ref(c, "LET", &out)) break;
        YVal r = yv_int(0);
        if (yc_int_or(c, "EXIST", 0)) r = yv_int(t != NULL);
        else if (!t) r = yv_int(0);
        else if (yc_int_or(c, "E", 0)) r = yv_int(t->enabled);
        else if (yc_int_or(c, "Z", 0)) r = yv_int(t->z);
        else if (yc_int_or(c, "GOSUBNEST", 0)) r = yv_int(t->depth - 1);
        else if (yc_int_or(c, "LOOPNEST", 0)) r = yv_int(t->loopDepth);
        else if (yc_int_or(c, "FILENO", 0) || yc_int_or(c, "FILENO0", 0)) r = yv_int(t->script);
        else if (yc_int_or(c, "FILE", 0) || yc_int_or(c, "FILE0", 0))
            r = yv_str(t->script < y->scriptCount ? y->scripts[t->script].name : "");
        else if (yc_int_or(c, "LINE", 0) || yc_int_or(c, "LINE0", 0)) {
            YScript *s = yr_script(y, t->script);
            r = yv_int(s && t->ip < s->count ? s->ins[t->ip].line : 0);
        } else yc_unknown(c, "TASKINFO form");
        yr_ref_set(y, c->t, &out, &r);
        yv_free(&r);
        break;
    }
    }
    free(tid);
    return YR_NEXT;
}
