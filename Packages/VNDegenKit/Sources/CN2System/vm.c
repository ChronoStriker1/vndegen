// The bytecode interpreter. Every script context (the main script, CreateProcess threads, text blocks, system
// scripts started by keys) is a Thread with its own value and frame stacks, so any of them can wait without C
// recursion.
#include "n2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// A runaway loop yields after this many instructions in one frame instead of hanging the app.
#define STEP_BUDGET 2000000

Thread *thread_new(N2 *n, const char *name) {
    Thread *t = n2_alloc(sizeof *t);
    t->name = n2_strdup(name ? name : "");
    t->ctx = n->curRoot ? n->curRoot : &n->root;
    if (n->threadCount == n->threadCap) {
        n->threadCap = n->threadCap ? n->threadCap * 2 : 16;
        n->threads = n2_grow(n->threads, sizeof(Thread *) * (size_t)n->threadCap);
    }
    n->threads[n->threadCount++] = t;
    return t;
}

static void frame_free(Frame *f) {
    for (int i = 0; i < f->paramCount; i++) v_free(&f->params[i]);
    free(f->params);
}

static void thread_clear(Thread *t) {
    while (t->fp > 0) frame_free(&t->frames[--t->fp]);
    for (int i = 0; i < t->sp; i++) v_free(&t->stack[i]);
    t->sp = 0;
    free(t->waitQuery);
    t->waitQuery = NULL;
}

void thread_free(Thread *t) {
    if (!t) return;
    thread_clear(t);
    free(t->stack);
    free(t->frames);
    free(t->name);
    free(t);
}

void thread_kill(N2 *n, Thread *t) {
    thread_clear(t);
    t->wait = WAIT_DONE;
}

void vm_push(Thread *t, Value v) {
    if (t->sp == t->stackCap) {
        t->stackCap = t->stackCap ? t->stackCap * 2 : 64;
        t->stack = n2_grow(t->stack, sizeof(Value) * (size_t)t->stackCap);
    }
    t->stack[t->sp++] = v;
}

static Value pop(Thread *t) {
    if (t->sp <= 0) return v_null();
    return t->stack[--t->sp];
}

// Pushes a frame for c. pushResult says whether OP_RET leaves the return value for the caller.
static bool push_frame(Thread *t, Chunk *c, Value *args, int argc, bool pushResult) {
    if (t->fp > 512) return false;
    if (t->fp == t->frameCap) {
        t->frameCap = t->frameCap ? t->frameCap * 2 : 16;
        t->frames = n2_grow(t->frames, sizeof(Frame) * (size_t)t->frameCap);
    }
    Frame *f = &t->frames[t->fp++];
    memset(f, 0, sizeof *f);
    f->chunk = c;
    f->paramCount = c->paramCount;
    f->params = n2_alloc(sizeof(Value) * (size_t)(c->paramCount ? c->paramCount : 1));
    for (int i = 0; i < c->paramCount && i < argc; i++) f->params[i] = v_copy(&args[i]);
    f->stackBase = t->sp;
    f->pushes = pushResult;
    return true;
}

bool thread_call(N2 *n, Thread *t, Chunk *c, Value *args, int argc) {
    if (!push_frame(t, c, args, argc, false)) return false;
    if (t->wait == WAIT_DONE) t->wait = WAIT_NONE;
    return true;
}

static void call_chapter(N2 *n, Thread *t, const char *path, const char *name) {
    Script *s = n2_load_script(n, path);
    if (!s) return;
    Chunk *c = script_find(s, CHUNK_CHAPTER, name ? name : "main");
    if (!c) {
        n2_log(n, "no chapter %s in %s", name ? name : "main", path);
        return;
    }
    push_frame(t, c, NULL, 0, false);
}

static void call_scene(N2 *n, Thread *t, const char *spec) {
    // "nss/file.nss->scene", "@->scene" for the current file, or "scene".
    const char *arrow = strstr(spec, "->");
    Script *s = NULL;
    const char *name = spec;
    if (arrow) {
        name = arrow + 2;
        if (arrow - spec == 1 && spec[0] == '@') {
            s = t->fp ? t->frames[t->fp - 1].chunk->script : NULL;
        } else {
            char *path = n2_strndup(spec, (size_t)(arrow - spec));
            s = n2_load_script(n, path);
            free(path);
        }
    } else if (t->fp) {
        s = t->frames[t->fp - 1].chunk->script;
    }
    Chunk *c = s ? script_find(s, CHUNK_SCENE, name) : NULL;
    if (!c && s) c = script_find(s, CHUNK_CHAPTER, name);
    if (!c) {
        n2_log(n, "no scene %s", spec);
        return;
    }
    push_frame(t, c, NULL, 0, false);
}

