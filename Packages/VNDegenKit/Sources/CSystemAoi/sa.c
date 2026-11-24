#include "sa_private.h"

enum { TOWN_HOOK = 1, COMMAND_HOOK = 31 };

#include <stdarg.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

#include "vnfont.h"

#define PAN_MS_PER_PX 25
#define PAN_SKIP_SPEED 16


static const char *const uiNames[UI_COUNT] = {"msg_b00.agf", "msg_b01.agf", "msg_b02.agf", "msg_b03.agf", "msg_b04.agf",
                                              "msg_b05.agf", "msg_b06.agf", "msg_a03.agf", "msg_c00.agf", "msg_c01.agf",
                                              "msg_c02.agf"};

enum { TITLE_X = 640, TITLE_Y = 369, TITLE_H = 50, TITLE_W = 640 };

typedef struct Button {
    int ui, x, y, w, h;
} Button;

enum { BTN_AUTO, BTN_SKIP, BTN_LOG, BTN_REPEAT, BTN_OPTION, BTN_CLOSE, BTN_COUNT };

static const Button buttons[BTN_COUNT] = {
    {UI_AUTO, 1086, 554, 78, 30}, {UI_SKIP, 1086, 584, 78, 30},   {UI_LOG, 1086, 614, 80, 30},
    {UI_REPEAT, 1086, 644, 80, 30}, {UI_OPTION, 1086, 674, 80, 30}, {UI_CLOSE, 1191, 554, 32, 30},
};


void sa_log(SA *s, const char *fmt, ...) {
    if (!s || !s->logFile) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(s->logFile, fmt, ap);
    va_end(ap);
    fputc('\n', s->logFile);
    fflush(s->logFile);
}

static void host_log(void *ctx, const char *msg) { sa_log(ctx, "%s", msg); }

static Image *load_image(SA *s, const char *name) {
    size_t n;
    u8 *d = sa_vfs_read(&s->vfs, name, &n);
    if (!d) {
        sa_log(s, "missing picture %s", name);
        return NULL;
    }
    Image *im = sa_agf_decode(d, n);
    free(d);
    if (!im) sa_log(s, "can't decode picture %s", name);
    return im;
}

static void picture_set(SA *s, Picture *p, const char *name, int xslot, int yslot) {
    if (strcmp(p->name, name)) {
        sa_image_free(p->img);
        p->img = load_image(s, name);
        snprintf(p->name, sizeof p->name, "%s", name);
    }
    p->xslot = xslot;
    p->yslot = yslot;
    s->dirty = true;
}

static void picture_clear(Picture *p) {
    sa_image_free(p->img);
    memset(p, 0, sizeof *p);
}

void sa_show_frame_as_bg(SA *s, const u32 *px, const char *name) {
    picture_clear(&s->bg);
    s->bg.img = sa_image_new(SA_W, SA_H);
    memcpy(s->bg.img->px, px, sizeof(u32) * SA_W * SA_H);
    snprintf(s->bg.name, sizeof s->bg.name, "%s", name);
    s->bgX = s->bgY = 0;
    s->panDur = 0;
    for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
    s->dirty = true;
}

static void clear_text(SA *s) {
    for (int i = 0; i < s->lineCount; i++) free(s->lines[i]);
    s->lineCount = 0;
    s->shown = s->total = 0;
    s->textMs = 0;
}

static int utf8_len(const char *t) {
    int n = 0;
    for (u32 cp; *t; n++) t += sa_utf8_decode(t, &cp);
    return n;
}

static void host_text(void *ctx, const char *utf8, bool newLine) {
    SA *s = ctx;
    if (s->lineCount == MSG_LINES) return;
    s->lines[s->lineCount++] = sa_strdup(utf8);
    s->total += utf8_len(utf8);
    if (s->voice[0]) {
        snprintf(s->lastVoice, sizeof s->lastVoice, "%s.aog", s->voice);
        snprintf(s->pageVoice, sizeof s->pageVoice, "%s", s->lastVoice);
        sa_play(s->mix, CH_VOICE, 0, s->lastVoice, false);
        s->voice[0] = 0;
    }
    s->dirty = true;
}

static int face_index_by_prefix(SA *s, const char *prefix) {
    for (int i = 0; i < s->faceCount; i++)
        if (!strncmp(s->facePrefix[i], prefix, 5)) return i;
    return -1;
}

static void load_faces(SA *s) {
    size_t n;
    u8 *raw = sa_vfs_read(&s->vfs, "evntFace.obj", &n);
    if (!raw) return;
    for (size_t i = 0; i < n; i++) raw[i] = (u8)~raw[i];
    size_t start = 0;
    for (size_t k = 1; k * 12 + 24 < n && !start; k++) {
        size_t P = k * 12;
        bool ok = true;
        for (int r = 0; r < 3 && ok; r++)
            for (int c = 0; c < 2 && ok; c++) {
                u32 off = rd32(raw + r * 12 + c * 4);
                if (P + off + 2 > n || (!raw[P + off] && !raw[P + off + 1]) || (off && (raw[P + off - 2] || raw[P + off - 1]))) ok = false;
            }
        if (ok) start = P;
    }
    if (!start) {
        free(raw);
        return;
    }
    char *prev = NULL;
    for (size_t p = start; p + 1 < n && s->faceCount < 64;) {
        size_t q = p;
        while (q + 1 < n && (raw[q] || raw[q + 1])) q += 2;
        char *str = sa_utf16_to_utf8(raw + p, (int)((q - p) / 2));
        if (!str[0] || !strcmp(str, "    ")) {
            free(str);
            break;
        }
        if (!strncmp(str, "ta", 2) && strlen(str) == 5 && prev) {
            s->faceName[s->faceCount] = prev;
            s->facePrefix[s->faceCount] = str;
            snprintf(s->faceExpr[s->faceCount], 4, "000");
            s->faceCount++;
            prev = NULL;
        } else {
            free(prev);
            prev = str;
        }
        p = q + 2;
    }
    free(prev);
    free(raw);
    sa_log(s, "%d face sets", s->faceCount);
}

static void host_speaker(void *ctx, const char *name, const char *voice, const char *face) {
    SA *s = ctx;
    char want[40] = "";
    if (face[0]) snprintf(want, sizeof want, "%s", face);
    else
        for (int i = 0; i < s->faceCount; i++)
            if (!strcmp(s->faceName[i], name)) {
                snprintf(want, sizeof want, "ka%.3s%s.agf", s->facePrefix[i] + 2, s->faceExpr[i]);
                break;
            }
    if (want[0]) picture_set(s, &s->face, want, 0, 0);
    else picture_clear(&s->face);
    snprintf(s->name, sizeof s->name, "%s", name);
    snprintf(s->voice, sizeof s->voice, "%s", voice[0] == '!' ? voice + 1 : "");
    s->dirty = true;
}

static void host_page(void *ctx) {
    SA *s = ctx;
    sa_log_add(&s->log, s->name, s->pageVoice, s->lines, s->lineCount);
    s->pageVoice[0] = 0;
    s->dirty = true;
}

static double asd_time(const char *v) {
    double t = 0;
    const char *c = strchr(v, ':');
    if (c) {
        t = atof(v) * 60;
        v = c + 1;
    }
    return t + atof(v);
}

static void load_bgm_defs(SA *s) {
    size_t n;
    u8 *d = sa_vfs_read(&s->vfs, "bgm.asd", &n);
    if (!d) return;
    d[n] = 0;
    BgmDef *cur = NULL;
    for (char *line = (char *)d, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        while (*line == ' ' || *line == '\t') line++;
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        if (!strncmp(line, "[ASTD_", 6)) {
            s->bgm = sa_realloc(s->bgm, sizeof(BgmDef) * (size_t)(s->bgmCount + 1));
            cur = &s->bgm[s->bgmCount++];
            memset(cur, 0, sizeof *cur);
            cur->id = atoi(line + 6);
            continue;
        }
        char *eq = strchr(line, '=');
        if (!cur || !eq) continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        for (char *q = eq - 1; q >= k && (*q == ' ' || *q == '\t'); q--) *q = 0;
        while (*v == ' ' || *v == '\t') v++;
        if (sa_ieq(k, "File")) snprintf(cur->file, sizeof cur->file, "%s", v);
        else if (sa_ieq(k, "LoopTo")) cur->loopTo = asd_time(v);
        else if (sa_ieq(k, "LoopFrom")) cur->loopFrom = asd_time(v);
    }
    free(d);
    sa_log(s, "%d BGM entries", s->bgmCount);
}

static void play_bgm(SA *s, int id) {
    for (int i = 0; i < s->bgmCount; i++)
        if (s->bgm[i].id == id) {
            sa_play_loop(s->mix, CH_BGM, 0, s->bgm[i].file, s->bgm[i].loopTo, s->bgm[i].loopFrom);
            return;
        }
    sa_log(s, "no BGM %d", id);
}

