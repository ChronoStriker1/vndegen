#include "n2.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define NPS_TAG 0xf1f2b5c5u
#define REC_ROOM 0x200

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

typedef struct Proc {
    int first, last;
    Script **files;
    int fileCount;
    ScanRecord *recs;
    int recCount;
} Proc;

static int stmt_at_ip(const Chunk *c, int ip) {
    int best = -1;
    for (int i = 0; i < c->stmtCount; i++) {
        const Stmt *s = &c->stmts[i];
        if (s->ip <= ip && ip < s->ipEnd && (best < 0 || s->ip >= c->stmts[best].ip)) best = i;
    }
    return best;
}

static int enclosing(const Chunk *c, int ip, int kind) {
    int best = -1;
    for (int i = 0; i < c->stmtCount; i++) {
        const Stmt *s = &c->stmts[i];
        if (s->kind == kind && s->ip <= ip && ip < s->ipEnd && (best < 0 || s->ip >= c->stmts[best].ip)) best = i;
    }
    return best;
}

static int stmt_starting(const Chunk *c, int src) {
    int best = -1;
    for (int i = 0; i < c->stmtCount; i++) {
        const Stmt *s = &c->stmts[i];
        if (s->src <= src && (best < 0 || s->src > c->stmts[best].src)) best = i;
    }
    return best;
}

static int stmt_before(const Chunk *c, int src) {
    int best = -1;
    for (int i = 0; i < c->stmtCount; i++) {
        const Stmt *s = &c->stmts[i];
        if (s->src < src && (best < 0 || s->src >= c->stmts[best].src)) best = i;
    }
    return best;
}

static int after_line(const Script *s, int at) {
    while (at < (int)s->rawLen && s->raw[at] != '\n') at++;
    if (at < (int)s->rawLen) at++;
    while (at < (int)s->rawLen && (s->raw[at] == ' ' || s->raw[at] == '\t' || s->raw[at] == '\r' || s->raw[at] == '\n'))
        at++;
    return at;
}

static void add_file(N2 *n, Proc *p, Script *s) {
    if (!s) return;
    for (int i = 0; i < p->fileCount; i++)
        if (p->files[i] == s) return;
    p->files = n2_grow(p->files, sizeof(Script *) * (size_t)(p->fileCount + 1));
    p->files[p->fileCount++] = s;
    for (int i = 0; i < s->includeCount; i++) add_file(n, p, n2_load_script(n, s->includes[i]));
}

static int file_index(const Proc *p, const Script *s) {
    for (int i = 0; i < p->fileCount; i++)
        if (p->files[i] == s) return i;
    return -1;
}

static int scan_kind(int chunkKind) {
    return chunkKind == CHUNK_CHAPTER ? SCAN_CHAPTER : chunkKind == CHUNK_SCENE ? SCAN_SCENE : SCAN_FUNCTION;
}

static void scan_files(Proc *p) {
    u8 **texts = n2_alloc(sizeof(u8 *) * (size_t)(p->fileCount ? p->fileCount : 1));
    size_t *lens = n2_alloc(sizeof(size_t) * (size_t)(p->fileCount ? p->fileCount : 1));
    for (int i = 0; i < p->fileCount; i++) {
        texts[i] = n2_alloc(p->files[i]->rawLen + 1);
        memcpy(texts[i], p->files[i]->raw, p->files[i]->rawLen);
        lens[i] = p->files[i]->rawLen;
    }
    p->recs = scan_process(texts, lens, p->fileCount, &p->recCount);
    for (int i = 0; i < p->fileCount; i++) free(texts[i]);
    free(texts);
    free(lens);
}

static int record_of(const Proc *p, const Chunk *c) {
    int slot = file_index(p, c->script);
    for (int r = 0; r < p->recCount; r++)
        if (p->recs[r].slot == slot && p->recs[r].base == c->body && p->recs[r].kind == scan_kind(c->kind)) return r;
    return -1;
}

static void proc_free(Proc *p) {
    free(p->files);
    scan_free(p->recs, p->recCount);
}

static void put_name(Buf *b, const char *sjis, size_t len) {
    buf_put(b, sjis, len);
    u8 z[4] = {0};
    buf_put(b, z, 4);
}

