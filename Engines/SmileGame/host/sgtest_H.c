#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "png.h"
#include "sg.h"

static int failures, checks;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(cond)) {                                                                                                 \
            failures++;                                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
        }                                                                                                              \
    } while (0)

enum { VK_UP = 0x26, VK_DOWN = 0x28, VK_LEFT = 0x25, VK_RIGHT = 0x27, VK_OK = 0x0d, VK_CANCEL = 0x1b };

static void run(SG *g, int ms) {
    for (int t = 0; t < ms; t += 16) vnsmilegame_frame((VNSmileGame *)g, 16);
}

static void press(SG *g, int vk, int holdMs) {
    vnsmilegame_key((VNSmileGame *)g, vk, true);
    run(g, holdMs);
    vnsmilegame_key((VNSmileGame *)g, vk, false);
    run(g, 32);
}

static int vk_of(char d) {
    switch (d) {
    case 'U': return VK_UP;
    case 'D': return VK_DOWN;
    case 'L': return VK_LEFT;
    default: return VK_RIGHT;
    }
}

static const char *scene(SG *g) { return vnsmilegame_scene((VNSmileGame *)g); }

static bool in_scene(SG *g, const char *s) { return strcmp(scene(g), s) == 0; }

static bool until_scene(SG *g, const char *want, int maxPresses) {
    for (int i = 0; i < maxPresses; i++) {
        for (int t = 0; t < 1500 && in_scene(g, "event"); t += 16) run(g, 16);
        if (in_scene(g, want)) return true;
        if (in_scene(g, "message") || in_scene(g, "scroll") || (g->interp.active && g->interp.state == 4)) press(g, VK_OK, 48);
        run(g, 300);
    }
    return in_scene(g, want);
}

static bool showing(SG *g, const char *text) {
    Interp *ip = &g->interp;
    return ip->active && ip->state == 1 && ip->page < ip->pageCount && strstr(ip->pages[ip->page], text);
}

static bool upto(SG *g, const char *text, int maxPresses) {
    for (int i = 0; i < maxPresses; i++) {
        for (int t = 0; t < 3000 && !showing(g, text) && !in_scene(g, "message"); t += 16) run(g, 16);
        if (showing(g, text)) return true;
        if (g->interp.active) press(g, VK_OK, 48);
        run(g, 250);
    }
    return false;
}

static bool blocked(SG *g, int x, int z) {
    for (int i = 0; i < g->field.npcCount; i++)
        if (g->field.npcs[i].visible && (int)floorf(g->field.npcs[i].x) == x && (int)floorf(g->field.npcs[i].z) == z) return true;
    return false;
}

static bool step_ok(SG *g, int x0, int z0, int x1, int z1) {
    const Map *m = &g->field.map;
    if (x1 < 0 || z1 < 0 || x1 >= m->w || z1 >= m->h) return false;
    if (!sgi_cell_free(g, x0, z0, x1, z1, -1)) return false;
    return !blocked(g, x1, z1);
}

static bool walk_to(SG *g, int tx, int tz, bool dry) {
    const Map *m = &g->field.map;
    int n = m->w * m->h;
    int *prev = malloc(sizeof(int) * (size_t)n), *q = malloc(sizeof(int) * (size_t)n), qh = 0, qt = 0;
    for (int i = 0; i < n; i++) prev[i] = -2;
    int sx = (int)floorf(g->field.player.x), sz = (int)floorf(g->field.player.z);
    prev[sz * m->w + sx] = -1;
    q[qt++] = sz * m->w + sx;
    static const int dx[4] = {0, 1, 0, -1}, dz[4] = {-1, 0, 1, 0};
    while (qh < qt) {
        int c = q[qh++], cx = c % m->w, cz = c / m->w;
        if (cx == tx && cz == tz) break;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d], nz = cz + dz[d];
            if (nx < 0 || nz < 0 || nx >= m->w || nz >= m->h || prev[nz * m->w + nx] != -2) continue;
            if (!step_ok(g, cx, cz, nx, nz)) continue;
            prev[nz * m->w + nx] = c;
            q[qt++] = nz * m->w + nx;
        }
    }
    bool ok = prev[tz * m->w + tx] != -2;
    if (ok && dry) {
        int path[4096], len = 0, px = sx, pz = sz;
        for (int c = tz * m->w + tx; c != sz * m->w + sx && len < 4096; c = prev[c]) path[len++] = c;
        for (int i = len - 1; i >= 0; i--) {
            int nx = path[i] % m->w, nz = path[i] / m->w;
            putchar(nx > px ? 'R' : nx < px ? 'L' : nz > pz ? 'D' : 'U');
            px = nx, pz = nz;
        }
        putchar('\n');
    } else if (ok) {
        int path[4096], len = 0;
        for (int c = tz * m->w + tx; c != sz * m->w + sx && len < 4096; c = prev[c]) path[len++] = c;
        for (int i = len - 1; i >= 0 && ok; i--) {
            int nx = path[i] % m->w, nz = path[i] / m->w;
            int px = (int)floorf(g->field.player.x), pz = (int)floorf(g->field.player.z);
            int vk = nx > px ? VK_RIGHT : nx < px ? VK_LEFT : nz > pz ? VK_DOWN : VK_UP;
            vnsmilegame_key((VNSmileGame *)g, vk, true);
            int t = 0;
            for (; t < 2000; t += 16) {
                run(g, 16);
                if (g->interp.active || sg_battle_active(g)) break;
                if ((int)floorf(g->field.player.x) == nx && (int)floorf(g->field.player.z) == nz && !g->field.player.moving) break;
            }
            vnsmilegame_key((VNSmileGame *)g, vk, false);
            run(g, 16);
            if (g->interp.active || sg_battle_active(g)) break;
            if (t >= 2000) ok = false;
        }
    }
    free(prev);
    free(q);
    return ok;
}