static void compose(SA *s);

static u32 transition_ms(int kind) { return kind == 10 ? 500 : kind == 11 || kind == 12 ? 300 : 0; }

static void begin_transition(SA *s, VM *vm, int kind) {
    u32 ms = transition_ms(kind);
    if (!ms) return;
    if (s->dirty) compose(s);
    memcpy(s->from, s->fb, (size_t)SA_W * SA_H * 4);
    s->transDur = ms;
    s->transMs = 0;
    vm->state = VM_WAIT_TIME;
    s->waitMs = ms;
}

static int64_t movie_read(void *user, uint64_t offset, void *buffer, size_t length) {
    FILE *f = user;
    if (fseeko(f, (off_t)offset, SEEK_SET)) return -1;
    return (int64_t)fread(buffer, 1, length, f);
}

static void movie_stop(SA *s) {
    pthread_mutex_lock(&s->movieLock);
    void *m = s->movie;
    s->movie = NULL;
    pthread_mutex_unlock(&s->movieLock);
    if (m) s->movies.close(m);
    if (s->movieFile) fclose(s->movieFile);
    s->movieFile = NULL;
    if (s->vm.state == VM_WAIT_TIME) s->vm.state = VM_RUN;
    s->dirty = true;
}

static void movie_start(SA *s, VM *vm, const char *name) {
    if (!s->haveMovies) {
        sa_log(s, "movie %s left out: no movie decoder", name);
        return;
    }
    char *p = sa_fmt("%s/%s", s->gameDir, name);
    char real[2048];
    FILE *f = sa_path_resolve(p, real, sizeof real) == 1 ? fopen(real, "rb") : NULL;
    free(p);
    if (!f) {
        sa_log(s, "missing movie %s", name);
        return;
    }
    fseeko(f, 0, SEEK_END);
    uint64_t size = (uint64_t)ftello(f);
    VNSystemAoiMovieInfo info = {0};
    char err[256] = "";
    void *m = s->movies.open(movie_read, f, size, 0, &info, err, sizeof err);
    if (!m) {
        sa_log(s, "can't play movie %s: %s", name, err);
        fclose(f);
        return;
    }
    sa_log(s, "movie %s %dx%d %.2f fps %.1f s, sound %d Hz", name, info.width, info.height, info.fps, info.duration, info.audioRate);
    sa_stop(s->mix, CH_BGM, 0, 0);
    s->movieFile = f;
    s->movieTime = 0;
    s->movieW = info.width;
    s->movieH = info.height;
    pthread_mutex_lock(&s->movieLock);
    s->movie = m;
    pthread_mutex_unlock(&s->movieLock);
    vm->state = VM_WAIT_TIME;
    s->waitMs = 0;
    s->waitClick = false;
}

static void movie_tick(SA *s, u32 ms) {
    s->movieTime += ms / 1000.0;
    int changed = 0, ended = 0;
    const uint8_t *px = s->movies.frame(s->movie, s->movieTime, &changed, &ended);
    if (px && changed) {
        u32 *fb = s->fb;
        for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
        int ox = (SA_W - s->movieW) / 2, oy = (SA_H - s->movieH) / 2;
        for (int y = 0; y < s->movieH; y++) {
            int dy = oy + y;
            if (dy < 0 || dy >= SA_H) continue;
            for (int x = 0; x < s->movieW; x++) {
                int dx = ox + x;
                if (dx < 0 || dx >= SA_W) continue;
                const uint8_t *q = px + ((size_t)y * s->movieW + x) * 4;
                fb[(size_t)dy * SA_W + dx] = 0xff000000u | (u32)q[2] << 16 | (u32)q[1] << 8 | q[0];
            }
        }
        s->serial++;
    }
    if (ended) movie_stop(s);
}

static char *opd_str(const u8 *code, const Insn *in, int i) {
    if (i >= in->count || in->o[i].kind != OPD_STR) return sa_strdup("");
    return sa_utf16_to_utf8(code + in->o[i].strOffset, in->o[i].strLen);
}

