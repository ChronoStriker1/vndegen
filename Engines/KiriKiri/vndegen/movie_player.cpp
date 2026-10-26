// The engine's movie player. MovieDecoder decodes on its worker thread; everything here runs on the
// main thread, from the engine's calls and its continuous event hook, except the MovieAudioSink
// methods, which the worker calls.
//
// The clock is the engine's tick count, which stops while the engine is paused, kept within
// resyncThreshold of the audio actually played. Each tick shows the newest frame that is due.
#include "tjsCommHead.h"
#include "movie_player.h"
#include "movie_decoder.h"

#include "LayerBitmap.h"
#include "Platform.h"
#include "PlatformAudio.h"
#include "TVPCompositor.h"
#include "TVPDebug.h"
#include "TVPEvent.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

static const double resyncThreshold = 0.04; // seconds
// Sound buffer chunks. The decoder writes about half a second ahead, a few dozen packets.
static const int soundBufferChunks = 128;
// VideoOverlay.audioVolume runs from 0 to 100000, as DirectShow's attenuation did in krmovie.
static const double fullVolume = 100000;

static ttstr format(const char *pattern, ...)
{
    char text[512];
    va_list args;
    va_start(args, pattern);
    vsnprintf(text, sizeof text, pattern, args);
    va_end(args);
    return ttstr(text);
}

class MoviePlayer : public iTVPVideoOverlay, public tTVPContinuousEventCallbackIntf, public MovieAudioSink {
public:
    MoviePlayer(iTVPVideoCallback *callback, std::unique_ptr<MovieDecoder> decoder, const ttstr &name,
                MovieMode mode);
    ~MoviePlayer();

    void AddRef() override { refCount++; }
    void Release() override
    {
        if (--refCount == 0)
            delete this;
    }

    void SetWindow(class tTJSNI_Window *value) override
    {
        window = value;
        UpdateSpriteVisibility();
    }
    void SetMessageDrainWindow(void *) override {}
    void SetRect(int l, int t, int r, int b) override
    {
        sprite.placed = r > l && b > t;
        sprite.placeLeft = l;
        sprite.placeTop = t;
        sprite.placeWidth = r - l;
        sprite.placeHeight = b - t;
    }
    void SetVisible(bool value) override
    {
        visible = value;
        UpdateSpriteVisibility();
    }
    void Play() override;
    void Stop() override;
    void Pause() override;
    void SetPosition(uint64_t tick) override { SeekTo(tick / 1000.0); }
    void GetPosition(uint64_t *tick) override { *tick = (uint64_t)std::llround(MovieTime() * 1000); }
    void GetStatus(tTVPVideoStatus *status) override
    {
        *status = playing ? vsPlaying : ended ? vsEnded : paused ? vsPaused : vsStopped;
    }

    void Rewind() override { SeekTo(0); }
    void SetFrame(int f) override { SeekTo(f / info.fps); }
    void GetFrame(int *f) override { *f = shownFrame; }
    void GetFPS(double *f) override { *f = info.fps; }
    void GetNumberOfFrame(int *f) override { *f = (int)std::lround(info.duration * info.fps); }
    void GetTotalTime(int64_t *t) override { *t = (int64_t)std::llround(info.duration * 1000); }

    void GetVideoSize(long *width, long *height) override
    {
        *width = info.width;
        *height = info.height;
    }
    tTVPBaseTexture *GetFrontBuffer() override { return buffers[front]; }
    void SetVideoBuffer(tTVPBaseTexture *buff1, tTVPBaseTexture *buff2, long) override
    {
        buffers[0] = buff1;
        buffers[1] = buff2;
        front = 0;
    }

    void SetStopFrame(int frame) override { stopFrame = frame; }
    void GetStopFrame(int *frame) override
    {
        if (stopFrame >= 0)
            *frame = stopFrame;
        else
            GetNumberOfFrame(frame);
    }
    void SetDefaultStopFrame() override { stopFrame = -1; }

    // Movies play at their own speed; the rate is kept for scripts that read it back.
    void SetPlayRate(double rate) override { playRate = rate; }
    void GetPlayRate(double *rate) override { *rate = playRate; }

    // The engine's sound buffers have no panning.
    void SetAudioBalance(long value) override { balance = value; }
    void GetAudioBalance(long *value) override { *value = balance; }
    void SetAudioVolume(long value) override
    {
        volume = value;
        ApplyVolume();
    }
    void GetAudioVolume(long *value) override { *value = volume; }

