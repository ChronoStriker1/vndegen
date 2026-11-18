#include "th.h"

static Scene *parse(const char *name, const u8 *d, size_t size) {
    if (size < 4) return NULL;
    u32 count = rd32(d);
    if (count > 1000000) return NULL;
    Scene *s = th_alloc(sizeof *s);
    s->name = th_strdup(name);
    s->recs = th_alloc(sizeof(Rec) * (count ? count : 1));
    bool swapped = size >= 10 && rd16(d + 4) == 6 && rd32(d + 6) == 0x140050u;
    size_t pos = 4;
    for (u32 i = 0; i < count && pos + 6 <= size; i++) {
        u16 len = rd16(d + pos);
        if (len < 4 || pos + 2 + len > size) break;
        Rec *r = &s->recs[s->count++];
        r->cls = rd16(d + pos + 2);
        r->op = rd16(d + pos + 4);
        r->len = len - 4;
        r->body = th_alloc((size_t)r->len + 1);
        for (int k = 0; k < r->len; k++) {
            u8 b = d[pos + 6 + k];
            r->body[k] = swapped ? (u8)(b >> 4 | b << 4) : b;
        }
        pos += 2 + len;
    }
    return s;
}

void scene_free(Scene *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) free(s->recs[i].body);
    free(s->recs);
    free(s->name);
    free(s);
}

Scene *scene_load(TH *t, const char *name) {
    for (int i = 0; i < t->sceneCount; i++)
        if (th_ieq(t->scenes[i]->name, name)) return t->scenes[i];
    size_t size;
    u8 *d = store_load(&t->store, PAC_SCRIPT, name, &size);
    if (!d) {
        th_log(t, "missing scene %s", name);
        return NULL;
    }
    Scene *s = parse(name, d, size);
    free(d);
    if (!s) {
        th_log(t, "bad scene %s", name);
        return NULL;
    }
    t->scenes = th_realloc(t->scenes, sizeof(Scene *) * (size_t)(t->sceneCount + 1));
    t->scenes[t->sceneCount++] = s;
    return s;
}

int scene_label(const Scene *s, const char *label) {
    for (int i = 0; i < s->count; i++)
        if (s->recs[i].cls == 0x10 && s->recs[i].op == 0 && !strcmp((const char *)s->recs[i].body, label)) return i;
    return -1;
}
