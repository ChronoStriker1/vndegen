// Wolf RPG Editor's sound: one BGM, one BGS and any number of sound effects, mixed to float stereo.
//
// The host pulls the mix with render() from its own audio thread (AVAudioEngine on Apple platforms, SDL in the Linux
// test tool). Commands come from the game thread. Compressed files stay compressed in memory and a decoding thread
// decodes them a little ahead into a buffer per sound, so render() only mixes: it doesn't allocate, read files or
// decode, and holds a lock only as long as the game thread's commands take to change a few fields. Without the
// decoding thread, render() decodes on the calling thread, so tests get the same samples every run.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace wolf {

class GameFiles;

enum class SoundKind { BGM, BGS, SE };

// The parameters of Wolf's sound command and the BGM and BGS lists in the system database.
struct MusicParams {
    int volume = 100;     // percent of the file's own level
    int frequency = 100;  // percent; playback rate for sampled files (changes pitch), tempo for MIDI. 0 means 100
    // Where the track loops back to when it reaches its end, in ms. 0 uses the file's own LOOPSTART/LOOPLENGTH Ogg
    // comments or MIDI CC111 marker, else the start. For MIDI files Wolf's list labels this field "key ±", so it is
    // a transposition in semitones there instead.
    int loopStartMs = 0;
    int fadeFrames = 0;  // fade-in time in game frames
    int startMs = 0;     // where playback starts
};

class Audio {
public:
    using Log = std::function<void(const std::string &)>;

    // `sampleRate` is the output rate; use the hardware's to save a second resampling. `decodeThread` false is for
    // tests: render() then decodes as well. `log` gets one line for a missing or undecodable file and for MIDI
    // without a SoundFont, on the game thread. The host stops calling render() before destroying the Audio.
    explicit Audio(const GameFiles &files, int sampleRate = 44100, bool decodeThread = true, Log log = {});
    ~Audio();
    Audio(const Audio &) = delete;
    Audio &operator=(const Audio &) = delete;

    // The General MIDI SoundFont (.sf2 or .sf3) for MIDI files, and one to load instead when it can't be read, such as
    // the bundled font behind one the player chose. Without one, MIDI files play silently. Defaults to the
    // WOLF_SOUNDFONT environment variable. The font loads on the first MIDI file.
    void setSoundFontPath(std::string path, std::string fallback = {});
    // Game frames per second, which fade and delay times count in. Wolf games run at 60 or 30.
    void setFrameRate(int fps);

    // BGM or BGS. `path` is relative to Data, as GameFiles takes it. When the same file is already playing on that
    // channel and `restart` is false, it keeps playing and takes the new volume and frequency. A different file
    // replaces the current one at once. Returns false when the file can't be played; the channel is then silent.
    bool playMusic(SoundKind channel, const std::string &path, const MusicParams &params, bool restart = false);
    void stopMusic(SoundKind channel, int fadeFrames = 0);
    // The playing file's path as passed to playMusic. Empty after stopMusic, even while the old track fades out.
    std::string musicPath(SoundKind channel) const;
    // Playback position in ms within the file, following loops; 0 when nothing plays.
    int musicPositionMs(SoundKind channel) const;

    // Sound effects play once, side by side. `delayFrames` is Wolf's SE delay.
    bool playSE(const std::string &path, int volume = 100, int frequency = 100, int delayFrames = 0);
    void stopAllSE();

    // The host's volume for each kind, 0 to 1, on top of the game's own volumes.
    void setMasterVolume(SoundKind kind, float volume);
    // Pausing freezes every sound where it is, fades included, and render() outputs silence. The app pauses when it
    // goes to the background.
    void pause();
    void resume();

    // Writes the next `frames` frames of interleaved stereo float at sampleRate() into `out`, silence while paused.
    // Call it from one thread at a time, normally the host's real-time audio thread.
    void render(float *out, int frames);
    int sampleRate() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wolf