    // Only the first audio stream plays.
    void GetNumberOfAudioStream(unsigned long *count) override { *count = info.audioStreams; }
    void SelectAudioStream(unsigned long) override {}
    void GetEnableAudioStreamNum(long *number) override { *number = sound && !audioDisabled ? 0 : -1; }
    void DisableAudioStream() override
    {
        audioDisabled = true;
        ApplyVolume();
    }

    void GetNumberOfVideoStream(unsigned long *count) override { *count = 1; }
    void SelectVideoStream(unsigned long) override {}
    void GetEnableVideoStreamNum(long *number) override { *number = 0; }

    void SetMixingBitmap(class tTVPBaseTexture *, float) override {}
    void ResetMixingBitmap() override {}
    void SetMixingMovieAlpha(float a) override { mixingAlpha = a; }
    void GetMixingMovieAlpha(float *a) override { *a = mixingAlpha; }
    void SetMixingMovieBGColor(unsigned long color) override { mixingColor = color; }
    void GetMixingMovieBGColor(unsigned long *color) override { *color = mixingColor; }
    void PresentVideoImage() override {}

    // Picture adjustments are not supported, so every value reads as 0.
    void GetContrastRangeMin(float *v) override { *v = 0; }
    void GetContrastRangeMax(float *v) override { *v = 0; }
    void GetContrastDefaultValue(float *v) override { *v = 0; }
    void GetContrastStepSize(float *v) override { *v = 0; }
    void GetContrast(float *v) override { *v = 0; }
    void SetContrast(float) override {}
    void GetBrightnessRangeMin(float *v) override { *v = 0; }
    void GetBrightnessRangeMax(float *v) override { *v = 0; }
    void GetBrightnessDefaultValue(float *v) override { *v = 0; }
    void GetBrightnessStepSize(float *v) override { *v = 0; }
    void GetBrightness(float *v) override { *v = 0; }
    void SetBrightness(float) override {}
    void GetHueRangeMin(float *v) override { *v = 0; }
    void GetHueRangeMax(float *v) override { *v = 0; }
    void GetHueDefaultValue(float *v) override { *v = 0; }
    void GetHueStepSize(float *v) override { *v = 0; }
    void GetHue(float *v) override { *v = 0; }
    void SetHue(float) override {}
    void GetSaturationRangeMin(float *v) override { *v = 0; }
    void GetSaturationRangeMax(float *v) override { *v = 0; }
    void GetSaturationDefaultValue(float *v) override { *v = 0; }
    void GetSaturationStepSize(float *v) override { *v = 0; }
    void GetSaturation(float *v) override { *v = 0; }
    void SetSaturation(float) override {}

    // VideoOverlay loops by rewinding on EC_COMPLETE, and runs segment loops itself.
    void SetLoopSegement(int, int) override {}

    void OnContinuousCallback(tjs_uint64) override;

    size_t Queued() override { return sound->GetQueuedSamples(); }
    void Write(const int16_t *samples, size_t frames, double pts) override;

private:
    double MovieTime();
    void SeekTo(double t);
    void Halt();
    void Show(const MovieFrame &frame);
    void Finish();
    bool AudioDone();
    void ApplyVolume();
    void UpdateSpriteVisibility() { sprite.isVisible = visible && window && hasFrame; }
    void Post(int message, intptr_t wparam, intptr_t lparam);
    void PostState(tTVPVideoStatus status);
    void LogErrors();
    void LogSummary(const char *event);

    iTVPVideoCallback *callback;
    std::unique_ptr<MovieDecoder> decoder;
    const MovieInfo info;
    const ttstr name;
    const MovieMode mode;
    int refCount = 1;
    iTVPSoundBuffer *sound = nullptr;

    bool playing = false, paused = false, ended = false;
    double clockPts = 0;     // movie time at clockTick
    uint64_t clockTick = 0;  // TVPGetRoughTickCount
    double shownPts = -1;
    int shownFrame = 0;
    int stopFrame = -1;
    int64_t shownCount = 0, droppedCount = 0;

    // Written by the worker in Write, read by MovieTime.
    std::mutex audioMutex;
    double audioEnd = 0;      // pts just past the last sample written
    bool audioWritten = false; // since the last seek

    tTVPBaseTexture *buffers[2] = {};
    int front = 0;
    TVPSprite sprite;
    class tTJSNI_Window *window = nullptr;
    bool visible = false, hasFrame = false;

    double playRate = 1;
    long balance = 0, volume = (long)fullVolume;
    bool audioDisabled = false;
    float mixingAlpha = 1;
    unsigned long mixingColor = 0xff000000;
};

