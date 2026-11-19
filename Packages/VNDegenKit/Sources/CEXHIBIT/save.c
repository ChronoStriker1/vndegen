#include "ex.h"

#include <stdarg.h>
#include <time.h>

typedef struct Buf {
    char *d;
    size_t n, cap;
} Buf;

static void put(Buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    int k = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (k < 0) return;
    if (b->n + (size_t)k + 1 > b->cap) {
        b->cap = (b->n + (size_t)k + 1) * 2;
        b->d = ex_realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, tmp, (size_t)k + 1);
    b->n += (size_t)k;
}

static void put_str(Buf *b, const char *s) {
    put(b, " ");
    for (const u8 *p = (const u8 *)(s ? s : ""); *p; p++) {
        if (*p <= ' ' || *p == '%') put(b, "%%%02x", *p);
        else put(b, "%c", *p);
    }
    if (!s || !*s) put(b, "%%00");
}

static char *get_str(char **pp) {
    char *p = *pp;
    while (*p == ' ') p++;
    char *start = p, *w = p;
    while (*p && *p != ' ' && *p != '\n') {
        if (*p == '%' && p[1] && p[2]) {
            char hex[3] = {p[1], p[2], 0};
            *w++ = (char)strtol(hex, NULL, 16);
            p += 3;
        } else *w++ = *p++;
    }
    if (*p == ' ') p++;
    *w = 0;
    *pp = p;
    return start;
}

static long get_int(char **pp) {
    char *end;
    long v = strtol(*pp, &end, 10);
    *pp = end;
    return v;
}

char *save_state(EX *e, size_t *len) {
    Buf b = {0};
    put(&b, "EXHIBIT-VNDEGEN 1\n");
    put(&b, "script");
    put_str(&b, e->savePoint.sc ? e->savePoint.sc->name : (e->sc ? e->sc->name : ""));
    put(&b, " %d\n", e->savePoint.sc ? e->savePoint.pc : e->pc);
    put(&b, "stack %d", e->savePoint.sp);
    for (int i = 0; i < e->savePoint.sp; i++) {
        put_str(&b, e->savePoint.stack[i].sc ? e->savePoint.stack[i].sc->name : "");
        put(&b, " %d", e->savePoint.stack[i].pc);
    }
    put(&b, "\n");
    put(&b, "R");
    for (int i = 0; i < e->nR; i++)
        if (e->R[i]) put(&b, " %d=%d", i, e->R[i]);
    put(&b, "\nF");
    for (int i = 0; i < 64; i++)
        if (e->F[i]) put(&b, " %d=%d", i, e->F[i]);
    put(&b, "\nQ");
    for (int i = 0; i < 16; i++)
        if (e->Q[i]) put(&b, " %d=%d", i, e->Q[i]);
    put(&b, "\n");
    put(&b, "cg %d %u %d %d %d %d\n", e->cgId, e->cgColor & 0xffffff, (int)e->cam.x, (int)e->cam.y, (int)e->cam.w, (int)e->cam.h);
    for (int i = 0; i < EX_BILLBOARDS; i++) {
        Billboard *bb = &e->bb[i];
        if (!bb->used || bb->removing || bb->kind == 3) continue;
        put(&b, "bb %d %d %d %u %d %d %d %d %d", bb->id, bb->kind, bb->srcId, bb->color, bb->x, bb->y, bb->w, bb->h, (int)bb->a);
        put_str(&b, bb->srcFile);
        put(&b, "\n");
    }
    for (int i = 0; i < EX_ACTORS; i++) {
        Actor *a = &e->actors[i];
        if (!a->used || a->leaving) continue;
        put(&b, "actor %d %d %d %d %d %d %d\n", a->chara, a->pose, a->face, (int)a->x, (int)a->y, (int)a->a, a->order);
    }
    for (int i = 0; i < 4; i++) {
        Portrait *p = &e->portraits[i];
        if (p->used) put(&b, "portrait %d %d %d %d %d %d\n", i, p->chara, p->pose, p->face, p->x, p->y);
    }
    put(&b, "msg %d %d %d %d\n", e->msg.frame, e->msg.shown, e->msg.ctrlShown, e->msg.tempHidden);
    put(&b, "sound %d %d %d\n", e->musicTrack, e->loopId, e->envId);
    put(&b, "text %d\n", e->msg.textId);
    *len = b.n;
    return b.d;
}