static const HeroDef *hero_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->heroCount; i++)
        if (strcmp(t->heroes[i].name, name) == 0) return &t->heroes[i];
    return NULL;
}

static const MonsterDef *monster_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->monsterCount; i++)
        if (strcmp(t->monsters[i].name, name) == 0) return &t->monsters[i];
    return NULL;
}

static void where(SG *g, const char *tag) {
    Party *p = &sgb(g)->party;
    printf("%s: %s map %s at %d,%d dir %d", tag, scene(g), g->field.map.name ? g->field.map.name : "-", (int)floorf(g->field.player.x),
           (int)floorf(g->field.player.z), g->field.player.dir);
    if (sg_battle_active(g)) printf(" battle %s [%s]", battle_phase(g), battle_bar(g));
    printf(" result %d money %d", sg_battle_result(g), p->money);
    for (int i = 0; i < p->count; i++) {
        const HeroDef *h = hero_def(&sgb(g)->tables, p->members[i].hero);
        printf(" | %s L%d HP%d MP%d EXP%d", h ? h->name : "?", p->members[i].level, p->members[i].hp, p->members[i].mp, p->members[i].exp);
    }
    Guid bgm;
    if (snd_bgm_playing(g, &bgm)) {
        const Res *r = res_find(&g->res, bgm);
        printf(" bgm %s", r ? r->name : "?");
    }
    printf("\n");
}

static bool fight(SG *g, const char *how, int maxTurns) {
    for (int turn = 0; turn < maxTurns && sg_battle_active(g); turn++) {
        for (int t = 0; t < 30000 && sg_battle_active(g); t += 16) {
            const char *ph = battle_phase(g);
            if (!strcmp(ph, "command") || !strcmp(ph, "result") || !strcmp(ph, "gameover")) break;
            run(g, 16);
        }
        if (!sg_battle_active(g)) break;
        const char *ph = battle_phase(g);
        if (!strcmp(ph, "result") || !strcmp(ph, "gameover")) {
            run(g, 1600);
            press(g, VK_OK, 48);
            run(g, 200);
            continue;
        }
        if (!strcmp(how, "escape")) {
            for (int k = 0; k < 8; k++) press(g, VK_UP, 48);
            press(g, VK_OK, 48);
        } else if (!strcmp(how, "defend")) {
            press(g, VK_DOWN, 48);
            press(g, VK_OK, 48);
        } else {
            press(g, VK_OK, 48);
            press(g, VK_OK, 48);
        }
        run(g, 100);
    }
    return !sg_battle_active(g);
}

static bool start_script(SG *g, const char *hex);