static Value arith(int op, Value a, Value b) {
    Value r;
    if (op == OP_ADD && (a.t == V_STR || b.t == V_STR)) {
        char t1[64], t2[64];
        const char *x = v_tostr(&a, t1, sizeof t1), *y = v_tostr(&b, t2, sizeof t2);
        size_t lx = strlen(x), ly = strlen(y);
        char *s = n2_alloc(lx + ly + 1);
        memcpy(s, x, lx);
        memcpy(s + lx, y, ly);
        r = v_strn(s, (int)(lx + ly));
        free(s);
    } else {
        double x = v_tonum(&a), y = v_tonum(&b), z = 0;
        switch (op) {
        case OP_ADD: z = x + y; break;
        case OP_SUB: z = x - y; break;
        case OP_MUL: z = x * y; break;
        case OP_DIV: z = y != 0 ? x / y : 0; break;
        case OP_MOD: z = y != 0 ? fmod(x, y) : 0; break;
        }
        r = v_num(z);
    }
    v_free(&a);
    v_free(&b);
    return r;
}

static Value compare(int op, Value a, Value b) {
    bool res;
    if (op == OP_EQ || op == OP_NE) {
        res = v_equal(&a, &b);
        if (op == OP_NE) res = !res;
    } else {
        double x = v_tonum(&a), y = v_tonum(&b);
        res = op == OP_LT ? x < y : op == OP_LE ? x <= y : op == OP_GT ? x > y : x >= y;
    }
    v_free(&a);
    v_free(&b);
    return v_bool(res);
}

static Value *var_slot(N2 *n, Thread *t, const char *name) {
    return var_get(n, name);
}