static void host_command(void *ctx, const u8 *code, const Insn *in, VM *vm) {
    SA *s = ctx;
    switch (in->op) {
    case 0x4c: {
        char *n = opd_str(code, in, 0);
        begin_transition(s, vm, in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 0);
        picture_set(s, &s->bg, n, 0, 0);
        s->bgX = s->bgY = 0;
        s->panDur = 0;
        for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
        free(n);
        break;
    }
    case 0x52: {
        char *n = opd_str(code, in, 0);
        int x = in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 4;
        int y = in->count > 2 ? sa_vm_get(vm, code, &in->o[2]) : 4;
        begin_transition(s, vm, in->count > 3 ? sa_vm_get(vm, code, &in->o[3]) : 0);
        if (x >= 0 && x < STD_SLOTS) picture_set(s, &s->std[x], n, x, y);
        int fi = face_index_by_prefix(s, n);
        if (fi >= 0 && strlen(n) >= 8) snprintf(s->faceExpr[fi], 4, "%.3s", n + 5);
        free(n);
        break;
    }
    case 0x56: {
        int x = in->count > 0 ? sa_vm_get(vm, code, &in->o[0]) : -1;
        begin_transition(s, vm, in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 0);
        for (int i = 0; i < STD_SLOTS; i++)
            if (x < 0 || x == i) picture_clear(&s->std[i]);
        s->dirty = true;
        break;
    }
    case 0x41:
        if (in->count > 0) play_bgm(s, sa_vm_get(vm, code, &in->o[0]));
        break;
    case 0x49: {
        char *n = opd_str(code, in, 0);
        movie_start(s, vm, n);
        free(n);
        break;
    }
    case 0x42:
        break;
    case 0xf5: {
        begin_transition(s, vm, 11);
        picture_clear(&s->bg);
        s->bg.img = sa_image_new(SA_W, SA_H);
        sa_town_draw_map(&s->town, &s->vfs, vm, s->bg.img->px);
        snprintf(s->bg.name, sizeof s->bg.name, "*town");
        s->bgX = s->bgY = 0;
        s->panDur = 0;
        for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
        s->dirty = true;
        break;
    }
    case 0x50:
        if (in->count >= 3 && sa_vm_get(vm, code, &in->o[2]) == 0) {
            s->panFromX = s->bgX;
            s->panFromY = s->bgY;
            s->panToX = sa_vm_get(vm, code, &in->o[0]);
            s->panToY = sa_vm_get(vm, code, &in->o[1]);
            int dx = abs(s->panToX - s->panFromX), dy = abs(s->panToY - s->panFromY);
            s->panMs = 0;
            s->panDur = (u32)(dx > dy ? dx : dy) * PAN_MS_PER_PX;
            if (!s->panDur) {
                s->bgX = s->panToX;
                s->bgY = s->panToY;
            }
            s->dirty = true;
        } else if (s->panDur) {
            vm->state = VM_WAIT_TIME;
            s->waitMs = s->panDur - s->panMs;
            s->waitClick = false;
            s->panWait = true;
        }
        break;
    case 0x8a: {
        int gain[4] = {0, 0, 0, 0}, before[4], after[4];
        bool any = false;
        for (int i = 0; i < 4 && i < in->count; i++) {
            gain[i] = sa_vm_get(vm, code, &in->o[i]);
            any = any || gain[i];
        }
        int base = s->exp.active ? 0xd2 : C_RESIDENTS;
        for (int i = 0; i < 4; i++) before[i] = vm->c[base + i];
        if (s->exp.active) sa_exp_gain(s, gain);
        else
            for (int i = 0; i < 4; i++) vm->c[base + i] += gain[i];
        for (int i = 0; i < 4; i++) after[i] = vm->c[base + i];
        char *raw = opd_str(code, in, 4), *t = sa_vm_expand(vm, raw);
        free(raw);
        clear_text(s);
        if (any) sa_dialog_open_turn(&s->dialog, before, after, t);
        else sa_dialog_open(&s->dialog, t);
        free(t);
        for (int i = 0; i < s->joinedCount; i++) {
            int r = -1;
            for (int k = 0; k < s->party.units.rows && r < 0; k++)
                if (!strcmp(sa_table_text(&s->party.units, k, 1), s->joined[i])) r = k;
            if (r < 0) continue;
            char name[24];
            snprintf(name, sizeof name, "u%04da2.agf", sa_table_int(&s->party.units, r, 11));
            Image *im = sa_load_image(&s->vfs, name);
            if (!im) {
                snprintf(name, sizeof name, "u%04da0.agf", sa_table_int(&s->party.units, r, 11));
                im = sa_load_image(&s->vfs, name);
            }
            sa_dialog_add_unit(&s->dialog, im, sa_table_text(&s->party.units, r, 2));
        }
        s->joinedCount = 0;
        vm->state = VM_WAIT_DIALOG;
        s->dirty = true;
        break;
    }
    case 0x89:
        if (s->exp.active) {
            begin_transition(s, vm, in->count > 3 ? sa_vm_get(vm, code, &in->o[3]) : 10);
            clear_text(s);
            s->name[0] = 0;
            sa_exp_draw_view(s, s->fb);
            sa_show_frame_as_bg(s, s->fb, "*dungeon");
        }
        break;
    case 0xbf:
        if (in->count >= 2) sa_vm_set(vm, &in->o[0], sa_exp_floors_reached(s, sa_vm_get(vm, code, &in->o[1]) + 1));
        break;
    case 0xf3:
        if (in->count >= 5) {
            char *name = in->o[0].kind == OPD_STR ? opd_str(code, in, 0) : NULL;
            int count = 0, level = 0, total = 0;
            for (int c = 0; c < TOWN_CELLS; c++) {
                const Facility *f = &s->town.cell[c];
                int r = f->type ? sa_town_type_row(&s->town, f->type) : -1;
                if (r < 0) continue;
                total++;
                if (name && strcmp(sa_table_text(&s->town.build, r, 1), name)) continue;
                count++;
                if (f->level > level) level = f->level;
            }
            for (int i = 1; i < in->count; i++) {
                if (in->o[i].kind != OPD_VAR) continue;
                sa_vm_set(vm, &in->o[i], i == 3 ? count : i == 4 ? level : i == 1 ? total : 0);
            }
            free(name);
        }
        break;
    case 0xf1:
        if (in->count >= 1) {
            s->town.stage = sa_vm_get(vm, code, &in->o[0]);
            vm->c[C_TOWN_STAGE] = s->town.stage;
            s->dirty = true;
        }
        break;
    case 0xf7:
        if (in->count >= 1) {
            int loss = sa_vm_get(vm, code, &in->o[0]);
            for (int c = 0; c < TOWN_CELLS; c++)
                if (s->town.cell[c].type) s->town.cell[c].exp = s->town.cell[c].exp > loss ? s->town.cell[c].exp - loss : 0;
        }
        break;
    case 0xee:
        if (in->count >= 2) {
            char *name = opd_str(code, in, 0);
            int cell = sa_vm_get(vm, code, &in->o[1]);
            for (int r = 0; r < s->town.build.rows; r++)
                if (!strcmp(sa_table_text(&s->town.build, r, 1), name) && cell >= 0 && cell < TOWN_CELLS) {
                    s->town.cell[cell].type = sa_table_int(&s->town.build, r, 0);
                    s->town.cell[cell].level = 1;
                    s->town.cell[cell].exp = 0;
                    break;
                }
            free(name);
            s->dirty = true;
        }
        break;
    case 0xc9:
        if (in->count >= 2) {
            char *key = opd_str(code, in, 1);
            int level = 0;
            for (int i = 0; i < PARTY_SLOTS; i++) {
                int r = s->party.unit[i] ? sa_party_unit_row(&s->party, s->party.unit[i]) : -1;
                if (r >= 0 && !strcmp(sa_table_text(&s->party.units, r, 1), key)) level = s->party.level[i];
            }
            sa_vm_set(vm, &in->o[0], level);
            free(key);
        }
        break;
    case 0x9e:
        if (in->count >= 1) sa_vm_set(vm, &in->o[0], sa_mission_available(s));
        break;
    case 0x9f:
        if (in->count >= 1) sa_vm_set(vm, &in->o[0], sa_mission_start(s));
        break;
    case 0xa0:
        sa_mission_check(s);
        break;
    case 0xc3:
        if (in->count >= 3) {
            char *name = opd_str(code, in, 0);
            int unit = s->party.post[sa_party_post_index(&s->party, name)];
            free(name);
            sa_vm_set(vm, &in->o[1], unit != 0);
            sa_vm_set(vm, &in->o[2], unit);
        }
        break;
    case 0xbc:
        if (in->count >= 2) {
            char *key = opd_str(code, in, 1);
            if (sa_party_join(&s->party, &s->vfs, key)) {
                sa_log(s, "joined: %s", key);
                if (s->joinedCount < DIALOG_UNITS) snprintf(s->joined[s->joinedCount++], sizeof s->joined[0], "%s", key);
            }
            else sa_log(s, "no unit %s to join", key);
            free(key);
            s->dirty = true;
        }
        break;
    case 0x94:
        if (in->count >= 1) sa_party_add_item(&s->party, sa_vm_get(vm, code, &in->o[0]), in->count > 1 ? sa_vm_get(vm, code, &in->o[1]) : 1);
        break;
    case 0x96:
        if (in->count >= 2) sa_vm_set(vm, &in->o[0], sa_party_item_count(&s->party, sa_vm_get(vm, code, &in->o[1])));
        break;
    case 0xad:
        if (in->count >= 1) {
            char *k = in->count > 2 && in->o[2].kind == OPD_STR ? opd_str(code, in, 2) : NULL;
            sa_vm_set(vm, &in->o[0], sa_party_count_units(&s->party, k));
            free(k);
        }
        break;
    case 0xde:
        if (in->count >= 1) sa_vm_set(vm, &in->o[0], sa_exp_area_registered(s));
        break;
    case 0xa1:
        sa_mission_end(s, in->count >= 1 ? sa_vm_get(vm, code, &in->o[0]) : 0);
        break;
    case 0x01:
    case 0xdf:
    case 0xe1:
        break;
    case 0xd7:
        if (in->count >= 5) {
            char *f = opd_str(code, in, 0);
            sa_dungeon_set(&s->dungeon, f, sa_vm_get(vm, code, &in->o[1]), sa_vm_get(vm, code, &in->o[2]), sa_vm_get(vm, code, &in->o[3]),
                           sa_vm_get(vm, code, &in->o[4]), in->count > 5 ? sa_vm_get(vm, code, &in->o[5]) : 0);
            free(f);
            s->dirty = true;
        }
        break;
    case 0xd8:
        if (in->count >= 5) {
            char *f = opd_str(code, in, 0);
            sa_vm_set(vm, &in->o[1], sa_dungeon_get(&s->dungeon, f, sa_vm_get(vm, code, &in->o[2]), sa_vm_get(vm, code, &in->o[3]),
                                                    sa_vm_get(vm, code, &in->o[4])));
            free(f);
        }
        break;
    case 0xa8: {
        char *g = opd_str(code, in, 0);
        if (in->count >= 3) sa_exp_add_group_at(s, g, sa_vm_get(vm, code, &in->o[1]), sa_vm_get(vm, code, &in->o[2]));
        else sa_exp_add_group(s, g);
        free(g);
        break;
    }
    case 0xef:
        if (in->count >= 2) {
            int c = s->town.condition + sa_vm_get(vm, code, &in->o[1]);
            s->town.condition = c < 0 ? 0 : c > 100 ? 100 : c;
        }
        break;
    case 0xa4:
        s->missions.confirm = true;
        s->missions.hover = 0;
        vm->state = VM_WAIT_DIALOG;
        s->dirty = true;
        break;
    case 0x7a:
        s->gameOver = true;
        vm->depth = 0;
        vm->state = VM_DONE;
        break;
    case 0x2a: {
        int n = in->count > 0 ? sa_vm_get(vm, code, &in->o[0]) : 0;
        if (n < 0) {
            s->waitClick = true;
            vm->state = VM_WAIT_TIME;
            s->waitMs = 0;
        } else if (n > 0) {
            vm->state = VM_WAIT_TIME;
            s->waitMs = (u32)n * 1000 / 60;
        }
        break;
    }
    default: {
        static u32 seen[8];
        u32 bit = 1u << (in->op & 31);
        if (!(seen[in->op >> 5] & bit)) {
            seen[in->op >> 5] |= bit;
            char *t = sa_insn_text(code, in);
            sa_log(s, "not handled: %s", t + 8);
            free(t);
        }
        break;
    }
    }
}





enum { MSG_X = 0, MSG_Y = 546, TEXT_X = 324, TEXT_Y = 589, TEXT_SIZE = 28, TEXT_PITCH = 31, NAME_X = 305, NAME_Y = 553 };
enum { CHOICE_W = 504, CHOICE_H = 50, CHOICE_EDGE = 22, FEATHER_X = 1046, FEATHER_Y = 679 };

static int choice_y(SA *s, int i) {
    int n = s->vm.choiceCount;
    int top = (MSG_Y - n * CHOICE_H - 2 * CHOICE_EDGE) / 2 + CHOICE_EDGE;
    return top + i * CHOICE_H;
}

static int button_at(int x, int y) {
    for (int i = 0; i < BTN_COUNT; i++)
        if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w && y >= buttons[i].y && y < buttons[i].y + buttons[i].h) return i;
    return -1;
}

static bool window_shown(SA *s) {
    return (s->lineCount || s->name[0]) && !s->windowHidden && !s->log.open && !s->dialog.open;
}