static int drive(int argc, char **argv) {
    char log[1024];
    snprintf(log, sizeof log, "%s/drive.log", argv[3]);
    VNSmileGameConfig cfg = {.gameDir = argv[2], .saveDir = argv[3], .logPath = log, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) {
        fprintf(stderr, "open failed: %s\n", err);
        return 1;
    }
    run(g, 16);
    if (g->titleCursor == 1) press(g, VK_UP, 48);
    int rc = 0;
    for (int i = 4; i < argc && !rc; i++) {
        const char *c = argv[i];
        int a, b, d;
        char s[256];
        sg_log(g, "== %s", c);
        if (sscanf(c, "wait:%d", &a) == 1) run(g, a);
        else if (!strcmp(c, "ok")) press(g, VK_OK, 48), run(g, 200);
        else if (!strcmp(c, "cancel")) press(g, VK_CANCEL, 48), run(g, 200);
        else if (!strcmp(c, "new")) press(g, VK_OK, 48), run(g, 500);
        else if (sscanf(c, "key:%c", s) == 1) press(g, vk_of(s[0]), 48), run(g, 300);
        else if (sscanf(c, "hold:%c,%d", s, &a) == 2) press(g, vk_of(s[0]), a);
        else if (sscanf(c, "until:%255s", s) == 1) {
            if (!until_scene(g, s, 400)) where(g, "until failed"), rc = 1;
        } else if (!strncmp(c, "upto:", 5)) {
            if (!upto(g, c + 5, 300)) where(g, "upto failed"), rc = 1;
        } else if (sscanf(c, "pick:%d", &a) == 1) {
            if (!until_scene(g, "event", 1) && !(g->interp.active && g->interp.state == 4)) {
                for (int k = 0; k < 100 && !(g->interp.active && g->interp.state == 4); k++) press(g, VK_OK, 48), run(g, 250);
            }
            for (int k = 0; k < a; k++) press(g, VK_DOWN, 48), run(g, 60);
            press(g, VK_OK, 48);
            run(g, 200);
        } else if (sscanf(c, "to:%d,%d", &a, &b) == 2) {
            if (!walk_to(g, a, b, false)) where(g, "walk failed"), rc = 1;
            run(g, 100);
        } else if (sscanf(c, "path:%d,%d", &a, &b) == 2) {
            walk_to(g, a, b, true);
        } else if (sscanf(c, "talk:%c", s) == 1) {
            press(g, vk_of(s[0]), 48);
            run(g, 100);
            press(g, VK_OK, 48);
            run(g, 300);
        } else if (sscanf(c, "script:%255s", s) == 1) {
            if (!start_script(g, s)) rc = 1;
            run(g, 100);
        } else if (sscanf(c, "phase:%255s", s) == 1) {
            for (int t = 0; t < 60000 && sg_battle_active(g) && strcmp(battle_phase(g), s) != 0; t += 16) {
                const char *ph = battle_phase(g);
                if (!strcmp(ph, "command") || !strcmp(ph, "target")) press(g, VK_OK, 48);
                else run(g, 16);
            }
            if (strcmp(battle_phase(g), s) != 0) where(g, "phase failed"), rc = 1;
        } else if (sscanf(c, "fight:%255s", s) == 1) {
            if (!fight(g, s, 60)) where(g, "fight unfinished"), rc = 1;
        } else if (sscanf(c, "var:%d=%d", &a, &b) == 2) g->vars[a] = b;
        else if (sscanf(c, "sw:%d=%d", &a, &b) == 2) g->switches[a] = (u8)b;
        else if (sscanf(c, "seed:%d", &a) == 1) battle_seed(g, (u32)a);
        else if (sscanf(c, "hp:%d=%d", &a, &b) == 2) sgb(g)->party.members[a].hp = b;
        else if (sscanf(c, "join:%255s", s) == 1) {
            const HeroDef *h = hero_named(g, s);
            if (h) party_add_hero(g, h->id);
            else rc = 1;
        } else if (sscanf(c, "warp:%255[^,],%d,%d,%d", s, &a, &b, &d) == 4) {
            if (!sg_debug_warp((VNSmileGame *)g, s, a, b, d)) rc = 1;
            run(g, 300);
        } else if (sscanf(c, "save:%d", &a) == 1) {
            if (!sg_save_write(g, a)) rc = 1;
        } else if (sscanf(c, "load:%d", &a) == 1) {
            if (!sg_save_read(g, a)) rc = 1;
            run(g, 500);
        } else if (sscanf(c, "shot:%255s", s) == 1) {
            char path[1400];
            snprintf(path, sizeof path, "%s/%s.png", argv[3], s);
            png_write(path, g->fb, g->w, g->h);
        } else if (sscanf(c, "where:%255s", s) == 1) where(g, s);
        else {
            fprintf(stderr, "unknown %s\n", c);
            rc = 2;
        }
    }
    where(g, "end");
    vnsmilegame_close((VNSmileGame *)g);
    return rc;
}

