#include "sg.h"

#include <pthread.h>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define stb_vorbis_close sg_stb_vorbis_close
#define stb_vorbis_decode_memory sg_stb_vorbis_decode_memory
#define stb_vorbis_get_comment sg_stb_vorbis_get_comment
#define stb_vorbis_get_error sg_stb_vorbis_get_error
#define stb_vorbis_get_file_offset sg_stb_vorbis_get_file_offset
#define stb_vorbis_get_frame_float sg_stb_vorbis_get_frame_float
#define stb_vorbis_get_frame_short sg_stb_vorbis_get_frame_short
#define stb_vorbis_get_frame_short_interleaved sg_stb_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_info sg_stb_vorbis_get_info
#define stb_vorbis_get_sample_offset sg_stb_vorbis_get_sample_offset
#define stb_vorbis_get_samples_float sg_stb_vorbis_get_samples_float
#define stb_vorbis_get_samples_float_interleaved sg_stb_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_short sg_stb_vorbis_get_samples_short
#define stb_vorbis_get_samples_short_interleaved sg_stb_vorbis_get_samples_short_interleaved
#define stb_vorbis_open_memory sg_stb_vorbis_open_memory
#define stb_vorbis_seek sg_stb_vorbis_seek
#define stb_vorbis_seek_frame sg_stb_vorbis_seek_frame
#define stb_vorbis_seek_start sg_stb_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples sg_stb_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds sg_stb_vorbis_stream_length_in_seconds
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "third_party/stb_vorbis.c"
#pragma GCC diagnostic pop
#undef L
#undef C
#undef R

typedef struct Voice {
    u8 *data;
    stb_vorbis *ogg;
    int rate;
    double step, pos;
    float cur[2], next[2];
    bool primed, loop, done, paused;
    float gain, target, delta;
    bool stopAtZero;
    int ch;
    Guid id;
    short buf[4096];
    int bufLen, bufPos;
    u32 played, loopStart, loopEnd;
} Voice;

static const float kPlayerLevel = 0.5f;

enum { V_BGM, V_BGM_OLD, V_BGS, V_BGS_OLD, V_ME, V_BGM_HELD, V_SE, V_SE_COUNT = 16, V_COUNT = V_SE + V_SE_COUNT };

struct Mixer {
    int rate;
    pthread_mutex_t lock;
    Voice *v[V_COUNT];
    float bgmMaster, seMaster;
    bool enabled;
};


SgE *sg_e(SG *g) { return &g->e; }

static void voice_free(Voice *v) {
    if (!v) return;
    if (v->ogg) stb_vorbis_close(v->ogg);
    free(v->data);
    free(v);
}

void sg_e_open(SG *g) {
    SgE *e = &g->e;
    e->curScene = -1;
    Mixer *m = sg_alloc(sizeof *m);
    m->rate = g->cfg.audioRate > 0 ? g->cfg.audioRate : 44100;
    m->bgmMaster = m->seMaster = 1;
    m->enabled = true;
    pthread_mutex_init(&m->lock, NULL);
    e->mix = m;
}

void sg_e_close(SG *g) {
    SgE *e = &g->e;
    Mixer *m = e->mix;
    if (!m) return;
    for (int i = 0; i < V_COUNT; i++) voice_free(m->v[i]);
    pthread_mutex_destroy(&m->lock);
    free(m);
    save_free(&e->save);
    memset(e, 0, sizeof *e);
}

static bool refill(Voice *v) {
    v->bufPos = 0;
    v->bufLen = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        v->bufLen = stb_vorbis_get_samples_short_interleaved(v->ogg, 2, v->buf, 4096);
        if (v->bufLen > 0) return true;
        if (!v->loop) return false;
        stb_vorbis_seek(v->ogg, v->loopStart);
        v->played = v->loopStart;
    }
    return false;
}