static void compose(SA *s) {
    u32 *fb = s->fb;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    sa_blit(fb, s->bg.img, s->bgX, s->bgY);
    for (int i = 0; i < STD_SLOTS; i++) {
        Picture *p = &s->std[i];
        if (!p->img) continue;
        int cx = p->xslot * SA_W / 8;
        sa_blit(fb, p->img, cx - p->img->w / 2, SA_H - p->img->h);
    }
    if (window_shown(s)) {
        sa_blit_alpha(fb, s->msgBand, MSG_X, MSG_Y, 160);
        sa_blit(fb, s->msgFrame, MSG_X, MSG_Y);
        sa_blit_rect(fb, s->ui[UI_LEAF], 0, 0, 44, 41, 1044, 536);
        for (int i = 0; i < BTN_COUNT; i++) {
            const Button *b = &buttons[i];
            int col = (i == BTN_AUTO && s->autoMode) || (i == BTN_SKIP && s->skipMode) ? 1 : 0;
            sa_blit_rect(fb, s->ui[b->ui], col * b->w, i == s->hoverButton ? b->h : 0, b->w, b->h, b->x, b->y);
        }
        if (s->vm.state == VM_WAIT_CLICK && s->shown >= s->total) {
            int f = (int)(s->featherMs / 33) % 31;
            sa_blit_rect(fb, s->ui[UI_FEATHER], f * 32, 0, 32, 32, FEATHER_X, FEATHER_Y);
        }
        sa_blit(fb, s->face.img, -1, 294);
        if (s->name[0]) sa_text_draw(fb, SA_W, SA_H, s->name, -1, NAME_X, NAME_Y, TEXT_SIZE, 0xffffff, 0x000000);
        int left = s->shown;
        for (int i = 0; i < s->lineCount && left > 0; i++) {
            int n = utf8_len(s->lines[i]);
            sa_text_draw(fb, SA_W, SA_H, s->lines[i], n < left ? n : left, TEXT_X, TEXT_Y + i * TEXT_PITCH, TEXT_SIZE, 0xffffff, 0x000000);
            left -= n;
        }
    }
    if (s->vm.state == VM_WAIT_CHOICE) {
        int x = (SA_W - CHOICE_W) / 2;
        sa_blit(fb, s->ui[UI_CHOICE_TOP], x, choice_y(s, 0) - CHOICE_EDGE);
        for (int i = 0; i < s->vm.choiceCount; i++) {
            int y = choice_y(s, i);
            sa_blit_rect(fb, s->ui[UI_CHOICE_ROW], 0, i == s->hover ? CHOICE_H : 0, CHOICE_W, CHOICE_H, x, y);
            const char *t = s->vm.choice[i].label;
            int w = sa_text_width(t, -1, TEXT_SIZE);
            sa_text_draw(fb, SA_W, SA_H, t, -1, x + (CHOICE_W - w) / 2, y + (CHOICE_H - TEXT_SIZE) / 2, TEXT_SIZE, 0x302010, 0xffffffffu);
        }
        sa_blit(fb, s->ui[UI_CHOICE_BOTTOM], x, choice_y(s, s->vm.choiceCount));
    }
    sa_dialog_draw(&s->dialog, fb);
    sa_mission_draw_confirm(s, fb);
    if (s->log.open) sa_log_draw(&s->log, fb, s->skipMode, s->autoMode);
    if (s->transDur && s->transMs < s->transDur) {
        u32 a = s->transMs * 256 / s->transDur;
        for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) {
            u32 p = fb[i], q = s->from[i];
            u32 rb = (((p & 0xff00ff) * a + (q & 0xff00ff) * (256 - a)) >> 8) & 0xff00ff;
            u32 g = (((p & 0xff00) * a + (q & 0xff00) * (256 - a)) >> 8) & 0xff00;
            fb[i] = 0xff000000u | rb | g;
        }
    }
    s->serial++;
}

static int title_at(int x, int y) {
    if (x < TITLE_X || x >= TITLE_X + TITLE_W || y < TITLE_Y) return -1;
    int i = (y - TITLE_Y) / TITLE_H;
    return i < TITLE_ITEMS ? i : -1;
}

static void compose_title(SA *s) {
    u32 *fb = s->fb;
    for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) fb[i] = 0xff000000u;
    sa_blit(fb, s->titleBg, 0, 0);
    for (int i = 0; i < TITLE_ITEMS; i++)
        sa_blit_rect(fb, s->titleItem[i], 0, i == s->titleHover ? TITLE_H : 0, TITLE_W, TITLE_H, TITLE_X, TITLE_Y + i * TITLE_H);
    s->serial++;
}

void sa_reset_scene(SA *s) {
    picture_clear(&s->bg);
    s->bgX = s->bgY = 0;
    s->panDur = 0;
    picture_clear(&s->face);
    for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
    clear_text(s);
    s->name[0] = 0;
    picture_clear(&s->face);
    s->windowHidden = s->autoMode = s->skipMode = false;
    s->transDur = 0;
    s->waitClick = false;
}

bool sa_start_script(SA *s, const char *box, int entry) {
    s->vm.depth = 0;
    s->vm.choiceCount = 0;
    s->mode = MODE_STORY;
    if (sa_vm_start(&s->vm, &s->vfs, box, entry, &s->host)) return true;
    sa_log(s, "can't start %s #%d", box, entry);
    return false;
}

static void go_title(SA *s) {
    sa_reset_scene(s);
    s->inGame = false;
    sa_stop(s->mix, CH_VOICE, 0, 0);
    sa_stop(s->mix, CH_BGM, 0, 500);
    s->mode = MODE_TITLE;
    s->titleHover = -1;
    s->dirty = true;
}

static void new_game(SA *s) {
    sa_reset_scene(s);
    memset(s->vm.c, 0, sizeof s->vm.c);
    memset(s->vm.f, 0, sizeof s->vm.f);
    snprintf(s->nextBox, sizeof s->nextBox, "bo0000.box");
    s->nextEntry = 0;
    sa_town_new_game(&s->town);
    s->vm.c[C_TOWN_STAGE] = s->town.stage;
    s->vm.c[C_TURN] = 1;
    sa_party_new_game(&s->party, &s->vfs);
    s->inGame = true;
    s->playMs = 0;
    if (s->exp.explored) memset(s->exp.explored, 0, sizeof(*s->exp.explored) * EXP_DUNGEONS);
    if (s->exp.passages) memset(s->exp.passages, 0, sizeof(*s->exp.passages) * EXP_DUNGEONS);
    if (s->exp.floorInfo) memset(s->exp.floorInfo, 0, sizeof(*s->exp.floorInfo) * EXP_DUNGEONS);
    static const int noPlace[4] = {0, -1, -1, 0};
    memcpy(s->exp.last, noPlace, sizeof noPlace);
    memset(s->exp.entry, 0, sizeof s->exp.entry);
    s->exp.groupCount = 0;
    sa_mission_reset(s);
    sa_start_script(s, "init.box", 0);
}

void sa_end_turn(SA *s) {
    VM *vm = &s->vm;
    int food = 0, money = 0, res = 0, people = 0, cap = 0;
    for (int c = 0; c < TOWN_CELLS; c++) {
        int type = s->town.cell[c].type;
        int r = type ? sa_town_type_row(&s->town, type) : -1;
        if (r < 0) continue;
        cap += sa_table_int(&s->town.build, r, 9);
        if (!sa_town_staffed(&s->town, vm, c)) continue;
        food += sa_table_int(&s->town.build, r, 25);
        money += sa_table_int(&s->town.build, r, 27);
        res += sa_table_int(&s->town.build, r, 29) - sa_table_int(&s->town.build, r, 34);
        int lo = sa_table_int(&s->town.build, r, 23), hi = sa_table_int(&s->town.build, r, 24);
        people += hi > lo ? lo + (int)(sa_vm_random(vm) % (u32)(hi - lo + 1)) : hi;
    }
    food -= vm->c[C_RESIDENTS] / 10;
    sa_log(s, "turn %d ends: residents %d%+d, food %d%+d, money %d%+d, resources %d%+d", vm->c[C_TURN], vm->c[C_RESIDENTS], people,
           vm->c[C_FOOD], food, vm->c[C_MONEY], money, vm->c[C_RESOURCES], res);
    int before[4] = {vm->c[C_RESIDENTS], vm->c[C_FOOD], vm->c[C_MONEY], vm->c[C_RESOURCES]};
    vm->c[C_RESIDENTS] += people;
    if (cap && vm->c[C_RESIDENTS] > cap) vm->c[C_RESIDENTS] = cap;
    vm->c[C_FOOD] += food;
    vm->c[C_MONEY] += money;
    vm->c[C_RESOURCES] += res;
    int after[4] = {vm->c[C_RESIDENTS], vm->c[C_FOOD], vm->c[C_MONEY], vm->c[C_RESOURCES]};
    sa_dialog_open_turn(&s->dialog, before, after, "このターンの収支です");
    s->turnPending = true;
    s->town.mapOnly = true;
}