MoviePlayer::MoviePlayer(iTVPVideoCallback *callback, std::unique_ptr<MovieDecoder> decoder,
                         const ttstr &name, MovieMode mode)
  : callback(callback), decoder(std::move(decoder)), info(this->decoder->Info()), name(name), mode(mode)
{
    sprite.type = 2;
    if (info.audioRate) {
        tTVPWaveFormat waveFormat = {};
        waveFormat.SamplesPerSec = info.audioRate;
        waveFormat.Channels = info.audioChannels;
        waveFormat.BitsPerSample = 16;
        waveFormat.BytesPerSample = 2;
        sound = TVPCreateSoundBuffer(waveFormat, soundBufferChunks);
        if (sound) {
            // A new buffer's device is already running, and would play the audio decoded ahead.
            sound->Play();
            sound->Pause();
        }
    }
    static const char *modeNames[] = {"overlay", "layer", "mixer", "Media Foundation"};
    TVPAddImportantLog(ttstr(TJS_N("movie: opened ")) + name +
                       format(" (%s), %.3f fps, %.2f s, %s mode%s%s", info.format.c_str(), info.fps,
                              info.duration, modeNames[(int)mode],
                              info.hardware ? ", VideoToolbox decoding" : "",
                              info.audioRate && !sound ? ", no sound output" : ""));
    this->decoder->Start(sound ? this : nullptr);
}

MoviePlayer::~MoviePlayer()
{
    TVPRemoveContinuousEventHook(this);
    LogErrors();
    if (shownCount && !ended)
        LogSummary("closed");
    // The worker writes to the sound buffer until it stops.
    decoder.reset();
    if (sound)
        sound->Release();
    if (sprite.texture) {
        krkrsdl3::TVPDepartTexture(&sprite);
        krkrsdl3::TVPDestroyTexture(&sprite);
    }
}

void MoviePlayer::Write(const int16_t *samples, size_t frames, double pts)
{
    std::lock_guard<std::mutex> lock(audioMutex);
    sound->AppendBuffer(samples, (unsigned)(frames * info.audioChannels * sizeof(int16_t)));
    audioEnd = pts + frames / (double)info.audioRate;
    audioWritten = true;
}

double MoviePlayer::MovieTime()
{
    if (!playing)
        return clockPts;
    uint64_t now = TVPGetRoughTickCount();
    double t = clockPts + (now - clockTick) / 1000.0;
    if (!sound)
        return t;
    // Read before audioMutex: the worker takes the decoder's lock, then audioMutex.
    bool audioEnded = decoder->AudioEnded();
    std::lock_guard<std::mutex> lock(audioMutex);
    if (!audioWritten)
        return t;
    size_t queued = sound->GetQueuedSamples();
    if (audioEnded && !queued)
        return t; // the audio is over; the video runs on the tick count
    double audioTime = audioEnd - queued / (double)info.audioRate;
    if (std::fabs(audioTime - t) > resyncThreshold) {
        clockPts = audioTime;
        clockTick = now;
        t = audioTime;
    }
    return t;
}

void MoviePlayer::Play()
{
    if (playing)
        return;
    if (ended)
        SeekTo(0);
    playing = true;
    paused = false;
    clockTick = TVPGetRoughTickCount();
    if (sound)
        sound->Play();
    TVPAddContinuousEventHook(this);
    PostState(vsPlaying);
}

void MoviePlayer::Halt()
{
    if (!playing)
        return;
    clockPts = MovieTime();
    playing = false;
    if (sound)
        sound->Pause();
    TVPRemoveContinuousEventHook(this);
}

void MoviePlayer::Pause()
{
    if (!playing)
        return;
    Halt();
    paused = true;
    PostState(vsPaused);
}

void MoviePlayer::Stop()
{
    Halt();
    paused = false;
    PostState(vsStopped);
}

void MoviePlayer::SeekTo(double t)
{
    t = std::clamp(t, 0.0, std::max(info.duration, 0.0));
    decoder->Seek(t, [&] {
        if (!sound)
            return;
        std::lock_guard<std::mutex> lock(audioMutex);
        sound->Stop(); // clears the queued audio
        if (playing)
            sound->Play();
        audioEnd = t;
        audioWritten = false;
    });
    clockPts = t;
    clockTick = TVPGetRoughTickCount();
    shownPts = -1;
    shownFrame = (int)std::lround(t * info.fps);
    ended = false;
}

