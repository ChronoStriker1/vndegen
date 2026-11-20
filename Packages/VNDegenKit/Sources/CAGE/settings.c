#include "age.h"

#include <strings.h>

static const char *defaults[] = {
    "[display]", "UseDirectDrawEmuration=0", "UseWideSurface=0", "ScreenMode=1", "FullScreenBit=32",
    "UseIVideoWindow=1", "DeviceType=0", "VertexProcessing=0", "PresentInterval=0", "TextureBit=32", "PixelShader=-1",
    "ForceScreen=0", "VirtualFullScreen=0", "AspectMode=0", "FullScreenWidth=0", "FullScreenHeight=0", "LimitAero=0",
    "VirtualFullScreenType=0", "[sound]", "Sound=1", "Music=2", "SE=1", "Voice=1", "Movie=1", "UseDirectSound=1",
    "UseDirectSoundEmuration=1", "DirectSoundCooperativeLevel=1", "Volume0=-1", "Volume1=-1", "Volume2=-1",
    "Volume3=-1", "Volume4=-1", "MusicFadeOnVoicePlaying=1", "MusicFadeOnVoicePlayingVolume=50", "[message]", "Font=",
    "RMouseEvent=0", "MessageSpeed=5", "MesWinAlpha=8", "UseAntiFont=1", "AntiFontLevel=0", "AutoMessageTime0=500",
    "AutoMessageTime1=2000", "ReadTextSkip=0", "AdvanceMesOnWheel=0", "[system]", "SaveBMPPath=", "UseMMX=1",
    "EffectSkipOnClick=0", "OutErrorLog=0",
};

#define lines (g->ini)
#define nlines (g->nini)

static void add(AG *g, const char *section, const char *key, const char *value) {
    lines = ag_realloc(lines, sizeof(IniLine) * (size_t)(nlines + 1));
    lines[nlines].section = ag_strdup(section);
    lines[nlines].key = key ? ag_strdup(key) : NULL;
    lines[nlines].value = value ? ag_strdup(value) : NULL;
    nlines++;
}

static IniLine *find(AG *g, const char *section, const char *key) {
    for (int i = 0; i < nlines; i++)
        if (lines[i].key && !strcasecmp(lines[i].section, section) && !strcasecmp(lines[i].key, key)) return &lines[i];
    return NULL;
}

static void set(AG *g, const char *section, const char *key, const char *value) {
    IniLine *l = find(g, section, key);
    if (l) {
        free(l->value);
        l->value = ag_strdup(value);
        return;
    }
    int at = nlines;
    for (int i = 0; i < nlines; i++)
        if (!strcasecmp(lines[i].section, section)) at = i + 1;
    add(g, section, key, value);
    IniLine t = lines[nlines - 1];
    memmove(lines + at + 1, lines + at, sizeof(IniLine) * (size_t)(nlines - 1 - at));
    lines[at] = t;
}

static void set_int(AG *g, const char *section, const char *key, int v) {
    char b[32];
    snprintf(b, sizeof b, "%d", v);
    set(g, section, key, b);
}

static int get_int(AG *g, const char *section, const char *key, int def) {
    IniLine *l = find(g, section, key);
    return l && l->value && *l->value ? atoi(l->value) : def;
}

static void parse_line(AG *g, char *section, size_t cap, const char *s) {
    if (*s == '[') {
        const char *e = strchr(s, ']');
        size_t n = e ? (size_t)(e - s - 1) : strlen(s + 1);
        if (n >= cap) n = cap - 1;
        memcpy(section, s + 1, n);
        section[n] = 0;
        add(g, section, NULL, NULL);
        return;
    }
    const char *eq = strchr(s, '=');
    if (!eq) return;
    char key[128];
    size_t kn = (size_t)(eq - s);
    if (kn >= sizeof key) kn = sizeof key - 1;
    memcpy(key, s, kn);
    key[kn] = 0;
    add(g, section, key, eq + 1);
}

static char *path_of(AG *g) { return ag_fmt("%s/SYS4REG.INI", g->saveDir ? g->saveDir : "."); }