static char *pc_file_name(const Script *s) {
    char *name = n2_strdup(s->path);
    for (char *q = name; *q; q++)
        if (*q == '/') *q = '\\';
    return name;
}

static bool text_shown(N2 *n, Obj *o, const char *name) {
    for (int i = 0; i < o->childCount; i++) {
        Obj *c = o->children[i];
        if (c->kind == OBJ_TEXT && c->text && !c->erased && n2_streq_ci(c->name, name)) return true;
        if (text_shown(n, c, name)) return true;
    }
    return false;
}

static u32 rec_id(int proc, int rec) { return 0x01000000u + ((u32)proc << 16) + (u32)rec + 1; }
static u32 proc_id(int proc) { return 0x00e00000u + (u32)proc * 0x100 + 0x10; }

u32 slot_proc_serial(int proc) { return 0x2000u + (u32)proc * 0x111; }

Buf slot_script_write(N2 *n, int *procCount) {
    Buf b = {0};
    Thread *t = n->main;
    *procCount = 0;
    if (!t || t->fp == 0) return b;
    int top = t->fp - 1, runSrc = -1;
    if (n->activeBlock && n->activeBlockScript && text_shown(n, &n->root, n->activeBlock->name)) {
        for (int f = t->fp - 1; f >= 0; f--) {
            Chunk *c = t->frames[f].chunk;
            if (c->script != n->activeBlockScript) continue;
            if (c->body <= n->activeBlock->srcpos && stmt_starting(c, n->activeBlock->srcpos) >= 0 &&
                c->stmts[stmt_starting(c, n->activeBlock->srcpos)].src == n->activeBlock->srcpos) {
                top = f;
                runSrc = n->activeBlock->srcpos;
                break;
            }
        }
    }
    Proc procs[64];
    int np = 0;
    for (int f = 0; f <= top; f++) {
        Chunk *c = t->frames[f].chunk;
        if (c->kind == CHUNK_CHAPTER || np == 0) {
            if (np == 64) break;
            memset(&procs[np], 0, sizeof(Proc));
            procs[np].first = f;
            np++;
        }
        procs[np - 1].last = f;
        add_file(n, &procs[np - 1], c->script);
    }
    for (int i = 0; i < np; i++) scan_files(&procs[i]);
    int chain[512];
    bool ok = true;
    for (int f = 0; f <= top && f < 512; f++) {
        int pi = 0;
        while (pi + 1 < np && procs[pi + 1].first <= f) pi++;
        chain[f] = record_of(&procs[pi], t->frames[f].chunk);
        if (chain[f] < 0) {
            n2_log(n, "save: no PC record for %s in %s", t->frames[f].chunk->name, t->frames[f].chunk->script->path);
            ok = false;
        }
    }
    if (!ok) {
        for (int i = 0; i < np; i++) proc_free(&procs[i]);
        return b;
    }
    for (int f = 0; f <= top; f++) {
        Frame *fr = &t->frames[f];
        for (int k = 0; k < fr->chunk->paramCount && k < fr->paramCount; k++) var_set(n, fr->chunk->params[k], &fr->params[k]);
    }
    u32 textTotal = 0, empty = 0, recTotal = 0;
    for (int i = 0; i < np; i++) {
        for (int k = 0; k < procs[i].fileCount; k++) textTotal += (u32)procs[i].files[k]->rawLen;
        empty += 512u - (u32)procs[i].fileCount;
        recTotal += (u32)procs[i].recCount;
    }
    u32 size = 16 + textTotal + (recTotal + (u32)np) * REC_ROOM + 4 * empty;
    u8 *out = n2_alloc(size);
    memcpy(out, "NPSS", 4);
    u32 head[3] = {NPS_TAG, size, (u32)np};
    for (int k = 0; k < 3; k++)
        for (int j = 0; j < 4; j++) out[4 + k * 4 + j] = (u8)(head[k] >> (8 * j));
    u32 textBefore = 0, recBefore = 0, fileId = 0x00100000u;
    for (int i = 0; i < np; i++) {
        Proc *p = &procs[i];
        Buf pb = {0};
        for (int k = 0; k < 512; k++) {
            if (k >= p->fileCount) {
                buf_u32(&pb, 0);
                continue;
            }
            Script *s = p->files[k];
            buf_u32(&pb, fileId += 0x10);
            char *name = pc_file_name(s);
            put_name(&pb, name, strlen(name));
            free(name);
            buf_u32(&pb, (u32)s->rawLen);
            buf_put(&pb, s->raw, s->rawLen);
        }
        int cur = chain[p->last];
        u32 hdr[7] = {proc_id(i), slot_proc_serial(i), 1, i == np - 1, rec_id(i, cur), i + 1 < np ? proc_id(i + 1) : 0,
                      i > 0 ? proc_id(i - 1) : 0};
        for (int k = 0; k < 7; k++) buf_u32(&pb, hdr[k]);
        int *frameOf = n2_alloc(sizeof(int) * (size_t)(p->recCount ? p->recCount : 1));
        for (int r = 0; r < p->recCount; r++) frameOf[r] = -1;
        for (int f = p->first; f <= p->last; f++) frameOf[chain[f]] = f;
        for (int r = 0; r < p->recCount; r++) {
            ScanRecord *sr = &p->recs[r];
            int f = frameOf[r];
            u32 state = 0, f2 = 0, pos = 0, ls = 0, le = 0, ss = 0, se = 0, ret = 0;
            char *scenePath = NULL;
            if (f >= 0) {
                Frame *fr = &t->frames[f];
                Chunk *c = fr->chunk;
                state = f == top ? 1 : 2;
                f2 = sr->kind == SCAN_FUNCTION ? 0 : 1;
                int ip = fr->pc > 0 ? fr->pc - 1 : 0;
                int si = stmt_at_ip(c, ip);
                if (f == top && runSrc >= 0) {
                    si = stmt_starting(c, runSrc);
                    pos = (u32)(runSrc - c->body);
                    ip = si >= 0 ? c->stmts[si].ip : 0;
                } else if (si >= 0 && f == top) {
                    pos = (u32)(c->stmts[si].src - c->body);
                } else if (si >= 0) {
                    const Stmt *st = &c->stmts[si];
                    int end = st->kind == STMT_CALL_SCENE ? after_line(c->script, st->srcEnd) : st->srcEnd;
                    pos = (u32)(end - c->body);
                }
                int w = enclosing(c, ip, STMT_WHILE);
                if (w >= 0) {
                    ls = (u32)(c->stmts[w].loopStart - c->body);
                    le = (u32)(c->stmts[w].loopClose - c->body);
                }
                int sel = enclosing(c, ip, STMT_SELECT);
                if (sel >= 0) {
                    ss = (u32)(c->stmts[sel].loopStart - c->body);
                    se = (u32)(c->stmts[sel].loopClose - c->body);
                }
                if (f > p->first) ret = rec_id(i, chain[f - 1]);
                Buf path = {0};
                for (int g = p->first; g <= f; g++) {
                    if (t->frames[g].chunk->kind != CHUNK_SCENE) continue;
                    if (path.len) buf_u8(&path, '/');
                    buf_str(&path, t->frames[g].chunk->name);
                }
                buf_u8(&path, 0);
                scenePath = (char *)path.data;
            }
            size_t sl = 0;
            char *sp = scenePath ? n2_utf8_to_sjis(scenePath, &sl) : NULL;
            put_name(&pb, sp ? sp : "", sl);
            free(sp);
            free(scenePath);
            u32 vals[8] = {rec_id(i, r), 1, state, f2, 0, (u32)sr->kind, 1, 0xffffffffu};
            for (int k = 0; k < 8; k++) buf_u32(&pb, vals[k]);
            for (int k = 0; k < 16; k++) {
                if (f >= 0 && k < sr->paramCount) {
                    buf_u32(&pb, 1);
                    put_name(&pb, sr->params[k], strlen(sr->params[k]));
                } else {
                    buf_u32(&pb, 0);
                }
            }
            u32 offs[9] = {pos, ls, ls ? le : 0, 0, 0, ss, ss ? se : 0, 0, ret};
            for (int k = 0; k < 9; k++) buf_u32(&pb, offs[k]);
        }
        free(frameOf);
        u32 at = 16 + textBefore + (recBefore + (u32)i) * REC_ROOM;
        if (at + pb.len <= size) memcpy(out + at, pb.data, pb.len);
        buf_free(&pb);
        for (int k = 0; k < p->fileCount; k++) textBefore += (u32)p->files[k]->rawLen;
        recBefore += (u32)p->recCount;
    }
    *procCount = np;
    for (int i = 0; i < np; i++) proc_free(&procs[i]);
    buf_put(&b, out, size);
    free(out);
    return b;
}