void thread_run(N2 *n, Thread *t) {
    int budget = STEP_BUDGET;
    while (t->wait == WAIT_NONE && t->fp > 0 && !n->quit) {
        if (--budget == 0) {
            t->wait = WAIT_FRAME;
            break;
        }
        Frame *f = &t->frames[t->fp - 1];
        Chunk *c = f->chunk;
        int pc = f->pc;
        if (pc >= c->codeLen) {
            // Fell off the end: return null.
            vm_push(t, v_null());
            goto do_ret;
        }
        u32 ins = c->code[pc];
        int op = (int)(ins & 0xff), arg = (int)(ins >> 8);
        f->pc = pc + 1;
        switch (op) {
        case OP_NOP: break;
        case OP_CONST: vm_push(t, v_copy(&c->consts[arg])); break;
        case OP_NULL: vm_push(t, v_null()); break;
        case OP_TRUE: vm_push(t, v_bool(true)); break;
        case OP_FALSE: vm_push(t, v_bool(false)); break;
        case OP_DUP: vm_push(t, t->sp ? v_copy(&t->stack[t->sp - 1]) : v_null()); break;
        case OP_GETVAR: {
            Value *v = var_slot(n, t, c->consts[arg].s->s);
            vm_push(t, v ? v_copy(v) : v_null());
            break;
        }
        case OP_SETVAR: {
            Value v = pop(t);
            v.delta = 0;
            var_set(n, c->consts[arg].s->s, &v);
            v_free(&v);
            break;
        }
        case OP_GETPARAM: vm_push(t, arg < f->paramCount ? v_copy(&f->params[arg]) : v_null()); break;
        case OP_SETPARAM: {
            Value v = pop(t);
            if (arg < f->paramCount) {
                v_free(&f->params[arg]);
                f->params[arg] = v;
            } else {
                v_free(&v);
            }
            break;
        }
        case OP_GETIDX: {
            Value idx = pop(t);
            Arr *a = arr_get(n, c->consts[arg].s->s, false);
            int i = (int)v_tonum(&idx);
            vm_push(t, a && i >= 0 && i < a->count ? v_copy(&a->items[i]) : v_null());
            v_free(&idx);
            break;
        }
        case OP_SETIDX: {
            Value v = pop(t), idx = pop(t);
            Arr *a = arr_get(n, c->consts[arg].s->s, true);
            int i = (int)v_tonum(&idx);
            if (i >= 0 && i < 100000) {
                if (i >= a->count) {
                    a->items = n2_grow(a->items, sizeof(Value) * (size_t)(i + 1));
                    for (int k = a->count; k <= i; k++) a->items[k] = v_null();
                    a->count = i + 1;
                }
                v_free(&a->items[i]);
                a->items[i] = v;
            } else {
                v_free(&v);
            }
            v_free(&idx);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD: {
            Value b = pop(t), a = pop(t);
            bool delta = a.delta || b.delta;
            Value r = arith(op, a, b);
            r.delta = delta;
            vm_push(t, r);
            break;
        }
        case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE: {
            Value b = pop(t), a = pop(t);
            vm_push(t, compare(op, a, b));
            break;
        }
        case OP_NOT: {
            Value a = pop(t);
            bool r = !v_truthy(&a);
            v_free(&a);
            vm_push(t, v_bool(r));
            break;
        }
        case OP_NEG: {
            Value a = pop(t);
            Value r = v_num(-v_tonum(&a));
            r.delta = a.delta;
            v_free(&a);
            vm_push(t, r);
            break;
        }
        case OP_DELTA: {
            Value a = pop(t);
            if (a.t != V_NUM) {
                double x = v_tonum(&a);
                v_free(&a);
                a = v_num(x);
            }
            a.delta = 1;
            vm_push(t, a);
            break;
        }
        case OP_POP: {
            Value a = pop(t);
            v_free(&a);
            break;
        }
        case OP_JMP: f->pc = arg; break;
        case OP_JZ:
        case OP_JNZ: {
            Value a = pop(t);
            bool truth = v_truthy(&a);
            v_free(&a);
            if (truth == (op == OP_JNZ)) f->pc = arg;
            break;
        }
        case OP_CALL: {
            const char *name = c->consts[arg & 0xffff].s->s;
            int argc = arg >> 16;
            Value *args = &t->stack[t->sp - argc];
            Chunk *fn = n2_find_function(n, name);
            if (fn) {
                if (!push_frame(t, fn, args, argc, true)) {
                    n2_fail(n, "the scripts called functions too deeply (%s)", name);
                    break;
                }
                // The arguments were copied into the frame; drop them from the caller's stack.
                for (int i = 0; i < argc; i++) v_free(&args[i]);
                t->sp -= argc;
                t->frames[t->fp - 1].stackBase = t->sp;
                break;
            }
            BuiltinFn b = builtin_find(name);
            Value ret = v_null();
            bool waits = false;
            if (b) {
                waits = b(n, t, args, argc, &ret);
            } else {
                static Map warned;
                if (!map_get(&warned, name)) {
                    map_set(&warned, name, (void *)1);
                    n2_log(n, "unknown function %s", name);
                }
            }
            for (int i = 0; i < argc; i++) v_free(&args[i]);
            t->sp -= argc;
            vm_push(t, ret);
            (void)waits;
            break;
        }
        case OP_RET:
        do_ret: {
            Value r = pop(t);
            Frame *done = &t->frames[t->fp - 1];
            bool pushes = done->pushes;
            // Drop anything the frame left on the stack.
            while (t->sp > done->stackBase) {
                Value x = pop(t);
                v_free(&x);
            }
            frame_free(done);
            t->fp--;
            if (pushes) vm_push(t, r);
            else v_free(&r);
            if (t->fp == 0) t->wait = WAIT_DONE;
            break;
        }
        case OP_CALL_CHAPTER: {
            Value p = pop(t);
            char tmp[64];
            char *path = n2_strdup(v_tostr(&p, tmp, sizeof tmp));
            v_free(&p);
            // "file->name" calls a named chapter.
            char *arrow = strstr(path, "->");
            if (arrow) {
                *arrow = 0;
                call_chapter(n, t, path, arrow + 2);
            } else {
                call_chapter(n, t, path, NULL);
            }
            free(path);
            break;
        }
        case OP_CALL_SCENE: {
            Value p = pop(t);
            char tmp[64];
            char *spec = n2_strdup(v_tostr(&p, tmp, sizeof tmp));
            v_free(&p);
            call_scene(n, t, spec);
            free(spec);
            break;
        }
        case OP_SELECT_BEGIN:
            f->selectPressed = false;
            break;
        case OP_ISPRESSED: {
            const char *label = c->consts[arg].s->s;
            if (label[0] == '@') label++;
            bool pressed = map_get(&n->pressedButtons, label) != NULL;
            if (pressed) {
                f->selectPressed = true;
                map_set(&n->pressedButtons, label, NULL);
            }
            vm_push(t, v_bool(pressed));
            break;
        }
        case OP_SELECT_END:
            if (f->selectPressed) {
                f->selectPressed = false;
            } else {
                f->pc = arg;
                t->wait = WAIT_FRAME;
            }
            break;
        case OP_PRE: {
            TextBlock *b = &c->script->blocks[arg];
            n->activeBlock = b;
            n->activeBlockScript = c->script;
            Value box = v_str(b->box);
            var_set(n, "$SYSTEM_present_preprocess", &box);
            v_free(&box);
            char *tn = n2_fmt("@%s", b->name);
            Value text = v_str(tn);
            var_set(n, "$SYSTEM_present_text", &text);
            v_free(&text);
            free(tn);
            break;
        }
        case OP_TEXT:
            if (t->textOwner) {
                text_append(n, t->textOwner, c->consts[arg].s->s);
                t->wait = WAIT_TEXTSEG;
            }
            break;
        case OP_TEXT_END:
            vm_push(t, v_null());
            goto do_ret;
        }
    }
}