void state_reset(EX *e) {
    for (int i = 0; i < EX_BILLBOARDS; i++)
        if (e->bb[i].used) {
            if (e->bb[i].ownImg) image_free(e->bb[i].img);
            image_free(e->bb[i].mask);
            memset(&e->bb[i], 0, sizeof e->bb[i]);
        }
    for (int i = 0; i < EX_ACTORS; i++) chara_leave(e, e->actors[i].used ? e->actors[i].chara : -1, 0);
    for (int i = 0; i < 4; i++) {
        image_free(e->portraits[i].img);
        memset(&e->portraits[i], 0, sizeof e->portraits[i]);
    }
    memset(e->tracks, 0, sizeof e->tracks);
    e->focus = 0;
    view_destroy(e);
    msg_clear(e);
    image_free(e->cgPrev);
    e->cgPrev = NULL;
    e->cgFade.active = false;
}

bool load_state(EX *e, const char *data, size_t len) {
    char *text = ex_alloc(len + 1);
    memcpy(text, data, len);
    if (strncmp(text, "EXHIBIT-VNDEGEN 1\n", 18)) {
        free(text);
        return false;
    }
    char *scName = NULL;
    int pc = 0;
    state_reset(e);
    memset(e->R, 0, sizeof(i32) * (size_t)e->nR);
    memset(e->F, 0, sizeof e->F);
    memset(e->Q, 0, sizeof e->Q);
    e->sp = 0;
    snd_stop(e, EX_CH_MUSIC, 0);
    snd_stop(e, EX_CH_LOOP, 0);
    snd_stop(e, EX_CH_ENV, 0);
    snd_stop(e, EX_CH_VOICE, 0);
    snd_stop(e, EX_CH_SE, 0);
    for (char *line = strtok(text + 18, "\n"); line; line = strtok(NULL, "\n")) {
        char *p = strchr(line, ' ');
        char key[16] = {0};
        size_t kl = p ? (size_t)(p - line) : strlen(line);
        if (kl >= sizeof key) continue;
        memcpy(key, line, kl);
        p = p ? p + 1 : line + kl;
        if (!strcmp(key, "script")) {
            scName = ex_strdup(get_str(&p));
            pc = (int)get_int(&p);
        } else if (!strcmp(key, "stack")) {
            int n = (int)get_int(&p);
            for (int i = 0; i < n && i < 64; i++) {
                char *name = get_str(&p);
                int fpc = (int)get_int(&p);
                Script *sc = rld_get(e, name);
                if (sc) e->stack[e->sp++] = (Frame){sc, fpc};
            }
        } else if (!strcmp(key, "R") || !strcmp(key, "F") || !strcmp(key, "Q")) {
            while (*p) {
                int idx = (int)get_int(&p);
                if (*p != '=') break;
                p++;
                *reg_ref(e, key[0], idx) = (i32)get_int(&p);
                while (*p == ' ') p++;
            }
        } else if (!strcmp(key, "cg")) {
            int id = (int)get_int(&p);
            u32 color = (u32)get_int(&p);
            float x = (float)get_int(&p), y = (float)get_int(&p), w = (float)get_int(&p), h = (float)get_int(&p);
            gfx_set_cg(e, id, color);
            gfx_camera(e, x, y, w > 0 ? w : EX_W, h > 0 ? h : EX_H, 0, 0);
        } else if (!strcmp(key, "bb")) {
            int v[9];
            for (int i = 0; i < 9; i++) v[i] = (int)get_int(&p);
            gfx_restore_billboard(e, v, get_str(&p));
        } else if (!strcmp(key, "actor")) {
            int v[7];
            for (int i = 0; i < 7; i++) v[i] = (int)get_int(&p);
            chara_restore(e, v);
        } else if (!strcmp(key, "portrait")) {
            int v[6];
            for (int i = 0; i < 6; i++) v[i] = (int)get_int(&p);
            char args[128];
            snprintf(args, sizeof args, "%d,%d,%d,%d,0,%d,%d,9,-1", v[0], v[1], v[2], v[3], v[4], v[5]);
            portrait_draw_cmd(e, args);
        } else if (!strcmp(key, "msg")) {
            int v[4];
            for (int i = 0; i < 4; i++) v[i] = (int)get_int(&p);
            msg_set_frame(e, v[0]);
            msg_show(e, v[1] != 0, 0);
            e->msg.ctrlShown = v[2] != 0;
            msg_temp_hide(e, v[3] != 0, 0);
        } else if (!strcmp(key, "sound")) {
            int music = (int)get_int(&p), loop = (int)get_int(&p), env = (int)get_int(&p);
            if (music >= 0) snd_play_music(e, music, 0);
            if (loop >= 0) snd_play_id(e, loop, EX_CH_LOOP, true, 0);
            if (env >= 0) snd_play_env(e, env, 100);
        }
    }
    free(text);
    Script *sc = scName ? rld_get(e, scName) : NULL;
    free(scName);
    if (!sc) return false;
    e->sc = sc;
    e->pc = pc;
    e->wait = W_NONE;
    e->ended = false;
    e->view.active = false;
    return true;
}

