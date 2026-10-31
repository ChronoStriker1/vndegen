// KiwiSoundEngine: music and sound channels in later games (Test Game 56). The calls match StoatSpriteEngine's
// Music_* and Sound_* ones; the data is a numbered entry, "00001.ogg", of the game's BA.afa (music) or WA.afa
// (sounds).
#include <cstdio>
#include <map>

#include "archive.h"
#include "game.h"

namespace s4 {
namespace {

Game &G() { return game(); }

// The groups the mixer volumes apply to, as SystemService numbers them.
constexpr int kMusicGroup = 1, kSoundGroup = 2, kVoiceGroup = 3;

struct Channel { int handle = 0; };
std::map<int, Channel> g_music, g_sound;

const ArchiveEntry *entryOf(AfaArchive *a, int number) {
    if (!a || number < 0) return nullptr;
    char name[32];
    std::snprintf(name, sizeof name, "%05d.ogg", number);
    return a->find(name);
}

int prepare(std::map<int, Channel> &chans, int ch, AfaArchive *a, int number, int group) {
    Channel &c = chans[ch];
    if (c.handle) G().audio.close(c.handle);
    c = {};
    const ArchiveEntry *e = entryOf(a, number);
    auto d = std::make_shared<std::vector<uint8_t>>();
    if (!e || !a->read(*e, *d)) return 0;
    c.handle = G().audio.open(d);
    if (c.handle) G().audio.setGroup(c.handle, group);
    return c.handle ? 1 : 0;
}
int handleOf(std::map<int, Channel> &chans, int ch) {
    auto it = chans.find(ch);
    return it == chans.end() ? 0 : it->second.handle;
}
void unprepare(std::map<int, Channel> &chans, int ch) {
    if (int h = handleOf(chans, ch)) G().audio.close(h);
    chans.erase(ch);
}

S4_HLL_RESET {
    for (auto *m : {&g_music, &g_sound})
        for (auto &[ch, c] : *m)
            if (c.handle) G().audio.close(c.handle);
    g_music.clear();
    g_sound.clear();
}

}  // namespace

S4_HLL(KiwiSoundEngine, SetGlobalFocus) {}

S4_HLL(KiwiSoundEngine, Music_IsExist) { c.retInt(entryOf(G().bgmAfa.get(), c.i(0)) ? 1 : 0); }
S4_HLL(KiwiSoundEngine, Music_Prepare) {
    int ok = prepare(g_music, c.i(0), G().bgmAfa.get(), c.i(1), kMusicGroup);
    if (ok) G().audio.setLoops(handleOf(g_music, c.i(0)), 0);
    c.retInt(ok);
}
S4_HLL(KiwiSoundEngine, Music_Unprepare) { unprepare(g_music, c.i(0)); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_Play) { G().audio.play(handleOf(g_music, c.i(0))); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_Stop) { G().audio.stop(handleOf(g_music, c.i(0))); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_IsPlay) { c.retInt(G().audio.playing(handleOf(g_music, c.i(0)))); }
S4_HLL(KiwiSoundEngine, Music_SetLoopCount) { G().audio.setLoops(handleOf(g_music, c.i(0)), c.i(1)); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_GetLoopCount) { c.retInt(G().audio.loops(handleOf(g_music, c.i(0)))); }
S4_HLL(KiwiSoundEngine, Music_Pause) { G().audio.pause(handleOf(g_music, c.i(0)), true); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_Restart) { G().audio.pause(handleOf(g_music, c.i(0)), false); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_IsPause) { c.retInt(G().audio.paused(handleOf(g_music, c.i(0)))); }
S4_HLL(KiwiSoundEngine, Music_Fade) { G().audio.fade(handleOf(g_music, c.i(0)), c.i(1), c.i(2), c.i(3)); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_StopFade) { G().audio.stopFade(handleOf(g_music, c.i(0))); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_IsFade) { c.retInt(G().audio.fading(handleOf(g_music, c.i(0)))); }
S4_HLL(KiwiSoundEngine, Music_GetPos) { c.retInt(int32_t(G().audio.positionMs(handleOf(g_music, c.i(0))))); }
S4_HLL(KiwiSoundEngine, Music_GetLength) { c.retInt(int32_t(G().audio.lengthMs(handleOf(g_music, c.i(0))))); }
S4_HLL(KiwiSoundEngine, Music_Seek) { G().audio.seekMs(handleOf(g_music, c.i(0)), c.i(1)); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Music_MillisecondsToSamples) { c.retInt(int32_t(int64_t(c.i(0)) * c.i(1) / 1000)); }

S4_HLL(KiwiSoundEngine, Sound_IsExist) { c.retInt(entryOf(G().waveAfa.get(), c.i(0)) ? 1 : 0); }
S4_HLL(KiwiSoundEngine, Sound_Prepare) { c.retInt(prepare(g_sound, c.i(0), G().waveAfa.get(), c.i(1), kSoundGroup)); }
S4_HLL(KiwiSoundEngine, Sound_Unprepare) { unprepare(g_sound, c.i(0)); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Sound_Play) { G().audio.play(handleOf(g_sound, c.i(0))); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Sound_Stop) { G().audio.stop(handleOf(g_sound, c.i(0))); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Sound_IsPlay) { c.retInt(G().audio.playing(handleOf(g_sound, c.i(0)))); }
S4_HLL(KiwiSoundEngine, Sound_Fade) { G().audio.fade(handleOf(g_sound, c.i(0)), c.i(1), c.i(2), c.i(3)); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Sound_StopFade) { G().audio.stopFade(handleOf(g_sound, c.i(0))); c.retInt(1); }
S4_HLL(KiwiSoundEngine, Sound_IsFade) { c.retInt(G().audio.fading(handleOf(g_sound, c.i(0)))); }
S4_HLL(KiwiSoundEngine, Sound_Seek) { G().audio.seekMs(handleOf(g_sound, c.i(0)), c.i(1)); c.retBool(true); }
S4_HLL(KiwiSoundEngine, Sound_GetTimeLength) { c.retInt(int32_t(G().audio.lengthMs(handleOf(g_sound, c.i(0))))); }
S4_HLL(KiwiSoundEngine, Sound_GetGroupNum) { c.retInt(kSoundGroup); }
S4_HLL(KiwiSoundEngine, Sound_GetGroupNumFromDataNum) { c.retInt(kSoundGroup); }
S4_HLL(KiwiSoundEngine, Sound_GetDataLength) {
    AfaArchive *a = G().waveAfa.get();
    const ArchiveEntry *e = entryOf(a, c.i(0));
    auto d = std::make_shared<std::vector<uint8_t>>();
    if (!e || !a->read(*e, *d)) { c.retInt(0); return; }
    int h = G().audio.open(d);
    c.retInt(h ? int32_t(G().audio.lengthMs(h)) : 0);
    if (h) G().audio.close(h);
}

S4_HLL(KiwiSoundEngine, GetMasterGroup) { c.retInt(0); }
S4_HLL(KiwiSoundEngine, GetBGMGroup) { c.retInt(kMusicGroup); }
S4_HLL(KiwiSoundEngine, GetSEGroup) { c.retInt(kSoundGroup); }
S4_HLL(KiwiSoundEngine, GetVoiceGroup) { c.retInt(kVoiceGroup); }
S4_HLL(KiwiSoundEngine, GetGimicSEGroup) { c.retInt(kSoundGroup); }
S4_HLL(KiwiSoundEngine, GetBackVoiceGroup) { c.retInt(kVoiceGroup); }

}  // namespace s4