void settings_load(AG *g) {
    char section[64] = "";
    size_t n = 0;
    char *p = path_of(g);
    u8 *d = ag_read_file(p, &n);
    free(p);
    if (d) {
        char *s = (char *)d, *end = s + n;
        while (s < end) {
            char *e = memchr(s, '\n', (size_t)(end - s));
            if (!e) e = end;
            char *t = e;
            while (t > s && (t[-1] == '\r' || t[-1] == ' ')) t--;
            char save = *t;
            *t = 0;
            if (t > s) parse_line(g, section, sizeof section, s);
            *t = save;
            s = e + 1;
        }
        free(d);
    }
    if (!nlines)
        for (size_t i = 0; i < sizeof defaults / sizeof *defaults; i++) parse_line(g, section, sizeof section, defaults[i]);
    const char *font = store_setting(&g->st, "FONT");
    if (font && !find(g, "message", "Font")) set(g, "message", "Font", font);
    IniLine *f = find(g, "message", "Font");
    if (font && f && (!f->value || !*f->value)) set(g, "message", "Font", font);
    Sound *snd = &g->snd;
    for (int i = 0; i < 5; i++) {
        char k[16];
        snprintf(k, sizeof k, "Volume%d", i);
        int v = get_int(g, "sound", k, -1);
        snd->vol[i] = v < 0 ? 10000 : v;
    }
    snd->route[1] = get_int(g, "sound", "Music", 2) >= 0;
    snd->route[2] = get_int(g, "sound", "SE", 1) != 0;
    snd->route[3] = get_int(g, "sound", "Voice", 1) != 0;
    snd->route[4] = get_int(g, "sound", "Movie", 1) != 0;
    g->text.delay = get_int(g, "message", "MessageSpeed", 5);
    g->text.winAlpha = get_int(g, "message", "MesWinAlpha", 8);
    g->text.autoTime[0] = get_int(g, "message", "AutoMessageTime0", 500);
    g->text.autoTime[1] = get_int(g, "message", "AutoMessageTime1", 2000);
    g->in.readSkip = get_int(g, "message", "ReadTextSkip", 0) != 0;
    snd_apply(g);
}

void settings_save(AG *g) {
    if (!nlines || !g->saveDir) return;
    Sound *snd = &g->snd;
    for (int i = 0; i < 5; i++) {
        char k[16];
        snprintf(k, sizeof k, "Volume%d", i);
        if (get_int(g, "sound", k, -1) < 0 && !snd->volSet[i]) continue;
        set_int(g, "sound", k, snd->vol[i]);
    }
    int music = get_int(g, "sound", "Music", 2);
    if (snd->route[1] != (music >= 0)) set_int(g, "sound", "Music", snd->route[1] ? music + 3 : music - 3);
    set_int(g, "sound", "SE", snd->route[2]);
    set_int(g, "sound", "Voice", snd->route[3]);
    set_int(g, "sound", "Movie", snd->route[4]);
    set_int(g, "message", "MessageSpeed", g->text.delay);
    set_int(g, "message", "MesWinAlpha", g->text.winAlpha);
    set_int(g, "message", "AutoMessageTime0", g->text.autoTime[0]);
    set_int(g, "message", "AutoMessageTime1", g->text.autoTime[1]);
    set_int(g, "message", "ReadTextSkip", g->in.readSkip);
    char *out = NULL;
    size_t len = 0;
    for (int i = 0; i < nlines; i++) {
        char *l = lines[i].key ? ag_fmt("%s=%s\r\n", lines[i].key, lines[i].value ? lines[i].value : "") : ag_fmt("[%s]\r\n", lines[i].section);
        size_t n = strlen(l);
        out = ag_realloc(out, len + n + 1);
        memcpy(out + len, l, n + 1);
        len += n;
        free(l);
    }
    ag_mkdirs(g->saveDir);
    char *p = path_of(g);
    ag_write_file(p, out, len);
    free(p);
    free(out);
}

void settings_close(AG *g) {
    for (int i = 0; i < nlines; i++) {
        free(lines[i].section);
        free(lines[i].key);
        free(lines[i].value);
    }
    free(lines);
    lines = NULL;
    nlines = 0;
}