static void thumb_scale(const u32 *src, u32 *dst, int tw, int th) {
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++) {
            u32 r = 0, g = 0, b = 0;
            int x0 = x * EX_W / tw, y0 = y * EX_H / th;
            for (int j = 0; j < 6; j++)
                for (int i = 0; i < 6; i++) {
                    u32 p = src[(size_t)(y0 + j) * EX_W + (size_t)(x0 + i)];
                    r += p & 0xff;
                    g += p >> 8 & 0xff;
                    b += p >> 16 & 0xff;
                }
            dst[(size_t)y * (size_t)tw + (size_t)x] = (r / 36) | (g / 36) << 8 | (b / 36) << 16 | 0xff000000u;
        }
}

u8 *gyu_encode(const u32 *px, int w, int h, size_t *outLen) {
    size_t stride = ((size_t)w * 3 + 3) & ~(size_t)3;
    size_t raw = stride * (size_t)h;
    size_t packedMax = raw + raw / 8 + 16;
    u8 *out = ex_alloc(0x24 + packedMax);
    memcpy(out, "GYU\x1a", 4);
    out[4] = 0;
    out[5] = 0;
    out[6] = 0x00;
    out[7] = 0x04;
    u32 hdr[7] = {0xffffffffu, 24, (u32)w, (u32)h, 0, 0, 0};
    u8 *q = out + 0x24;
    u8 *row = ex_alloc(stride);
    size_t k = 0;
    u8 *flag = NULL;
    int bit = 8;
    for (int y = h - 1; y >= 0; y--) {
        memset(row, 0, stride);
        for (int x = 0; x < w; x++) {
            u32 p = px[(size_t)y * (size_t)w + (size_t)x];
            row[x * 3] = (u8)(p >> 16);
            row[x * 3 + 1] = (u8)(p >> 8);
            row[x * 3 + 2] = (u8)p;
        }
        for (size_t i = 0; i < stride; i++) {
            if (bit == 8) {
                flag = &q[k++];
                *flag = 0;
                bit = 0;
            }
            *flag |= (u8)(1 << bit++);
            q[k++] = row[i];
        }
    }
    free(row);
    hdr[4] = (u32)k;
    for (int i = 0; i < 7; i++) {
        u32 v = hdr[i];
        u8 *d = out + 8 + i * 4;
        d[0] = (u8)v;
        d[1] = (u8)(v >> 8);
        d[2] = (u8)(v >> 16);
        d[3] = (u8)(v >> 24);
    }
    *outLen = 0x24 + k;
    return out;
}

static char *slot_path(EX *e, int slot, const char *ext) { return ex_fmt("%s/vndegen/slot%03d.%s", e->saveDir, slot, ext); }

