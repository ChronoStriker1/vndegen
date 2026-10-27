// Sound commands. Music plays on one channel (bgm, mp3, mp3loop...), wave/waveloop on another, and dwave on 50
// channels, of which channel 0 is the voice channel. Volumes are 0..100.
#include <cctype>

#include "engine.h"

namespace vnns {

bool Engine::readSound(const std::string &name, Bytes &out) {
    if (fs_.read(name, out)) return true;
    errorf("sound %s not found", sjisToUtf8(name).c_str());
    return false;
}

void Engine::playBgm(const std::string &spec, bool loop) {
    // "(5.37)file.ogg" loops back to 5.37 seconds.
    std::string name = spec;
    double loopStart = 0;
    if (!name.empty() && name[0] == '(') {
        size_t close = name.find(')');
        if (close != std::string::npos) {
            loopStart = atof(name.c_str() + 1);
            name = name.substr(close + 1);
        }
    }
    bgmName_ = spec;
    bgmLoop_ = loop;
    if (!name.empty() && name[0] == '*') {
        logf("CD audio track %s is not supported", name.c_str());
        return;
    }
    Bytes data;
    if (!readSound(name, data)) return;
    if (!audio_.play(AudioMixer::kBgm, std::move(data), loop, volBgm_, bgmFadeIn_, loopStart))
        errorf("music %s could not be played", sjisToUtf8(name).c_str());
}

void Engine::playSound(int channel, const std::string &name, bool loop) {
    if (channel < 0 || channel >= 50) return;
    if ((skipMode_ || ctrlSkip()) && !waveDuringSkip_ && channel == 0) return;
    Bytes data;
    if (!readSound(name, data)) return;
    int vol = channel == 0 ? volVoice_ : volSe_;
    if (chVol_[size_t(channel)] >= 0) vol = vol * chVol_[size_t(channel)] / 100;
    audio_.play(channel, std::move(data), loop, vol);
}

namespace {
using E = Engine;

void cmdBgm(E &e, const std::string &name) {
    std::string file = e.readStr();
    bool loop = name == "bgm" || name == "mp3loop" || name == "play" || name == "loopbgm_dummy";
    e.playBgm(file, loop);
}

void cmdBgmStop(E &e, const std::string &name) {
    e.audio_.stop(AudioMixer::kBgm, e.bgmFadeOut_);
    e.audio_.stop(AudioMixer::kLoopBgmTail, e.bgmFadeOut_);
    e.bgmName_.clear();
    if (name == "stop") {
        e.audio_.stop(AudioMixer::kWave);
        e.waveName_.clear();
    }
}

void cmdFade(E &e, const std::string &name) {
    int ms = e.readInt();
    if (name == "bgmfadeout" || name == "mp3fadeout") e.bgmFadeOut_ = ms;
    else e.bgmFadeIn_ = ms;
}

void cmdVolume(E &e, const std::string &name) {
    int v = e.readInt();
    if (name.find("bgm") != std::string::npos || name.find("mp3") != std::string::npos) {
        e.volBgm_ = v;
        e.audio_.setVolume(AudioMixer::kBgm, v);
    } else if (name.find("voice") != std::string::npos) {
        e.volVoice_ = v;
        e.audio_.setVolume(0, v);
    } else {
        e.volSe_ = v;
        for (int c = 1; c < 50; c++) e.audio_.setVolume(c, v);
        e.audio_.setVolume(AudioMixer::kWave, v);
    }
}

void cmdGetVolume(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    e.setVar(v, name == "getbgmvol" || name == "getmp3vol" ? e.volBgm_ : name == "getvoicevol" ? e.volVoice_ : e.volSe_);
}

void cmdChVol(E &e, const std::string &) {
    int ch = e.readInt();
    e.readComma();
    int v = e.readInt();
    if (ch < 0 || ch >= 50) return;
    e.chVol_[size_t(ch)] = v;
    e.audio_.setVolume(ch, (ch == 0 ? e.volVoice_ : e.volSe_) * v / 100);
}

void cmdWave(E &e, const std::string &name) {
    std::string file = e.readStr();
    if ((e.skipMode_ || e.ctrlSkip()) && !e.waveDuringSkip_) return;
    Bytes data;
    if (!e.readSound(file, data)) return;
    e.waveName_ = file;
    e.waveLoop_ = name == "waveloop";
    e.audio_.play(AudioMixer::kWave, std::move(data), e.waveLoop_, e.volSe_);
}

void cmdWaveStop(E &e, const std::string &) {
    e.audio_.stop(AudioMixer::kWave);
    e.waveName_.clear();
}

void cmdDwave(E &e, const std::string &name) {
    int ch = e.readInt();
    e.readComma();
    std::string file = e.readStr();
    bool loop = name == "dwaveloop";
    if (name == "dwaveload") {
        if (ch >= 0 && ch < 50) e.dwaveLoaded_[size_t(ch)] = file;
        return;
    }
    e.playSound(ch, file, loop);
    if (ch >= 0 && ch < 50 && loop) e.dwaveLoopName_[size_t(ch)] = file;
}

void cmdDwavePlay(E &e, const std::string &name) {
    int ch = e.readInt();
    if (ch < 0 || ch >= 50) return;
    e.playSound(ch, e.dwaveLoaded_[size_t(ch)], name == "dwaveplayloop");
}

void cmdDwaveStop(E &e, const std::string &) {
    int ch = e.readInt();
    if (ch >= 0 && ch < 50) {
        e.audio_.stop(ch);
        e.dwaveLoopName_[size_t(ch)].clear();
    }
}

void cmdLoopBgm(E &e, const std::string &) {
    // loopbgm "head","loop": the head plays once, then the loop file repeats.
    std::string head = e.readStr();
    e.readComma();
    std::string loop = e.readStr();
    e.loopBgmHead_ = head;
    e.loopBgmTail_ = loop;
    Bytes data;
    if (e.readSound(head, data)) e.audio_.play(AudioMixer::kBgm, std::move(data), false, e.volBgm_);
    e.loopBgmPending_ = true;
}

void cmdLoopBgmStop(E &e, const std::string &) {
    e.loopBgmPending_ = false;
    e.loopBgmHead_.clear();
    e.loopBgmTail_.clear();
    e.audio_.stop(AudioMixer::kBgm, e.bgmFadeOut_);
    e.audio_.stop(AudioMixer::kLoopBgmTail, e.bgmFadeOut_);
}

// v123: wave "wav\123.wav"; dv123: dwave 0,"wav\123.wav"; mv123: voice\123.mp3, which no runtime here decodes.
void cmdVoice(E &e, const std::string &name) {
    size_t d = 0;
    while (d < name.size() && isalpha(uint8_t(name[d]))) d++;
    std::string head = name.substr(0, d), number = name.substr(d);
    if (head == "dv") e.playSound(0, "wav\\" + number + ".wav", false);
    else if (head == "v") {
        Bytes data;
        if (e.readSound("wav\\" + number + ".wav", data)) e.audio_.play(AudioMixer::kWave, std::move(data), false, e.volVoice_);
    } else {
        logf("mv voices are MP3, which isn't supported");
    }
}

void cmdWaveDemo(E &e, const std::string &) { e.waveDuringSkip_ = true; }

}  // namespace

void registerSoundCommands(std::unordered_map<std::string, CommandFn> &t) {
    for (const char *n : {"bgm", "bgmonce", "mp3", "mp3loop", "mp3save", "play", "playonce"}) t[n] = cmdBgm;
    for (const char *n : {"bgmstop", "mp3stop", "playstop", "stop"}) t[n] = cmdBgmStop;
    for (const char *n : {"bgmfadeout", "mp3fadeout", "bgmfadein", "mp3fadein"}) t[n] = cmdFade;
    for (const char *n : {"bgmvol", "mp3vol", "voicevol", "sevol", "defbgmvol", "defmp3vol", "defvoicevol", "defsevol"})
        t[n] = cmdVolume;
    for (const char *n : {"getbgmvol", "getmp3vol", "getvoicevol", "getsevol"}) t[n] = cmdGetVolume;
    t["chvol"] = cmdChVol;
    t["wave"] = cmdWave;
    t["waveloop"] = cmdWave;
    t["wavestop"] = cmdWaveStop;
    for (const char *n : {"dwave", "dwaveloop", "dwaveload"}) t[n] = cmdDwave;
    t["dwaveplay"] = cmdDwavePlay;
    t["dwaveplayloop"] = cmdDwavePlay;
    t["dwavestop"] = cmdDwaveStop;
    t["loopbgm"] = cmdLoopBgm;
    t["loopbgmstop"] = cmdLoopBgmStop;
    t["__voice"] = cmdVoice;
    t["mode_wave_demo"] = cmdWaveDemo;
}

}  // namespace vnns