void MoviePlayer::OnContinuousCallback(tjs_uint64)
{
    if (!playing)
        return;
    LogErrors();
    double now = MovieTime();
    MovieFrame frame;
    bool got = false;
    for (double pts; (pts = decoder->PeekFramePts()) >= 0 && pts <= now;) {
        if (got) {
            decoder->Recycle(std::move(frame));
            droppedCount++;
        }
        got = decoder->PopFrame(frame);
    }
    if (got) {
        Show(frame);
        decoder->Recycle(std::move(frame));
    }
    bool stopReached = stopFrame >= 0 && shownFrame >= stopFrame;
    bool lastFrameDone = decoder->VideoEnded() && now >= shownPts + 1 / info.fps;
    if (stopReached || (lastFrameDone && AudioDone()))
        Finish();
}

void MoviePlayer::Show(const MovieFrame &frame)
{
    shownPts = frame.pts;
    shownFrame = (int)std::lround(frame.pts * info.fps);
    shownCount++;
    const size_t rowBytes = (size_t)info.width * 4;
    if (mode == MovieMode::Layer) {
        tTVPBaseTexture *back = buffers[1 - front];
        if (!back)
            return;
        for (int y = 0; y < info.height; y++)
            memcpy(back->GetScanLineForWrite(y), frame.pixels.data() + y * rowBytes, rowBytes);
        front = 1 - front;
        Post(WM_GRAPHNOTIFY, EC_UPDATE, shownFrame);
        return;
    }
    if (!sprite.texture) {
        sprite.width = info.width;
        sprite.height = info.height;
        krkrsdl3::TVPCreateTexture(sprite);
        if (!sprite.texture)
            return;
        krkrsdl3::TVPJoinTexture(&sprite);
    }
    krkrsdl3::TVPUpdateTexture(&sprite, (uint8_t *)frame.pixels.data(), info.width, info.height,
                               (int)rowBytes);
    hasFrame = true;
    UpdateSpriteVisibility();
    if (mode == MovieMode::Mixer)
        Post(WM_GRAPHNOTIFY, EC_UPDATE, shownFrame);
}

bool MoviePlayer::AudioDone()
{
    return !sound || (decoder->AudioEnded() && sound->GetQueuedSamples() == 0);
}

void MoviePlayer::Finish()
{
    Halt();
    ended = true;
    LogSummary("ended");
    if (mode == MovieMode::MediaFoundation)
        PostState(vsEnded);
    else
        Post(WM_GRAPHNOTIFY, EC_COMPLETE, 0);
}

void MoviePlayer::ApplyVolume()
{
    if (sound)
        sound->SetVolume(audioDisabled ? 0 : (float)std::clamp(volume / fullVolume, 0.0, 1.0));
}

void MoviePlayer::Post(int message, intptr_t wparam, intptr_t lparam)
{
    if (!callback)
        return;
    NativeEvent event(message);
    event.WParam = wparam;
    event.LParam = lparam;
    callback->PostEvent(event);
}

// Only Media Foundation players report their status; VideoOverlay sets it for the others.
void MoviePlayer::PostState(tTVPVideoStatus status)
{
    if (mode == MovieMode::MediaFoundation)
        Post(WM_STATE_CHANGE, status, 0);
}

void MoviePlayer::LogErrors()
{
    std::string error = decoder->TakeError();
    if (!error.empty())
        TVPAddImportantLog(ttstr(TJS_N("movie: ")) + name + TJS_N(": ") + ttstr(error.c_str()));
}

void MoviePlayer::LogSummary(const char *event)
{
    MovieStats stats = decoder->Stats();
    TVPAddImportantLog(ttstr(TJS_N("movie: ")) + name +
                       format(" %s at %.2f s: %lld frames shown, %lld dropped, %lld decoded, %.2f s of audio",
                              event, MovieTime(), (long long)shownCount, (long long)droppedCount,
                              (long long)stats.videoFrames,
                              info.audioRate ? stats.audioFrames / (double)info.audioRate : 0.0));
}

iTVPVideoOverlay *VNKKCreateMoviePlayer(iTVPVideoCallback *callback, tTJSBinaryStream *stream,
                                        const ttstr &name, MovieMode mode)
{
    std::string error;
    std::unique_ptr<MovieDecoder> decoder = MovieDecoder::Open(stream, error);
    if (!decoder) {
        TVPAddImportantLog(ttstr(TJS_N("movie: cannot play ")) + name + TJS_N(": ") + ttstr(error.c_str()));
        return nullptr;
    }
    return new MoviePlayer(callback, std::move(decoder), name, mode);
}
