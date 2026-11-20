#include "br.h"

struct Stream {
    int num, loop, loopIndex;
    u32 start;
    int frames;
    char **file;
    int *ms;
    u32 total, loopStart;
    Image *cur;
    int curFrame;
};

static Node *top_pair(Node *root, const char *key) {
    for (int i = 0; i < root->n; i++) {
        Node *k = root->k[i];
        if (k->t == N_LIST && k->n && sx_sym(k->k[0], key)) return sx_cdr_value(k);
    }
    return NULL;
}

Stream *stream_open(BR *e, int num, int loop, u32 now) {
    char rel[128];
    snprintf(rel, sizeof rel, "stream/%08d/index.txt", num);
    size_t n;
    u8 *d = br_read_game(e, rel, &n);
    if (!d) {
        br_log(e, "stream %d is missing", num);
        return NULL;
    }
    Node *root = sx_parse((const char *)d, n);
    free(d);
    Stream *s = br_alloc(sizeof *s);
    s->num = num;
    s->loop = loop;
    s->start = now;
    s->curFrame = -1;
    const char *base = sx_str(top_pair(root, "image-basename"));
    int digits = sx_int(top_pair(root, "image-num-digits"), 4);
    const char *ext = sx_str(top_pair(root, "image-extension"));
    Node *li = top_pair(root, "loop-index");
    s->loopIndex = li ? li->i : -1;
    Node *list = top_pair(root, "piclist");
    int wait = 100;
    for (int i = 0; list && list->t == N_LIST && i < list->n; i++) {
        Node *f = list->k[i];
        Node *u = sx_cdr_value(sx_find(f, "url"));
        Node *w = sx_cdr_value(sx_find(f, "waittime"));
        if (w) wait = sx_int(w, wait);
        if (!u) continue;
        char name[160];
        if (u->t == N_INT) snprintf(name, sizeof name, "stream/%08d/%s%0*d%s%s", num, base ? base : "", digits, u->i, ext && *ext ? "." : "", ext ? ext : "");
        else snprintf(name, sizeof name, "stream/%08d/%s%s%s", num, sx_str(u), ext && *ext ? "." : "", ext ? ext : "");
        s->file = br_realloc(s->file, sizeof(char *) * (size_t)(s->frames + 1));
        s->ms = br_realloc(s->ms, sizeof(int) * (size_t)(s->frames + 1));
        s->file[s->frames] = br_strdup(name);
        s->ms[s->frames++] = wait > 0 ? wait : 1;
        s->total += (u32)(wait > 0 ? wait : 1);
    }
    for (int i = 0; i < s->loopIndex && i < s->frames; i++) s->loopStart += (u32)s->ms[i];
    sx_free(root);
    return s;
}

static int frame_at(Stream *s, u32 now) {
    if (!s->frames) return -1;
    u32 t = now - s->start;
    if (t >= s->total) {
        if (s->loopIndex < 0 || s->loopIndex >= s->frames) return s->frames - 1;
        u32 span = s->total - s->loopStart;
        t = s->loopStart + (span ? (t - s->loopStart) % span : 0);
    }
    int i = 0;
    while (i < s->frames - 1 && t >= (u32)s->ms[i]) t -= (u32)s->ms[i++];
    return i;
}

Image *stream_frame(BR *e, Stream *s, u32 now) {
    if (!s) return NULL;
    int f = frame_at(s, now);
    if (f < 0) return NULL;
    if (f != s->curFrame) {
        image_free(s->cur);
        s->cur = image_load(e, s->file[f]);
        s->curFrame = f;
    }
    return s->cur;
}

bool stream_done(Stream *s, u32 now) { return !s || (s->loopIndex < 0 && now - s->start >= s->total); }

void stream_restart(Stream *s, u32 now) {
    if (s) s->start = now;
}

void stream_free(Stream *s) {
    if (!s) return;
    for (int i = 0; i < s->frames; i++) free(s->file[i]);
    free(s->file);
    free(s->ms);
    image_free(s->cur);
    free(s);
}