static SG *open_game(const char *dir) {
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = "/nonexistent-sgtest-h", .audioRate = 44100};
    char err[256];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static const SkillDef *skill_named(SG *g, const char *name) {
    Tables *t = &sgb(g)->tables;
    for (int i = 0; i < t->skillCount; i++)
        if (strcmp(t->skills[i].name, name) == 0) return &t->skills[i];
    return NULL;
}

static Guid map_named(SG *g, const char *name) {
    for (int f = 0; f < g->data.mapCount; f++)
        for (int i = 0; i < g->data.maps[f].count; i++) {
            const SgrRec *r = &g->data.maps[f].recs[i];
            if (r->type != SGR_MAP) continue;
            Map m;
            if (!map_parse(&m, r)) continue;
            bool hit = strcmp(m.name, name) == 0;
            Guid id = m.id;
            map_free(&m);
            if (hit) return id;
        }
    return (Guid){{0}};
}

static const char *res_name(SG *g, Guid id) {
    const Res *r = res_find(&g->res, id);
    return r ? r->name : "";
}

static const char *bgm_name(SG *g) {
    Guid id;
    return snd_bgm_playing(g, &id) ? res_name(g, id) : "";
}

static Member *member_named(SG *g, const char *name) {
    Party *p = &sgb(g)->party;
    for (int i = 0; i < p->count; i++) {
        const HeroDef *h = hero_def(&sgb(g)->tables, p->members[i].hero);
        if (h && strcmp(h->name, name) == 0) return &p->members[i];
    }
    return NULL;
}

static void new_game(SG *g) {
    run(g, 16);
    if (g->titleCursor == 1) press(g, VK_UP, 48);
    press(g, VK_OK, 48);
    run(g, 200);
    interp_free(&g->interp);
}

static int log_damage(const char *log, const char *who, int *out, int max) {
    FILE *f = fopen(log, "r");
    if (!f) return 0;
    char line[512], want[256];
    snprintf(want, sizeof want, "battle: %s takes %%d", who);
    int n = 0;
    while (fgets(line, sizeof line, f) && n < max)
        if (sscanf(line, want, &out[n]) == 1) n++;
    fclose(f);
    return n;
}

static void test_tables(const char *colette, const char *sacred) {
    if (colette) {
        SG *g = open_game(colette);
        CHECK(g != NULL);
        if (g) {
            const SkillDef *fang = skill_named(g, "毒牙ＬＶ01"), *drain = skill_named(g, "吸血ＬＶ01"), *panic = skill_named(g, "パニックボイスLV01");
            CHECK(fang && drain && panic);
            if (fang && drain && panic) {
                BattleSkillInfo k = battle_skill_info(fang);
                CHECK(k.states == 1 && k.hit == 90 && !k.drain);
                k = battle_skill_info(drain);
                CHECK(k.states == 0 && k.drain);
                k = battle_skill_info(panic);
                CHECK(k.states == 8 && k.hit == 78);
            }
            const MonsterDef *z = monster_named(g, "ゾンビーナＬＶ01");
            CHECK(z && z->exp == 32 && z->money == 13);
            vnsmilegame_close((VNSmileGame *)g);
        }
    }
    if (sacred) {
        SG *g = open_game(sacred);
        CHECK(g != NULL);
        if (!g) return;
        BattleMapInfo info;
        CHECK(battle_map_info(g, map_named(g, "王家の洞窟"), &info));
        CHECK(info.rate == 40 && info.count == 2);
        CHECK(guid_eq(info.entries[0].monster, monster_named(g, "スライミー")->id));
        CHECK(guid_eq(info.entries[1].monster, monster_named(g, "ミニゴーレム")->id));
        CHECK(guid_eq(info.background, map_named(g, "Cave")));
        BattleSetup none = {0};
        CHECK(strcmp(res_name(g, battle_bgm_for(g, &none, map_named(g, "王家の洞窟"))), "BGM_battle1") == 0);
        CHECK(guid_eq(battle_background_for(g, &none, map_named(g, "ルディアノ城")), map_named(g, "Default")));
        vnsmilegame_close((VNSmileGame *)g);
    }
}