static void next_turn(SA *s) {
    s->turnPending = false;
    s->autosaveTurn = true;
    s->town.mapOnly = false;
    sa_reset_scene(s);
    if (s->vm.boxSlot[TOWN_HOOK]) {
        char box[32];
        snprintf(box, sizeof box, "%s", s->vm.boxSlot[TOWN_HOOK]);
        sa_town_draw_map(&s->town, &s->vfs, &s->vm, s->fb);
        for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) s->fb[i] = 0xff000000u | (s->fb[i] >> 1 & 0x7f7f7f);
        sa_show_frame_as_bg(s, s->fb, "*town");
        snprintf(s->nextBox, sizeof s->nextBox, "bo0000.box");
        s->nextEntry = 0;
        s->turnAdvance = true;
        sa_start_script(s, box, 0);
        return;
    }
    s->vm.c[C_TURN]++;
    sa_start_script(s, "bo0000.box", 0);
}

static bool dialog_input(SA *s, u32 ms) {
    if (!s->dialog.open) return false;
    if (sa_dialog_tick(&s->dialog, ms)) s->dirty = true;
    if (sa_dialog_hover(&s->dialog, s->mouseX, s->mouseY)) s->dirty = true;
    s->rightClicked = false;
    if (s->clicked) {
        s->clicked = false;
        if (sa_dialog_click(&s->dialog, s->mouseX, s->mouseY)) {
            if (s->vm.state == VM_WAIT_DIALOG) s->vm.state = VM_RUN;
            if (s->endTurnAfterDialog) {
                s->endTurnAfterDialog = false;
                sa_end_turn(s);
            } else if (s->turnPending)
                next_turn(s);
            s->dirty = true;
        }
    }
    return true;
}

static int command_number(int action) {
    switch (action) {
    case TOWN_CMD_EXPEDITION: return 20;
    case TOWN_CMD_POLICY: return 21;
    case TOWN_CMD_REST: return 22;
    case TOWN_CMD_BUILD: return 23;
    case TOWN_CMD_DEPLOY: return 24;
    case TOWN_CMD_EQUIP: return 25;
    case TOWN_CMD_TRAIN: return 26;
    case TOWN_CMD_SUBMIT: return 29;
    case TOWN_CMD_SHOP: return 27;
    }
    return -1;
}

static void run_command(SA *s, int action) {
    switch (action) {
    case TOWN_CMD_REST:
        sa_end_turn(s);
        break;
    case TOWN_CMD_EXPEDITION:
        sa_exp_open_select(s);
        break;
    case TOWN_CMD_POLICY:
        sa_policy_open(s);
        break;
    case TOWN_CMD_EQUIP:
        sa_equip_open(s);
        break;
    case TOWN_CMD_SHOP:
        sa_shop_open(s);
        break;
    case TOWN_CMD_TRAIN:
        sa_train_open(s);
        break;
    case TOWN_CMD_DEPLOY:
        sa_deploy_open(s);
        break;
    case TOWN_CMD_SUBMIT:
        sa_mission_open(s);
        break;
    case TOWN_POLICY_DONE:
        sa_end_turn(s);
        break;
    case TOWN_CMD_BUILD:
        sa_build_open(&s->build);
        s->town.mapOnly = true;
        break;
    default:
        sa_log(s, "town command %d not built yet", action);
        break;
    }
    s->dirty = true;
}

static void town_action(SA *s, int action) {
    int number = command_number(action);
    if (number >= 0) {
        s->vm.c[0x5f] = number;
        s->afterScript = action;
        sa_reset_scene(s);
        sa_start_script(s, "bo3000.box", 0);
        s->dirty = true;
        return;
    }
    switch (action) {
    case TOWN_BTN_HIDE:
        s->town.hidden = !s->town.hidden;
        break;
    case TOWN_BTN_SAVE:
    case TOWN_BTN_LOAD:
        sa_savescreen_open(s, action == TOWN_BTN_SAVE);
        break;
    case TOWN_NONE:
        if (s->town.hidden) s->town.hidden = false;
        break;
    default:
        sa_log(s, "town action %d not built yet", action);
        break;
    }
    s->dirty = true;
}

static void title_pick(SA *s, int item) {
    switch (item) {
    case TITLE_PROLOGUE:
        sa_reset_scene(s);
        s->nextBox[0] = 0;
        sa_start_script(s, "pro.box", 0);
        break;
    case TITLE_START:
        new_game(s);
        break;
    case TITLE_END:
        s->quit = true;
        break;
    case TITLE_LOAD:
        sa_savescreen_open(s, false);
        break;
    default:
        sa_log(s, "title item %d not built yet", item);
        break;
    }
    s->dirty = true;
}

static int choice_at(SA *s, int x, int y) {
    if (s->vm.state != VM_WAIT_CHOICE) return -1;
    int cx = (SA_W - CHOICE_W) / 2;
    for (int i = 0; i < s->vm.choiceCount; i++) {
        int cy = choice_y(s, i);
        if (x >= cx && x < cx + CHOICE_W && y >= cy && y < cy + CHOICE_H) return i;
    }
    return -1;
}

const VNSystemAoiMovies *vnsystemaoi_find_movies(void) {
    static VNSystemAoiMovies movies;
    static bool found = false;
    if (found) return &movies;
    const char *names[5] = {"vnffmovie_open", "vnffmovie_frame", "vnffmovie_audio", "vnffmovie_seek", "vnffmovie_close"};
    void *ptrs[5];
    for (int i = 0; i < 5; i++)
        if (!(ptrs[i] = dlsym(RTLD_DEFAULT, names[i]))) return NULL;
    movies.open = (void *(*)(VNSystemAoiMovieRead, void *, uint64_t, int, VNSystemAoiMovieInfo *, char *, size_t))ptrs[0];
    movies.frame = (const uint8_t *(*)(void *, double, int *, int *))ptrs[1];
    movies.audio = (size_t(*)(void *, float *, size_t))ptrs[2];
    movies.seek = (void (*)(void *, double))ptrs[3];
    movies.close = (void (*)(void *))ptrs[4];
    found = true;
    return &movies;
}

bool vnsystemaoi_probe(const char *gameDir, char *title, size_t titleSize) {
    char *p = sa_fmt("%s/box.vfs", gameDir);
    char real[2048];
    bool ok = sa_path_resolve(p, real, sizeof real) == 1;
    free(p);
    if (!ok) return false;
    Vfs v;
    if (!sa_vfs_open(&v, gameDir)) return false;
    ok = sa_vfs_find(&v, "init.box") && sa_vfs_find(&v, "pro.box");
    sa_vfs_close(&v);
    if (ok && title) snprintf(title, titleSize, "BUNNY BLACK 3");
    return ok;
}