bool save_slot(EX *e, int slot) {
    size_t n;
    char *data = save_state(e, &n);
    char *dir = ex_fmt("%s/vndegen", e->saveDir);
    ex_mkdirs(dir);
    free(dir);
    char *path = slot_path(e, slot, "sav");
    bool ok = ex_write_file(path, data, n);
    free(path);
    free(data);
    if (!ok) return false;
    u32 *thumb = ex_alloc(sizeof(u32) * 320 * 180);
    thumb_scale(e->saveShot ? e->saveShot : e->screen, thumb, 320, 180);
    size_t gl;
    u8 *g = gyu_encode(thumb, 320, 180, &gl);
    free(thumb);
    path = slot_path(e, slot, "gyu");
    ex_write_file(path, g, gl);
    free(path);
    free(g);
    SlotInfo *si = &e->slots[slot];
    si->used = true;
    si->time = (long long)time(NULL);
    image_free(si->thumb);
    si->thumb = NULL;
    si->thumbLoaded = false;
    free(si->memo);
    si->memo = e->memo ? ex_strdup(e->memo) : NULL;
    slots_write_index(e);
    return true;
}

bool load_slot(EX *e, int slot) {
    char *path = slot_path(e, slot, "sav");
    size_t n;
    u8 *d = ex_read_file(path, &n);
    free(path);
    if (!d) return false;
    bool ok = load_state(e, (const char *)d, n);
    free(d);
    return ok;
}

Image *slot_thumb(EX *e, int slot) {
    SlotInfo *si = &e->slots[slot];
    if (!si->thumbLoaded) {
        si->thumbLoaded = true;
        char *path = slot_path(e, slot, "gyu");
        size_t n;
        u8 *d = ex_read_file(path, &n);
        free(path);
        if (d) {
            si->thumb = gyu_decode(d, n);
            free(d);
        }
    }
    return si->thumb;
}

void slots_write_index(EX *e) {
    Buf b = {0};
    put(&b, "EXHIBIT-VNDEGEN-SLOTS 1\n");
    for (int i = 0; i < EX_SLOTS; i++) {
        SlotInfo *si = &e->slots[i];
        if (!si->used) continue;
        put(&b, "%d %lld", i, si->time);
        put_str(&b, si->memo);
        put(&b, "\n");
    }
    char *path = ex_fmt("%s/vndegen/slots.txt", e->saveDir);
    ex_write_file(path, b.d, b.n);
    free(path);
    free(b.d);
}

void slots_read_index(EX *e) {
    char *path = ex_fmt("%s/vndegen/slots.txt", e->saveDir);
    size_t n;
    char *d = (char *)ex_read_file(path, &n);
    free(path);
    if (!d) return;
    char *text = ex_alloc(n + 1);
    memcpy(text, d, n);
    free(d);
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        if (!strncmp(line, "EXHIBIT", 7)) continue;
        char *p = line;
        int slot = (int)get_int(&p);
        if (slot < 0 || slot >= EX_SLOTS) continue;
        SlotInfo *si = &e->slots[slot];
        si->used = true;
        si->time = strtoll(p, &p, 10);
        free(si->memo);
        si->memo = ex_strdup(get_str(&p));
    }
    free(text);
}

void slots_free(EX *e) {
    for (int i = 0; i < EX_SLOTS; i++) {
        image_free(e->slots[i].thumb);
        free(e->slots[i].memo);
    }
    memset(e->slots, 0, sizeof e->slots);
}

void reset_for_title(EX *e) {
    state_reset(e);
    gfx_draw_cg(e, -0x7fffffff - 1, 0);
    gfx_camera(e, 0, 0, EX_W, EX_H, 0, 0);
    msg_show(e, false, 0);
    e->msg.ctrlShown = false;
    e->msg.userHidden = false;
    e->sp = 0;
    e->skipping = e->autoMode = false;
    snd_stop(e, EX_CH_MUSIC, 0);
    snd_stop(e, EX_CH_LOOP, 0);
    snd_stop(e, EX_CH_ENV, 0);
    snd_stop(e, EX_CH_VOICE, 0);
}