static void test_colette_symbol(const char *dir, const char *tmp) {
    char log[1024];
    snprintf(log, sizeof log, "%s/colette-symbol.log", tmp);
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = tmp, .logPath = log, .audioRate = 44100};
    char err[256];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    CHECK(g != NULL);
    if (!g) return;
    new_game(g);
    party_add_hero(g, hero_named(g, "コレット")->id);
    g->switches[5] = 1;
    g->vars[9] = 2;
    CHECK(sg_debug_warp((VNSmileGame *)g, "ワールド", 20, 34, 0));
    run(g, 300);
    const char *worldBgm = bgm_name(g);
    battle_seed(g, 12);
    Member *dark = member_named(g, "ダーク"), *col = member_named(g, "コレット");
    CHECK(dark && col);
    if (!dark || !col) return;
    col->state = 1;
    static float pcm[44100 * 2];
    vnsmilegame_audio((VNSmileGame *)g, pcm, 44100);
    u32 worldAt = snd_bgm_position(g);
    CHECK(worldAt >= 44100);
    press(g, VK_UP, 48);
    run(g, 300);
    CHECK(showing(g, "モンスターめっ"));
    CHECK(until_scene(g, "battle", 10));
    run(g, 600);
    CHECK(strcmp(bgm_name(g), "BGM_battle1") == 0);
    CHECK(strcmp(g->field.map.name, "Grassland") == 0);
    int hp0 = col->hp;
    fight(g, "attack", 40);
    CHECK(!sg_battle_active(g) && sg_battle_result(g) == BATTLE_WON);
    run(g, 500);
    CHECK(strcmp(g->field.map.name, "ワールド") == 0);
    CHECK(strcmp(bgm_name(g), worldBgm) == 0);
    CHECK(snd_bgm_position(g) == worldAt);
    CHECK(g->switches[31] == 1);
    CHECK(dark->exp == 32 && col->exp == 32 && dark->level == 2 && col->level == 2);
    CHECK(member_max_hp(&sgb(g)->tables, dark) == 45 && member_max_hp(&sgb(g)->tables, col) == 41);
    CHECK(dark->hp <= 32 && col->hp < hp0);
    CHECK(sgb(g)->party.money == 13);
    CHECK(col->state == 1);
    const SkillDef *cure = skill_named(g, "キュアー");
    CHECK(cure && dark->learnedCount == 1 && guid_eq(dark->learned[0], cure->id) && col->learnedCount == 0);
    CHECK(sg_save_write(g, 7));
    char *path = sg_save_path(g, 7);
    size_t size = 0;
    u8 *data = sg_read_file(path, &size);
    free(path);
    SaveData sd;
    CHECK(data && save_parse(&sd, data, size));
    if (data && sd.partyCount == 2) {
        CHECK(sd.party[0].size == 204 && sd.party[1].size == 188);
        CHECK(sd.party[0].learnedCount == 1 && cure && guid_eq(sd.party[0].learned[0], cure->id));
        CHECK(sd.party[1].state == 1 && sd.party[1].learnedCount == 0);
        CHECK(sd.party[0].exp == 32 && sd.party[1].exp == 32);
        save_free(&sd);
    } else {
        CHECK(false);
    }
    free(data);
    vnsmilegame_close((VNSmileGame *)g);
    int dmg[64], n = log_damage(log, "ゾンビーナＬＶ01", dmg, 64);
    CHECK(n >= 6);
    for (int i = 0; i < n; i++) CHECK(dmg[i] == 0 || dmg[i] == 6 || dmg[i] == 7 || dmg[i] >= 12);
    n = log_damage(log, "コレット", dmg, 64);
    bool tick = false;
    for (int i = 0; i < n; i++) tick |= dmg[i] == 2;
    CHECK(tick);
}

static bool start_script(SG *g, const char *hex) {
    for (int f = 0; f < g->data.mapCount; f++)
        for (int i = 0; i < g->data.maps[f].count; i++) {
            const SgrRec *r = &g->data.maps[f].recs[i];
            if (r->type != SGR_EVENT || r->sub != 16 || r->size < 16) continue;
            char str[33];
            for (int k = 0; k < 16; k++) snprintf(str + k * 2, 3, "%02x", r->body[k]);
            if (strncmp(str, hex, strlen(hex)) != 0) continue;
            Script sc;
            if (!script_parse(&sc, r)) return false;
            interp_start(g, &sc);
            script_free(&sc);
            return true;
        }
    return false;
}

static void aria_party(SG *g) {
    Party *p = &sgb(g)->party;
    p->count = 0;
    party_add_hero(g, hero_named(g, "アリア")->id);
    Member *m = &p->members[0];
    member_set_level(&sgb(g)->tables, m, 2);
    m->exp = 26;
    m->hp = member_max_hp(&sgb(g)->tables, m);
    m->mp = member_max_mp(&sgb(g)->tables, m);
}