VNSystemAoi *vnsystemaoi_open(const VNSystemAoiConfig *config, char *err, size_t errSize) {
    SA *s = sa_alloc(sizeof *s);
    s->cfg = *config;
    s->gameDir = sa_strdup(config->gameDir);
    s->saveDir = sa_strdup(config->saveDir ? config->saveDir : config->gameDir);
    if (config->logPath) s->logFile = fopen(config->logPath, "w");
    if (!sa_vfs_open(&s->vfs, s->gameDir)) {
        if (err) snprintf(err, errSize, "no SystemAoi archives in the game folder");
        vnsystemaoi_close((VNSystemAoi *)s);
        return NULL;
    }
    sa_log(s, "%d archive entries", s->vfs.count);
    static const char *const fonts[] = {"MS Gothic", "MS PGothic", NULL};
    char used[1024] = "";
    if (sa_font_init(fonts, used, sizeof used)) sa_log(s, "font %s", used);
    else sa_log(s, "no font");
    sa_font_set_spacing(1);
    s->mix = sa_mixer_new(config->audioRate, &s->vfs);
    sa_mixer_set_log(s->mix, host_log, s);
    s->fb = sa_alloc((size_t)SA_W * SA_H * 4);
    s->msgFrame = load_image(s, "msg_a00.agf");
    s->msgBand = load_image(s, "msg_a00a.agf");
    for (int i = 0; i < UI_COUNT; i++) s->ui[i] = load_image(s, uiNames[i]);
    s->hoverButton = -1;
    pthread_mutex_init(&s->movieLock, NULL);
    if (config->movies) {
        s->movies = *config->movies;
        s->haveMovies = true;
    }
    s->from = sa_alloc((size_t)SA_W * SA_H * 4);
    load_bgm_defs(s);
    load_faces(s);
    sa_log_init(&s->log, &s->vfs);
    if (!sa_town_init(&s->town, &s->vfs)) sa_log(s, "can't read the town tables");
    if (!sa_party_init(&s->party, &s->vfs)) sa_log(s, "can't read the unit table");
    s->town.party = &s->party;
    if (!sa_battle_init(&s->battle, &s->vfs, &s->party)) sa_log(s, "can't read the battle tables");
    sa_build_init(&s->build, &s->vfs);
    sa_dialog_init(&s->dialog, &s->vfs);
    sa_exp_init(s);
    sa_policy_init(&s->policy, &s->vfs);
    sa_equip_init(&s->equip, &s->vfs);
    sa_shop_init(&s->shop, &s->vfs);
    sa_mission_init(s);
    sa_card_init(&s->card, &s->vfs);
    sa_train_init(&s->train, &s->vfs);
    sa_deploy_init(&s->deploy, &s->vfs);
    sa_savescreen_init(&s->saves, &s->vfs);
    snprintf(s->title, sizeof s->title, "BUNNY BLACK 3");
    s->host = (VMHost){.ctx = s, .text = host_text, .speaker = host_speaker, .page = host_page, .command = host_command, .log = host_log};
    s->hover = -1;
    s->vm.rng = config->seed ? config->seed : 0x2545f491u;
    s->titleBg = load_image(s, "tit_a00.agf");
    for (int i = 0; i < TITLE_ITEMS; i++) {
        char n[32];
        snprintf(n, sizeof n, "tit_a1%d.agf", i);
        s->titleItem[i] = load_image(s, n);
    }
    char *sav = sa_fmt("%s/Bunny3.sav", s->saveDir);
    char real[2048];
    bool first = sa_path_resolve(sav, real, sizeof real) != 1;
    if (first) sa_mkdirs(sav);
    free(sav);
    char *global = sa_fmt("%s/vndegen/global.dat", s->saveDir);
    size_t gn;
    u8 *g = sa_read_file(global, &gn);
    if (g && gn == sizeof s->vm.g) memcpy(s->vm.g, g, gn);
    free(g);
    free(global);
    go_title(s);
    if (first) {
        title_pick(s, TITLE_PROLOGUE);
        sa_log(s, "first start: prologue");
    }
    s->dirty = true;
    return (VNSystemAoi *)s;
}

void vnsystemaoi_close(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    if (!s) return;
    if (s->movie) movie_stop(s);
    sa_mixer_free(s->mix);
    sa_vm_free(&s->vm);
    sa_vfs_close(&s->vfs);
    picture_clear(&s->bg);
    for (int i = 0; i < STD_SLOTS; i++) picture_clear(&s->std[i]);
    sa_image_free(s->msgFrame);
    sa_image_free(s->msgBand);
    sa_image_free(s->titleBg);
    for (int i = 0; i < TITLE_ITEMS; i++) sa_image_free(s->titleItem[i]);
    for (int i = 0; i < UI_COUNT; i++) sa_image_free(s->ui[i]);
    free(s->from);
    free(s->bgm);
    sa_log_free(&s->log);
    sa_town_free(&s->town);
    sa_battle_free(&s->battle);
    sa_party_free(&s->party);
    sa_build_free(&s->build);
    sa_dialog_free(&s->dialog);
    sa_exp_free(s);
    sa_policy_free(&s->policy);
    sa_equip_free(&s->equip);
    sa_shop_free(&s->shop);
    sa_mission_free(s);
    sa_card_free(&s->card);
    sa_train_free(&s->train);
    sa_deploy_free(&s->deploy);
    sa_savescreen_free(&s->saves);
    sa_dungeon_free(&s->dungeon);
    r3_free(&s->dungeon.r3);
    picture_clear(&s->face);
    for (int i = 0; i < s->faceCount; i++) {
        free(s->faceName[i]);
        free(s->facePrefix[i]);
    }
    clear_text(s);
    sa_font_free();
    if (s->logFile) fclose(s->logFile);
    free(s->fb);
    free(s->gameDir);
    free(s->saveDir);
    free(s);
}