static bool read_frame(Voice *v, float out[2]) {
    if (v->loop && v->loopEnd && v->played >= v->loopEnd) {
        stb_vorbis_seek(v->ogg, v->loopStart);
        v->bufLen = v->bufPos = 0;
        v->played = v->loopStart;
    }
    if (v->bufPos >= v->bufLen && !refill(v)) return false;
    v->played++;
    out[0] = v->buf[v->bufPos * 2] / 32768.0f;
    out[1] = v->buf[v->bufPos * 2 + 1] / 32768.0f;
    v->bufPos++;
    return true;
}

static void mix_voice(Mixer *m, Voice *v, float *out, int frames) {
    if (!v->primed) {
        if (!read_frame(v, v->cur) || !read_frame(v, v->next)) {
            v->done = true;
            return;
        }
        v->primed = true;
    }
    float master = (v->ch == SND_SE ? m->seMaster : m->bgmMaster) * kPlayerLevel;
    for (int i = 0; i < frames; i++) {
        if (v->gain != v->target) {
            v->gain += v->delta;
            if ((v->delta > 0 && v->gain >= v->target) || (v->delta < 0 && v->gain <= v->target) || v->delta == 0) v->gain = v->target;
            if (v->gain <= 0 && v->stopAtZero) {
                v->done = true;
                return;
            }
        }
        float f = (float)v->pos, k = v->gain * master;
        out[i * 2] += (v->cur[0] + (v->next[0] - v->cur[0]) * f) * k;
        out[i * 2 + 1] += (v->cur[1] + (v->next[1] - v->cur[1]) * f) * k;
        v->pos += v->step;
        while (v->pos >= 1.0) {
            v->pos -= 1.0;
            v->cur[0] = v->next[0];
            v->cur[1] = v->next[1];
            if (!read_frame(v, v->next)) {
                v->done = true;
                return;
            }
        }
    }
}

void snd_mix(SG *g, float *out, int frames) {
    memset(out, 0, sizeof(float) * (size_t)frames * 2);
    SgE *e = &g->e;
    if (!e->mix) return;
    Mixer *m = e->mix;
    pthread_mutex_lock(&m->lock);
    bool me = m->v[V_ME] && !m->v[V_ME]->done;
    for (int s = 0; s < V_COUNT; s++) {
        Voice *v = m->v[s];
        if (!v || v->done || s == V_BGM_HELD) continue;
        if (me && (s == V_BGM || s == V_BGM_OLD)) continue;
        mix_voice(m, v, out, frames);
    }
    for (int s = 0; s < V_COUNT; s++)
        if (m->v[s] && m->v[s]->done && s != V_BGM && s != V_BGS) {
            voice_free(m->v[s]);
            m->v[s] = NULL;
        }
    pthread_mutex_unlock(&m->lock);
    for (int i = 0; i < frames * 2; i++) out[i] = out[i] > 1 ? 1 : out[i] < -1 ? -1 : out[i];
}

static u32 comment_value(stb_vorbis *ogg, const char *key) {
    stb_vorbis_comment c = stb_vorbis_get_comment(ogg);
    size_t n = strlen(key);
    for (int i = 0; i < c.comment_list_length; i++)
        if (strncasecmp(c.comment_list[i], key, n) == 0 && c.comment_list[i][n] == '=') return (u32)strtoul(c.comment_list[i] + n + 1, NULL, 10);
    return 0;
}

static const SgrRec *res_record(SG *g, Guid id) {
    Sgr *files[65];
    int n = 0;
    files[n++] = &g->data.resFile;
    for (int i = 0; i < g->data.packCount && n < 65; i++) files[n++] = &g->data.packRes[i];
    for (int f = 0; f < n; f++)
        for (int i = 0; i < files[f]->count; i++) {
            const SgrRec *r = &files[f]->recs[i];
            if (r->type == SGR_RESOURCE && r->size >= 16 && memcmp(r->body, id.b, 16) == 0) return r;
        }
    return NULL;
}