typedef struct SavedRec {
    u32 id, state, kind, pos, ret;
} SavedRec;

typedef struct SavedProc {
    u32 id, serial, running, cur, child, parent;
    Proc p;
    u8 **texts;
    SavedRec *recs;
} SavedProc;

static Chunk *chunk_at(Script *s, const ScanRecord *r) {
    for (int i = 0; i < s->chunkCount; i++) {
        Chunk *c = s->chunks[i];
        if (c->kind != CHUNK_TEXT && c->body == r->base && scan_kind(c->kind) == r->kind) return c;
    }
    return NULL;
}

static const u8 *skip_name(const u8 *d, size_t size, size_t *o) {
    const u8 *s = d + *o;
    const u8 *z = memchr(s, 0, size - *o);
    if (!z) return NULL;
    *o = (size_t)(z - d) + 4;
    return s;
}

bool slot_script_read(N2 *n, const u8 *d, size_t size, bool rewindText) {
    if (size < 16 || memcmp(d, "NPSS", 4) != 0) return false;
    u32 np = rd32(d + 12);
    if (np == 0 || np > 64) return false;
    SavedProc *sp = n2_alloc(sizeof(SavedProc) * np);
    bool ok = true;
    u32 textBefore = 0, recBefore = 0;
    for (u32 i = 0; i < np && ok; i++) {
        size_t o = 16 + textBefore + ((size_t)recBefore + i) * REC_ROOM;
        SavedProc *P = &sp[i];
        u8 *texts[512];
        size_t lens[512];
        for (int k = 0; k < 512 && ok; k++) {
            if (o + 4 > size) {
                ok = false;
                break;
            }
            u32 id = rd32(d + o);
            o += 4;
            if (!id) continue;
            const u8 *name = skip_name(d, size, &o);
            if (!name || o + 4 > size) {
                ok = false;
                break;
            }
            u32 len = rd32(d + o);
            o += 4;
            if (o + len > size) {
                ok = false;
                break;
            }
            char *u = n2_sjis_to_utf8(name, strlen((const char *)name), NULL);
            for (char *q = u; *q; q++)
                if (*q == '\\') *q = '/';
            Script *s = n2_load_script(n, u);
            if (!s) n2_log(n, "load: missing script %s", u);
            free(u);
            if (!s) {
                ok = false;
                break;
            }
            P->p.files = n2_grow(P->p.files, sizeof(Script *) * (size_t)(P->p.fileCount + 1));
            P->p.files[P->p.fileCount] = s;
            texts[P->p.fileCount] = n2_alloc(len + 1);
            memcpy(texts[P->p.fileCount], d + o, len);
            lens[P->p.fileCount] = len;
            P->p.fileCount++;
            o += len;
            textBefore += len;
        }
        if (!ok || o + 28 > size) {
            ok = false;
            break;
        }
        P->id = rd32(d + o);
        P->serial = rd32(d + o + 4);
        P->running = rd32(d + o + 12);
        P->cur = rd32(d + o + 16);
        P->child = rd32(d + o + 20);
        P->parent = rd32(d + o + 24);
        o += 28;
        P->p.recs = scan_process(texts, lens, P->p.fileCount, &P->p.recCount);
        for (int k = 0; k < P->p.fileCount; k++) free(texts[k]);
        P->recs = n2_alloc(sizeof(SavedRec) * (size_t)(P->p.recCount ? P->p.recCount : 1));
        for (int r = 0; r < P->p.recCount && ok; r++) {
            if (!skip_name(d, size, &o) || o + 32 > size) {
                ok = false;
                break;
            }
            SavedRec *R = &P->recs[r];
            R->id = rd32(d + o);
            R->state = rd32(d + o + 8);
            R->kind = rd32(d + o + 20);
            o += 32;
            for (int k = 0; k < 16 && ok; k++) {
                if (o + 4 > size) {
                    ok = false;
                    break;
                }
                u32 fl = rd32(d + o);
                o += 4;
                if (fl == 1 && !skip_name(d, size, &o)) ok = false;
            }
            if (!ok || o + 36 > size) {
                ok = false;
                break;
            }
            R->pos = rd32(d + o);
            R->ret = rd32(d + o + 32);
            o += 36;
            if (R->kind && R->kind != (u32)P->p.recs[r].kind) {
                n2_log(n, "load: record %d is kind %u in the save and %d in the script", r, R->kind, P->p.recs[r].kind);
                ok = false;
            }
        }
        recBefore += (u32)P->p.recCount;
    }
    int order[64], no = 0;
    if (ok) {
        int at = -1;
        for (u32 i = 0; i < np; i++)
            if (!sp[i].parent) at = (int)i;
        while (at >= 0 && no < (int)np) {
            order[no++] = at;
            int next = -1;
            for (u32 i = 0; i < np; i++)
                if (sp[at].child && sp[i].id == sp[at].child) next = (int)i;
            at = next;
        }
        if (no != (int)np) ok = false;
    }
    typedef struct Want {
        Chunk *chunk;
        int ip;
        int src;
        bool running;
    } Want;
    Want want[512];
    int nw = 0;
    for (int oi = 0; ok && oi < no; oi++) {
        SavedProc *P = &sp[order[oi]];
        int chainRec[512], nc = 0;
        u32 id = P->cur;
        while (id && nc < 512) {
            int r = -1;
            for (int k = 0; k < P->p.recCount; k++)
                if (P->recs[k].id == id) r = k;
            if (r < 0) break;
            chainRec[nc++] = r;
            id = P->recs[r].ret;
        }
        for (int k = nc - 1; k >= 0 && ok; k--) {
            int r = chainRec[k];
            ScanRecord *sr = &P->p.recs[r];
            Chunk *c = chunk_at(P->p.files[sr->slot], sr);
            if (!c) {
                n2_log(n, "load: no chunk for record %d (%s)", r, sr->name ? sr->name : "");
                ok = false;
                break;
            }
            int target = c->body + (int)P->recs[r].pos;
            bool running = P->recs[r].state == 1;
            int si = running ? stmt_starting(c, target) : stmt_before(c, target);
            Want *w = &want[nw++];
            w->chunk = c;
            w->running = running;
            w->src = target;
            w->ip = si < 0 ? 0 : running ? c->stmts[si].ip : c->stmts[si].ipEnd;
            if (nw == 512) break;
        }
    }
    if (ok && rewindText) {
        for (int k = nw - 1; k >= 0; k--) {
            Chunk *c = want[k].chunk;
            int best = -1;
            for (int i = 0; i < c->stmtCount; i++) {
                const Stmt *s = &c->stmts[i];
                if (s->src < want[k].src && s->src + 4 <= (int)c->script->rawLen &&
                    strncasecmp((const char *)c->script->raw + s->src, "<PRE", 4) == 0 &&
                    (best < 0 || s->src > c->stmts[best].src))
                    best = i;
            }
            if (best >= 0) {
                want[k].ip = c->stmts[best].ip;
                want[k].running = true;
                nw = k + 1;
                break;
            }
        }
    }
    if (ok && nw > 0) {
        Thread *t = n->main;
        thread_kill(n, t);
        for (int k = 0; k < nw; k++) {
            Chunk *c = want[k].chunk;
            Value *args = n2_alloc(sizeof(Value) * (size_t)(c->paramCount ? c->paramCount : 1));
            for (int q = 0; q < c->paramCount; q++) {
                Value *v = var_get(n, c->params[q]);
                args[q] = v ? v_copy(v) : v_null();
            }
            thread_call(n, t, c, args, c->paramCount);
            for (int q = 0; q < c->paramCount; q++) v_free(&args[q]);
            free(args);
            t->frames[t->fp - 1].pc = want[k].ip;
            int line = 1;
            int src = want[k].ip < c->codeLen ? c->srcpos[want[k].ip] : c->body;
            for (int q = 0; q < src && q < (int)c->script->rawLen; q++) line += c->script->raw[q] == '\n';
            n2_log(n, "load: %s %s resumes at line %d", c->script->path, c->name, line);
        }
        t->wait = WAIT_NONE;
        n->mainScript = want[nw - 1].chunk->script;
    }
    for (u32 i = 0; i < np; i++) {
        free(sp[i].p.files);
        scan_free(sp[i].p.recs, sp[i].p.recCount);
        free(sp[i].recs);
    }
    free(sp);
    return ok && nw > 0;
}