bool vnsystemaoi_frame(VNSystemAoi *sa, uint32_t elapsedMs) {
    SA *s = (SA *)sa;
    if (s->quit) return false;
    if (s->paused) return true;
    if (s->inGame) s->playMs += elapsedMs;
    if (s->vm.gDirty) {
        s->vm.gDirty = false;
        char *dir = sa_fmt("%s/vndegen", s->saveDir), *path = sa_fmt("%s/global.dat", dir);
        sa_mkdirs(dir);
        sa_write_file(path, s->vm.g, sizeof s->vm.g);
        free(dir);
        free(path);
    }
    if (s->movie) {
        if (s->clicked) {
            s->clicked = false;
            movie_stop(s);
        } else {
            movie_tick(s, elapsedMs);
            return true;
        }
    }
    if (s->mode == MODE_TITLE) {
        int h = title_at(s->mouseX, s->mouseY);
        if (h != s->titleHover) {
            s->titleHover = h;
            s->dirty = true;
        }
        if (s->clicked) {
            s->clicked = false;
            if (h >= 0) title_pick(s, h);
        }
        if (s->mode == MODE_TITLE) {
            if (s->dirty) {
                compose_title(s);
                s->dirty = false;
            }
            return !s->quit;
        }
    }
    if (s->mode == MODE_DUNGEON && dialog_input(s, elapsedMs)) {
        if (s->dirty) {
            sa_exp_draw_view(s, s->fb);
            sa_dialog_draw(&s->dialog, s->fb);
            s->serial++;
            s->dirty = false;
        }
        return true;
    }
    if (sa_exp_frame(s, elapsedMs)) return true;
    if (sa_policy_frame(s)) return true;
    if (sa_equip_frame(s)) return true;
    if (sa_shop_frame(s)) return true;
    if (sa_mission_frame(s)) return true;
    if (sa_train_frame(s)) return true;
    if (sa_deploy_frame(s, elapsedMs)) return true;
    if (sa_savescreen_frame(s)) return true;
    if (s->mode == MODE_BATTLE) {
        if (sa_battle_tick(&s->battle, elapsedMs)) s->dirty = true;
        if (sa_battle_hover(&s->battle, s->mouseX, s->mouseY)) s->dirty = true;
        if (s->rightClicked) {
            s->rightClicked = false;
            sa_battle_cancel(&s->battle);
            s->dirty = true;
        }
        if (s->clicked) {
            s->clicked = false;
            int result = sa_battle_click(&s->battle, s->mouseX, s->mouseY);
            if (result != BATTLE_RUNNING) sa_exp_battle_over(s, result);
            s->dirty = true;
        }
        if (s->dirty) {
            if (s->mode == MODE_BATTLE) sa_battle_draw(&s->battle, s->fb);
            else sa_town_draw(&s->town, &s->vfs, &s->vm, s->fb);
            s->serial++;
            s->dirty = false;
        }
        return true;
    }
    if (s->mode == MODE_TOWN && s->build.open) {
        if (sa_build_hover(&s->build, &s->town, &s->vm, s->mouseX, s->mouseY)) s->dirty = true;
        if (s->clicked || s->rightClicked) {
            int r = sa_build_click(&s->build, &s->town, &s->vm, s->mouseX, s->mouseY, s->rightClicked);
            s->clicked = s->rightClicked = false;
            if (r == BUILD_CLOSED) s->town.mapOnly = false;
            if (r == BUILD_DECIDED && s->vm.boxSlot[COMMAND_HOOK]) {
                char box[32];
                snprintf(box, sizeof box, "%s", s->vm.boxSlot[COMMAND_HOOK]);
                s->build.open = false;
                s->vm.c[0x5f] = 23;
                s->afterScript = TOWN_CMD_BUILD;
                sa_reset_scene(s);
                sa_start_script(s, box, 0);
                return true;
            }
            s->dirty = true;
        }
        if (s->dirty) {
            sa_town_draw(&s->town, &s->vfs, &s->vm, s->fb);
            if (s->build.open) sa_build_draw(&s->build, &s->town, &s->vfs, &s->vm, s->fb);
            s->serial++;
            s->dirty = false;
        }
        return true;
    }
    if (s->mode == MODE_TOWN && dialog_input(s, elapsedMs)) {
        if (s->mode == MODE_TOWN && s->dirty) {
            if (s->town.mapOnly) {
                sa_town_draw_map(&s->town, &s->vfs, &s->vm, s->fb);
                for (size_t i = 0; i < (size_t)SA_W * SA_H; i++) s->fb[i] = 0xff000000u | (s->fb[i] >> 1 & 0x7f7f7f);
            } else
                sa_town_draw(&s->town, &s->vfs, &s->vm, s->fb);
            sa_dialog_draw(&s->dialog, s->fb);
            s->serial++;
            s->dirty = false;
        }
        if (s->mode == MODE_TOWN) return true;
    }
    if (s->mode == MODE_TOWN) {
        if (sa_party_tick(&s->party, elapsedMs) && !s->town.hidden && !s->town.mapOnly) s->dirty = true;
        if (sa_town_hover(&s->town, &s->vm, s->mouseX, s->mouseY)) s->dirty = true;
        if (s->clicked) {
            s->clicked = false;
            town_action(s, sa_town_hit(&s->town, &s->vm, s->mouseX, s->mouseY));
        }
        if (s->mode == MODE_TOWN && s->dirty) {
            sa_town_draw(&s->town, &s->vfs, &s->vm, s->fb);
            s->serial++;
            s->dirty = false;
        }
        if (s->mode == MODE_TOWN || s->mode == MODE_SAVES) return true;
    }
    dialog_input(s, elapsedMs);
    if (sa_mission_confirm_input(s)) s->dirty = true;
    if (s->total && s->shown < s->total) {
        s->textMs += elapsedMs;
        int want = (int)(s->textMs / 30);
        if (want > s->total) want = s->total;
        if (want != s->shown) {
            s->shown = want;
            s->dirty = true;
        }
    }
    if (s->forceSkip && s->mode == MODE_STORY) {
        if (s->transDur) s->transMs = s->transDur;
        if (s->vm.state == VM_WAIT_TIME && !s->movie) s->waitMs = 0;
        if (s->vm.state == VM_WAIT_TIME && s->panWait) {
            s->bgX = s->panToX;
            s->bgY = s->panToY;
            s->panDur = 0;
            s->panWait = false;
            s->dirty = true;
            s->waitMs = 0;
            s->vm.state = VM_RUN;
        } else if (s->vm.state == VM_WAIT_TIME && s->waitClick) {
            s->waitClick = false;
            s->vm.state = VM_RUN;
        }
        if (s->vm.state == VM_WAIT_CLICK) {
            s->shown = s->total;
            s->clicked = true;
            s->mouseX = s->mouseY = -1;
        }
    }
    if (s->panDur) {
        s->panMs += s->forceSkip || s->skipMode ? elapsedMs * PAN_SKIP_SPEED : elapsedMs;
        u32 t = s->panMs < s->panDur ? s->panMs : s->panDur;
        s->bgX = s->panFromX + (int)((int64_t)(s->panToX - s->panFromX) * t / s->panDur);
        s->bgY = s->panFromY + (int)((int64_t)(s->panToY - s->panFromY) * t / s->panDur);
        if (t == s->panDur) {
            s->panDur = 0;
            if (s->panWait && s->vm.state == VM_WAIT_TIME) {
                s->waitMs = 0;
                s->vm.state = VM_RUN;
            }
            s->panWait = false;
        }
        s->dirty = true;
    }
    if (s->transDur) {
        s->transMs += elapsedMs;
        if (s->transMs >= s->transDur) s->transDur = 0;
        s->dirty = true;
    }
    if (s->vm.state == VM_WAIT_TIME && !s->waitClick) {
        if (s->waitMs > elapsedMs) s->waitMs -= elapsedMs;
        else {
            s->waitMs = 0;
            s->vm.state = VM_RUN;
        }
    }
    if (s->vm.state == VM_WAIT_CLICK && s->shown >= s->total) {
        s->featherMs += elapsedMs;
        s->dirty = true;
        bool voice = sa_playing(s->mix, CH_VOICE, 0);
        if (s->skipMode || (s->autoMode && !voice && (s->autoMs += elapsedMs) >= 1500)) {
            s->autoMs = 0;
            s->clicked = true;
            s->mouseX = s->mouseY = -1;
        }
    } else {
        s->featherMs = 0;
        s->autoMs = 0;
    }
    if (s->skipMode && s->shown < s->total) s->shown = s->total;
    if (s->log.open) {
        if (sa_log_hover(&s->log, s->mouseX, s->mouseY)) s->dirty = true;
        if (s->rightClicked) {
            s->rightClicked = false;
            s->log.open = false;
            s->dirty = true;
        }
        if (s->clicked) {
            s->clicked = false;
            char voice[40] = "";
            int act = sa_log_click(&s->log, s->mouseX, s->mouseY, voice, sizeof voice);
            if (act == LOG_ACT_VOICE) sa_play(s->mix, CH_VOICE, 0, voice, false);
            if (act == LOG_ACT_FAST || act == LOG_ACT_AUTO) {
                s->log.open = false;
                s->skipMode = act == LOG_ACT_FAST;
                s->autoMode = act == LOG_ACT_AUTO;
            }
            s->dirty = true;
        }
        if (s->dirty) {
            compose(s);
            s->dirty = false;
        }
        return true;
    }
    if (s->rightClicked) {
        s->rightClicked = false;
        if (s->windowHidden) s->windowHidden = false;
        else {
            s->skipMode = !s->skipMode;
            s->autoMode = false;
        }
        s->dirty = true;
    }
    int btn = window_shown(s) ? button_at(s->mouseX, s->mouseY) : -1;
    if (btn != s->hoverButton) {
        s->hoverButton = btn;
        s->dirty = true;
    }
    if (s->clicked && s->windowHidden) {
        s->clicked = false;
        s->windowHidden = false;
        s->dirty = true;
    }
    if (s->clicked && btn >= 0) {
        s->clicked = false;
        s->dirty = true;
        switch (btn) {
        case BTN_AUTO:
            s->autoMode = !s->autoMode;
            s->skipMode = false;
            break;
        case BTN_SKIP:
            s->skipMode = !s->skipMode;
            s->autoMode = false;
            break;
        case BTN_REPEAT:
            if (s->lastVoice[0]) sa_play(s->mix, CH_VOICE, 0, s->lastVoice, false);
            break;
        case BTN_CLOSE:
            s->windowHidden = true;
            break;
        case BTN_LOG:
            sa_log_open(&s->log);
            s->autoMode = s->skipMode = false;
            break;
        default:
            sa_log(s, "button %d not built yet", btn);
            break;
        }
    }
    if (s->clicked) {
        s->clicked = false;
        if (s->skipMode && s->mouseX >= 0) s->skipMode = false;
        if (s->vm.state == VM_WAIT_TIME && s->waitClick) {
            s->waitClick = false;
            s->vm.state = VM_RUN;
        } else if (s->vm.state == VM_WAIT_CHOICE) {
            int c = choice_at(s, s->mouseX, s->mouseY);
            if (c >= 0) {
                clear_text(s);
                s->name[0] = 0;
                picture_clear(&s->face);
                sa_vm_choose(&s->vm, c);
            }
        } else if (s->shown < s->total) {
            s->shown = s->total;
        } else if (s->vm.state == VM_WAIT_CLICK) {
            clear_text(s);
            s->name[0] = 0;
            picture_clear(&s->face);
            sa_stop(s->mix, CH_VOICE, 0, 0);
            sa_vm_click(&s->vm);
        }
        s->dirty = true;
    }
    if (s->vm.state == VM_RUN) sa_vm_run(&s->vm, &s->vfs, &s->host, 20000);
    if (s->vm.state == VM_DONE && !s->nextBox[0] && sa_exp_script_done(s)) return true;
    if (s->vm.state == VM_DONE) {
        if (s->nextBox[0]) {
            char box[32];
            snprintf(box, sizeof box, "%s", s->nextBox);
            s->nextBox[0] = 0;
            if (s->turnAdvance) {
                s->turnAdvance = false;
                s->vm.c[C_TURN]++;
                sa_reset_scene(s);
            }
            sa_start_script(s, box, s->nextEntry);
        } else {
            char *sav = sa_fmt("%s/Bunny3.sav", s->saveDir);
            sa_mkdirs(sav);
            free(sav);
            if (s->inGame && s->afterScript) {
                int a = s->afterScript;
                s->afterScript = 0;
                sa_reset_scene(s);
                s->mode = MODE_TOWN;
                s->town.hover = -1;
                run_command(s, a);
            } else if (s->gameOver) {
                s->gameOver = false;
                sa_log(s, "game over: back to the title");
                go_title(s);
            } else if (s->inGame) {
                sa_reset_scene(s);
                s->mode = MODE_TOWN;
                s->town.hover = -1;
                s->dirty = true;
                if (s->autosaveTurn) {
                    s->autosaveTurn = false;
                    sa_autosave(s);
                }
            } else {
                go_title(s);
            }
            return true;
        }
    }
    int h = choice_at(s, s->mouseX, s->mouseY);
    if (h != s->hover) {
        s->hover = h;
        s->dirty = true;
    }
    if (s->dirty) {
        compose(s);
        s->dirty = false;
    }
    return true;
}

const uint32_t *vnsystemaoi_pixels(VNSystemAoi *sa, int *width, int *height) {
    SA *s = (SA *)sa;
    *width = SA_W;
    *height = SA_H;
    return s->fb;
}

uint32_t vnsystemaoi_frame_serial(VNSystemAoi *sa) { return ((SA *)sa)->serial; }