static bool res_loops(SG *g, const Res *res) {
    if (res->kind != RES_BGM) return true;
    const SgrRec *rec = res_record(g, res->id);
    if (!rec) return true;
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    free(rd_str(&r));
    u8 loop = rd_u8(&r);
    return r.bad || loop != 0;
}

static Voice *open_path(SG *g, Mixer *m, const char *path, Guid id, int ch, int volume, int tempo, bool loop);

static Voice *open_voice(SG *g, Mixer *m, Guid id, int ch, int volume, int tempo) {
    if (guid_zero(id)) return NULL;
    const Res *res = res_find(&g->res, id);
    if (!res || (res->kind != RES_BGM && res->kind != RES_SE && res->kind != RES_BGS)) {
        char s[37];
        guid_str(id, s);
        sg_log(g, "sound %s is not a sound resource", s);
        return NULL;
    }
    return open_path(g, m, res->path, id, ch, volume, tempo, ch != SND_SE && ch != SND_ME && res_loops(g, res));
}

static Voice *open_path(SG *g, Mixer *m, const char *path, Guid id, int ch, int volume, int tempo, bool loop) {
    size_t n = 0;
    u8 *d = pack_read(&g->data.pack, path, &n);
    if (!d) {
        sg_log(g, "sound missing: %s", path);
        return NULL;
    }
    Voice *v = sg_alloc(sizeof *v);
    v->data = d;
    v->id = id;
    v->ch = ch;
    int err = 0;
    v->ogg = stb_vorbis_open_memory(d, (int)n, &err, NULL);
    if (v->ogg) {
        v->rate = (int)stb_vorbis_get_info(v->ogg).sample_rate;
        v->loopStart = comment_value(v->ogg, "LOOPSTART");
        u32 len = comment_value(v->ogg, "LOOPLENGTH");
        if (len) v->loopEnd = v->loopStart + len;
    }
    if (!v->ogg) {
        sg_log(g, "can't decode sound %s", path);
        voice_free(v);
        return NULL;
    }
    v->loop = loop;
    if (tempo <= 0) tempo = 100;
    v->step = (double)v->rate / (double)m->rate * (double)tempo / 100.0;
    if (volume < 0) volume = 0;
    v->gain = v->target = (float)volume / 100.0f;
    sg_log(g, "sound %s %s vol %d tempo %d%s", ch == SND_BGM ? "bgm" : ch == SND_BGS ? "bgs" : ch == SND_ME ? "me" : "se", path, volume, tempo,
           v->loopEnd ? " loop points" : "");
    return v;
}

static void fade_out(Mixer *m, Voice *v, int ms) {
    if (!v) return;
    if (ms <= 0) {
        v->done = true;
        return;
    }
    v->target = 0;
    v->stopAtZero = true;
    v->delta = -v->gain / ((float)ms * (float)m->rate / 1000.0f);
}

static void replace(Mixer *m, int slot, int oldSlot, Voice *nv, int fadeMs) {
    Voice *cur = m->v[slot];
    voice_free(m->v[oldSlot]);
    m->v[oldSlot] = NULL;
    if (cur && !cur->done && fadeMs > 0) {
        fade_out(m, cur, fadeMs);
        m->v[oldSlot] = cur;
    } else {
        voice_free(cur);
    }
    m->v[slot] = nv;
}

static void play_loop(SG *g, int slot, int oldSlot, int ch, Guid id, int volume, int tempo) {
    SgE *e = &g->e;
    Mixer *m = e->mix;
    if (!m) return;
    pthread_mutex_lock(&m->lock);
    Voice *cur = m->v[slot];
    if (cur && !cur->done && !cur->stopAtZero && guid_eq(cur->id, id)) {
        cur->target = (float)volume / 100.0f;
        cur->gain = cur->target;
        cur->step = (double)cur->rate / (double)m->rate * (double)(tempo > 0 ? tempo : 100) / 100.0;
        pthread_mutex_unlock(&m->lock);
        return;
    }
    pthread_mutex_unlock(&m->lock);
    Voice *v = open_voice(g, m, id, ch, volume, tempo);
    pthread_mutex_lock(&m->lock);
    replace(m, slot, oldSlot, v, 0);
    pthread_mutex_unlock(&m->lock);
}