static void test_sacred_practice(const char *dir, const char *tmp, bool lose) {
    char log[1024];
    snprintf(log, sizeof log, "%s/sacred-practice.log", tmp);
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = tmp, .logPath = log, .audioRate = 44100};
    char err[256];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    CHECK(g != NULL);
    if (!g) return;
    new_game(g);
    aria_party(g);
    CHECK(sg_debug_warp((VNSmileGame *)g, "酒場", 5, 5, 2));
    run(g, 300);
    const char *castleBgm = bgm_name(g);
    battle_seed(g, 5);
    Member *aria = member_named(g, "アリア");
    CHECK(aria != NULL);
    if (!aria) return;
    if (lose) aria->hp = 1;
    CHECK(start_script(g, "c426b710"));
    run(g, 100);
    for (int k = 0; k < 20 && !(g->interp.active && g->interp.state == 4); k++) press(g, VK_OK, 48), run(g, 250);
    press(g, VK_DOWN, 48);
    run(g, 60);
    press(g, VK_OK, 48);
    run(g, 200);
    CHECK(until_scene(g, "battle", 10));
    run(g, 600);
    CHECK(strcmp(g->field.map.name, "Default") == 0);
    CHECK(strcmp(bgm_name(g), "BGM_battle1") == 0);
    CHECK(battle_monster_hp(g, 0) == 30);
    fight(g, "attack", 40);
    CHECK(sg_battle_result(g) == (lose ? BATTLE_LOST : BATTLE_WON));
    CHECK(g->scene == SCENE_FIELD);
    CHECK(upto(g, "お疲れ様でした", 20));
    CHECK(aria->hp == member_max_hp(&sgb(g)->tables, aria));
    CHECK(strcmp(bgm_name(g), castleBgm) == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_cave(const char *dir, const char *tmp) {
    char log[1024];
    snprintf(log, sizeof log, "%s/sacred-cave.log", tmp);
    VNSmileGameConfig cfg = {.gameDir = dir, .saveDir = tmp, .logPath = log, .audioRate = 44100};
    char err[256];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    CHECK(g != NULL);
    if (!g) return;
    new_game(g);
    aria_party(g);
    CHECK(sg_debug_warp((VNSmileGame *)g, "王家の洞窟", 11, 19, 2));
    run(g, 300);
    interp_free(&g->interp);
    battle_seed(g, 3);
    CmdArg off = {.kind = ARG_INT, .i = 1}, on = {.kind = ARG_INT, .i = 0};
    Cmd gate = {.type = 66, .args = &off, .argCount = 1};
    bool busy = false;
    CHECK(sgi_exec(g, &gate, &busy) && !sgi_encounters_on(g));
    for (int k = 0; k < 24; k++) walk_to(g, 11, k % 2 ? 19 : 16, false);
    CHECK(!sg_battle_active(g));
    gate.args = &on;
    CHECK(sgi_exec(g, &gate, &busy) && sgi_encounters_on(g));
    walk_to(g, 11, 19, false);
    int steps = 0;
    for (int k = 0; k < 40 && !sg_battle_active(g); k++) {
        walk_to(g, 11, k % 2 ? 19 : 16, false);
        steps += 3;
    }
    CHECK(sg_battle_active(g));
    CHECK(steps >= 30 && steps <= 54);
    run(g, 800);
    CHECK(strcmp(g->field.map.name, "Cave") == 0);
    CHECK(strcmp(bgm_name(g), "BGM_battle1") == 0);
    fight(g, "escape", 20);
    CHECK(!sg_battle_active(g));
    run(g, 300);
    CHECK(strcmp(g->field.map.name, "王家の洞窟") == 0);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(int argc, char **argv) {
    if (argc > 3 && strcmp(argv[1], "drive") == 0) return drive(argc, argv);
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    char tmp[64];
    snprintf(tmp, sizeof tmp, "/tmp/sgtest-h-%d", (int)getpid());
    if (mkdir(tmp, 0700) != 0) return 1;
    test_tables(colette, sacred);
    if (colette) test_colette_symbol(colette, tmp);
    if (sacred) {
        test_sacred_practice(sacred, tmp, false);
        test_sacred_practice(sacred, tmp, true);
        test_cave(sacred, tmp);
    }
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", tmp);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", tmp);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