void vnsystemaoi_mouse_move(VNSystemAoi *sa, int x, int y) {
    SA *s = (SA *)sa;
    s->mouseX = x;
    s->mouseY = y;
}

void vnsystemaoi_mouse_button(VNSystemAoi *sa, int button, bool down) {
    SA *s = (SA *)sa;
    if (button == VNSYSTEMAOI_MOUSE_LEFT && !down) s->clicked = true;
    if (button == VNSYSTEMAOI_MOUSE_RIGHT && !down) s->rightClicked = true;
}

void vnsystemaoi_key(VNSystemAoi *sa, int virtualKey, bool down) {
    SA *s = (SA *)sa;
    if (!down) return;
    if (s->mode == MODE_DUNGEON) {
        sa_exp_key(s, virtualKey);
        return;
    }
    if (virtualKey == 0x0d || virtualKey == 0x20) {
        if (s->vm.state == VM_WAIT_CHOICE && s->hover < 0) s->hover = 0;
        if (s->vm.state == VM_WAIT_CHOICE) {
            int c = s->hover;
            clear_text(s);
            s->name[0] = 0;
            picture_clear(&s->face);
            sa_vm_choose(&s->vm, c);
            s->dirty = true;
        } else {
            s->clicked = true;
            s->mouseX = s->mouseY = -1;
        }
    } else if (virtualKey == 0x26 || virtualKey == 0x28) {
        if (s->vm.state == VM_WAIT_CHOICE && s->vm.choiceCount) {
            int n = s->vm.choiceCount;
            s->hover = ((s->hover < 0 ? 0 : s->hover) + (virtualKey == 0x28 ? 1 : n - 1)) % n;
            s->dirty = true;
        }
    }
}

void vnsystemaoi_wheel(VNSystemAoi *sa, int notches) {
    SA *s = (SA *)sa;
    if (s->mode == MODE_SHOP) {
        sa_shop_wheel(s, notches);
        return;
    }
    if (s->mode != MODE_STORY || s->movie) return;
    if (!s->log.open) {
        if (notches > 0 && s->log.count) {
            sa_log_open(&s->log);
            s->autoMode = s->skipMode = false;
            s->dirty = true;
        }
        return;
    }
    if (notches < 0 && s->log.scroll == 0) s->log.open = false;
    else sa_log_scroll(&s->log, notches);
    s->dirty = true;
}

void vnsystemaoi_audio(VNSystemAoi *sa, float *out, int frames) {
    SA *s = (SA *)sa;
    sa_mixer_mix(s->mix, out, frames, s->paused);
    pthread_mutex_lock(&s->movieLock);
    if (s->movie && !s->paused) {
        static float buf[8192];
        int done = 0;
        while (done < frames) {
            int want = frames - done < 4096 ? frames - done : 4096;
            size_t got = s->movies.audio(s->movie, buf, (size_t)want);
            for (size_t i = 0; i < got * 2; i++) out[done * 2 + i] += buf[i];
            if (got < (size_t)want) break;
            done += (int)got;
        }
    }
    pthread_mutex_unlock(&s->movieLock);
}

void vnsystemaoi_set_paused(VNSystemAoi *sa, bool paused) { ((SA *)sa)->paused = paused; }
void vnsystemaoi_set_skip(VNSystemAoi *sa, bool on) {
    SA *s = (SA *)sa;
    s->forceSkip = on;
    s->dirty = true;
}
void vnsystemaoi_request_quit(VNSystemAoi *sa) { ((SA *)sa)->quit = true; }
const char *vnsystemaoi_title(VNSystemAoi *sa) { return ((SA *)sa)->title; }
const char *vnsystemaoi_problem(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    return s->problem[0] ? s->problem : NULL;
}

void sa_debug_battle(VNSystemAoi *sa, const char *group, const char *backdrop) {
    SA *s = (SA *)sa;
    if (!s->party.unit[PARTY_FRONT + 1]) sa_party_new_game(&s->party, &s->vfs);
    if (!sa_battle_start(&s->battle, &s->vfs, group, backdrop, sa_vm_random(&s->vm))) {
        sa_log(s, "no enemy group %s", group);
        return;
    }
    s->mode = MODE_BATTLE;
    s->dirty = true;
}

void sa_debug_new_game(VNSystemAoi *sa) {
    SA *s = (SA *)sa;
    if (s->movie) movie_stop(s);
    s->mode = MODE_STORY;
    new_game(s);
}

void sa_debug_dungeon(VNSystemAoi *sa, const char *map, int x, int y, int dir) {
    SA *s = (SA *)sa;
    if (!s->party.unit[PARTY_FRONT + 1]) sa_party_new_game(&s->party, &s->vfs);
    if (!sa_dungeon_load(&s->dungeon, &s->vfs, "m010set.pvm", map)) {
        sa_log(s, "can't load the dungeon %s", map);
        return;
    }
    sa_dungeon_art(&s->dungeon, &s->vfs, "m000bt1a.agf");
    if (x < 0 && !sa_dungeon_find_entrance(&s->dungeon, &x, &y, &dir)) x = y = dir = 0;
    sa_dungeon_place(&s->dungeon, x, y, dir);
    s->mode = MODE_DUNGEON;
    s->dirty = true;
}

void sa_debug_expedition(VNSystemAoi *sa, int dungeon, int floor) {
    SA *s = (SA *)sa;
    if (!s->inGame) {
        sa_party_new_game(&s->party, &s->vfs);
        s->inGame = true;
    }
    if (dungeon < 0) sa_exp_force_battle(s);
    else if (dungeon > 0) sa_exp_enter(s, dungeon, floor);
    else sa_exp_open_select(s);
}

void sa_debug_place(VNSystemAoi *sa, int x, int y, int dir) { sa_exp_debug_place((SA *)sa, x, y, dir); }

void sa_debug_finds(VNSystemAoi *sa, int *out) {
    SA *s = (SA *)sa;
    for (int i = 0; i < 4; i++) out[i] = s->vm.c[0xd2 + i];
}

double sa_debug_bench(VNSystemAoi *sa, int n) {
    SA *s = (SA *)sa;
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < n; i++) sa_exp_draw_view(s, s->fb);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((double)(b.tv_sec - a.tv_sec) * 1e3 + (double)(b.tv_nsec - a.tv_nsec) / 1e6) / (n > 0 ? n : 1);
}

void sa_debug_set(VNSystemAoi *sa, bool flag, int index, int value) {
    SA *s = (SA *)sa;
    if (flag) s->vm.f[index] = (u8)(value != 0);
    else s->vm.c[index] = value;
    s->dirty = true;
}

int sa_debug_get(VNSystemAoi *sa, bool flag, int index) {
    SA *s = (SA *)sa;
    return flag ? s->vm.f[index] : s->vm.c[index];
}

void sa_debug_unbench(VNSystemAoi *sa, int index, int slot) {
    SA *s = (SA *)sa;
    sa_party_unbench(&s->party, &s->vfs, index, slot);
    s->dirty = true;
}

void sa_debug_join(VNSystemAoi *sa, const char *key) {
    SA *s = (SA *)sa;
    sa_party_join(&s->party, &s->vfs, key);
    s->dirty = true;
}

void sa_load_into_town(SA *s, int slot) {
    if (!s->inGame) sa_party_new_game(&s->party, &s->vfs);
    if (sa_save_read(s, slot)) sa_enter_loaded(s);
}

void sa_enter_loaded(SA *s) {
    sa_vm_register_hooks(&s->vm, &s->vfs, "init.box");
    s->playMs = (u32)s->vm.c[11] * 1000;
    sa_reset_scene(s);
    s->inGame = true;
    bool inDungeon = s->exp.active;
    s->exp.active = false;
    s->turnPending = s->autosaveTurn = s->turnAdvance = false;
    s->dialog.open = false;
    s->mode = MODE_TOWN;
    s->town.mapOnly = false;
    s->town.hover = -1;
    s->dirty = true;
    if (inDungeon) sa_exp_resume(s, s->exp.dungeon, s->exp.floor, s->vm.c[73], s->vm.c[74], s->vm.c[75], s->exp.groupCount > 0);
}

void sa_debug_pcload(VNSystemAoi *sa, const char *path) {
    SA *s = (SA *)sa;
    if (s->movie) movie_stop(s);
    if (!s->inGame) sa_party_new_game(&s->party, &s->vfs);
    if (sa_pcsave_read(s, path)) sa_enter_loaded(s);
}

void sa_debug_pcsave(VNSystemAoi *sa, const char *path) { sa_pcsave_write((SA *)sa, path); }

void sa_debug_save(VNSystemAoi *sa, int slot, bool load) {
    SA *s = (SA *)sa;
    if (load) {
        if (s->movie) movie_stop(s);
        sa_load_into_town(s, slot);
    } else
        sa_save_write(s, slot, 0);
}