void snd_bgm(SG *g, Guid id, int volume, int tempo) { play_loop(g, V_BGM, V_BGM_OLD, SND_BGM, id, volume, tempo); }

void snd_bgs(SG *g, Guid id, int volume, int tempo) { play_loop(g, V_BGS, V_BGS_OLD, SND_BGS, id, volume, tempo); }

static void stop_loop(SG *g, int slot, int oldSlot, int fadeMs) {
    SgE *e = &g->e;
    Mixer *m = e->mix;
    if (!m) return;
    pthread_mutex_lock(&m->lock);
    replace(m, slot, oldSlot, NULL, fadeMs);
    pthread_mutex_unlock(&m->lock);
}

void snd_bgm_stop(SG *g, int fadeMs) { stop_loop(g, V_BGM, V_BGM_OLD, fadeMs); }

void snd_bgs_stop(SG *g, int fadeMs) { stop_loop(g, V_BGS, V_BGS_OLD, fadeMs); }

void snd_me(SG *g, Guid id, int volume, int tempo) {
    SgE *e = &g->e;
    if (!e->mix) return;
    Voice *v = open_voice(g, e->mix, id, SND_ME, volume, tempo);
    pthread_mutex_lock(&e->mix->lock);
    voice_free(e->mix->v[V_ME]);
    e->mix->v[V_ME] = v;
    pthread_mutex_unlock(&e->mix->lock);
}

u32 sg_me_file(SG *g, const char *path, int volume) {
    SgE *e = &g->e;
    if (!e->mix) return 0;
    Voice *v = open_path(g, e->mix, path, (Guid){{0}}, SND_ME, volume, 100, false);
    if (!v) return 0;
    u32 ms = (u32)((double)stb_vorbis_stream_length_in_samples(v->ogg) * 1000.0 / (double)v->rate);
    pthread_mutex_lock(&e->mix->lock);
    voice_free(e->mix->v[V_ME]);
    e->mix->v[V_ME] = v;
    pthread_mutex_unlock(&e->mix->lock);
    return ms;
}

void snd_se(SG *g, Guid id, int volume, int tempo) {
    SgE *e = &g->e;
    if (!e->mix) return;
    Voice *v = open_voice(g, e->mix, id, SND_SE, volume, tempo);
    if (!v) return;
    Mixer *m = e->mix;
    pthread_mutex_lock(&m->lock);
    int slot = -1;
    for (int i = V_SE; i < V_COUNT && slot < 0; i++)
        if (!m->v[i] || m->v[i]->done) slot = i;
    if (slot < 0) {
        slot = V_SE;
        for (int i = V_SE + 1; i < V_COUNT; i++)
            if (m->v[i]->played > m->v[slot]->played) slot = i;
    }
    voice_free(m->v[slot]);
    m->v[slot] = v;
    pthread_mutex_unlock(&m->lock);
}

void snd_se_stop(SG *g) {
    SgE *e = &g->e;
    pthread_mutex_lock(&e->mix->lock);
    for (int i = V_SE; i < V_COUNT; i++) {
        voice_free(e->mix->v[i]);
        e->mix->v[i] = NULL;
    }
    pthread_mutex_unlock(&e->mix->lock);
}

void snd_set_master(SG *g, int bgmPercent, int sePercent) {
    SgE *e = &g->e;
    pthread_mutex_lock(&e->mix->lock);
    e->mix->bgmMaster = (float)bgmPercent / 100.0f;
    e->mix->seMaster = (float)sePercent / 100.0f;
    pthread_mutex_unlock(&e->mix->lock);
}

void snd_bgm_hold(SG *g) {
    Mixer *m = g->e.mix;
    if (!m) return;
    pthread_mutex_lock(&m->lock);
    voice_free(m->v[V_BGM_HELD]);
    m->v[V_BGM_HELD] = NULL;
    Voice *v = m->v[V_BGM];
    if (v && !v->done && !v->stopAtZero) {
        m->v[V_BGM_HELD] = v;
        m->v[V_BGM] = NULL;
    }
    pthread_mutex_unlock(&m->lock);
}

bool snd_bgm_release(SG *g, bool resume) {
    Mixer *m = g->e.mix;
    if (!m) return false;
    pthread_mutex_lock(&m->lock);
    Voice *v = m->v[V_BGM_HELD];
    m->v[V_BGM_HELD] = NULL;
    if (v && resume) {
        replace(m, V_BGM, V_BGM_OLD, v, 0);
        voice_free(m->v[V_BGM_OLD]);
        m->v[V_BGM_OLD] = NULL;
    } else {
        voice_free(v);
    }
    pthread_mutex_unlock(&m->lock);
    return v && resume;
}

u32 snd_bgm_position(SG *g) {
    Mixer *m = g->e.mix;
    if (!m) return 0;
    pthread_mutex_lock(&m->lock);
    Voice *v = m->v[V_BGM];
    u32 at = v && !v->done ? v->played : 0;
    pthread_mutex_unlock(&m->lock);
    return at;
}

bool snd_bgm_playing(SG *g, Guid *id) {
    SgE *e = &g->e;
    pthread_mutex_lock(&e->mix->lock);
    Voice *v = e->mix->v[V_BGM];
    bool on = v && !v->done;
    if (on && id) *id = v->id;
    pthread_mutex_unlock(&e->mix->lock);
    return on;
}

static Guid map_bgs(const SgrRec *rec) {
    Rd r = rd_make(rec->body, rec->size);
    rd_guid(&r);
    free(rd_str(&r));
    rd_i32(&r);
    free(rd_str(&r));
    rd_i64(&r);
    i32 n = rd_i32(&r);
    rd_skip(&r, 24 * (size_t)(n > 0 ? n : 0));
    rd_i32(&r);
    n = rd_i32(&r);
    rd_skip(&r, 24 * (size_t)(n > 0 ? n : 0) + 36);
    i32 w = rd_i32(&r), h = rd_i32(&r);
    if (w > 0 && h > 0) rd_skip(&r, 4 * (size_t)w * (size_t)h);
    n = rd_i32(&r);
    rd_skip(&r, 28 * (size_t)(n > 0 ? n : 0));
    n = rd_i32(&r);
    rd_skip(&r, 16 * (size_t)(n > 0 ? n : 0) + 32 + 32);
    Guid bgs = rd_guid(&r);
    return r.bad ? (Guid){{0}} : bgs;
}

void sg_e_frame(SG *g) {
    SgE *e = &g->e;
    if (g->scene == SCENE_TITLE) {
        if (e->curScene != SCENE_TITLE) {
            e->curMap = (Guid){{0}};
            snd_bgs_stop(g, 0);
            snd_bgm(g, g->data.settings.titleBgm, 100, 100);
        }
    } else if (g->field.loaded && !guid_eq(g->field.map.id, e->curMap)) {
        e->curMap = g->field.map.id;
        if (!guid_zero(g->field.map.bgm)) snd_bgm(g, g->field.map.bgm, 100, 100);
        const SgrRec *rec = game_record(&g->data, SGR_MAP, 0, g->field.map.id, NULL);
        Guid bgs = rec ? map_bgs(rec) : (Guid){{0}};
        if (guid_zero(bgs)) snd_bgs_stop(g, 0);
        else snd_bgs(g, bgs, 100, 100);
    }
    e->curScene = g->scene;
}
