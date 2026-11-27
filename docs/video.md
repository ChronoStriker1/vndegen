# Movies

RPG Maker MV and MZ transparent VP9 WebM movies use a separate native converter, `vnalpha.[h,mm]`. It decodes color and alpha with libvpx and writes HEVC with alpha through AVAssetWriter, with Opus audio converted to AAC. Other audio codecs and non-I420 video are currently unsupported. Games keep their original files and media controls.

Conversion runs serially off the main thread. Other WebM requests, for clips without alpha or already converted, are served while a clip converts. The original request stays pending until the completed movie is renamed into the cache. The video then reloads with an explicit QuickTime source type so WebKit selects the HEVC reader for the original WebM URL. DOM videos stay hidden until their first decoded frame. PIXI uploads the corrected video through one reusable canvas because WebKit drops alpha on direct video uploads. Quitting cancels conversion and removes its partial file. The app logs each conversion's clip length, elapsed time and process peak resident memory, which includes earlier app activity.

The converter reads the clip from the descriptor the game folder opened, never by reopening a path, because an iOS app's sandbox may refuse that. A WebM without alpha is served as it is, and so is one the converter's parser rejects, which WebKit may still play. A clip that can't be read, or that the converter can't start on or convert, isn't served at all: unconverted, WebKit would draw its transparent parts opaque, often white over the whole scene and its text (#44). A missing clip leaves the scene as it was. Each outcome has a line in the app log starting `alpha video`: no alpha track (once per clip), can't be checked, not served, or the conversion's result. An alpha clip packed inside an Electron `app.asar` is still served unconverted.

A game can also decode a movie in its own page and play it from a blob URL, as Princess and Conquest's `Club_VideoDecrypt` does with its scrambled movies (#203). Such a clip never reaches the app as a file. When a page makes a blob URL for a `video/webm` Blob, the NW.js layer reads the Blob and hands its bytes to the app through the bridge, and the app checks them for alpha. A clip with alpha is kept for the session in its own folder under the app's temporary directory, named by the SHA-256 of its bytes, and the page gets `vndegen://game/.vndegen/alpha/<hash>.webm` in place of the blob URL. That address converts and serves the clip as a file clip is, cached under the same hash, so a later session reuses the conversion. A clip without alpha keeps its blob URL. The kept clips go when the game quits, and stay until then so that a clip the cache has evicted can convert again. Clips left by a session that ended without quitting, as when the app is closed from the app switcher, go when the app next keeps a clip. Movies in data URLs aren't converted.

The output pixel pool holds at most eight buffers, about 63 MiB at 1080p. Conversion waits when those buffers are in use. Decoder reference frames and the encoder's internal allocations are additional memory; the pool limit is not a limit on the whole conversion's memory.

Cold conversion can add substantial resident memory to the native app. A single 1080p check added about 164 MiB on a Mac and 389 MiB on an iPad simulator, beyond each paused game's baseline. Those figures exclude the game's separate WebContent heap. Physical iPad memory and conversion speed remain unmeasured; the simulator figures are not device estimates. Cached playback avoids this conversion cost.

The cache lives under the app's Caches directory, is excluded from backups, and evicts the least recently used movies before exceeding 512 MiB. Settings has a Clear video cache button. The old WebCodecs alpha pre-decode path is removed. The short opaque-loop smoothing path retains its separate frame budget.

Other looping WebMs reload at their end rather than use WebKit's loop, which stops about 500 ms at each turn and on an iPad simulator never finishes for a WebM with Vorbis sound. Alpha clips, kept frames, and MP4s keep WebKit's loop. See [frame-rate.md](frame-rate.md#looping-clips-in-tyranoscript-rpg-maker-mv-and-mz).

vnvideo is the movie decoder VNDegen's native engines share, in `Engines/Shared/vnvideo.[ch]`. It plays:

- MPEG-1 video with MPEG-1 Layer I or Layer II sound, through pl_mpeg (Layer I is VNDegen's addition; see
  `Engines/Shared/UPSTREAM.md`).
- MPEG-1 video with MP3 (Layer III) sound, as Spooky Pregnant School: The Quickening's movies have: pl_mpeg decodes the
  picture, and a second pl_mpeg demuxer, reading the file at its own position, hands the sound packets to dr_mp3.
- A bare MPEG-1 video stream, silent, with no program stream around it, as Magical Sword's `ConceLine.mpg` is: pl_mpeg's
  video decoder reads it directly. The file starts with a sequence header (`00 00 01 B3`) rather than a pack header
  (`00 00 01 BA`), and `vnvideo_test.c` plays the same red test picture both ways (`red.mpg` and
  `red-elementary.mpg`).
- Ogg Theora video, silent or with Opus sound, through libogg, libtheora, and libopus.

Ogg Theora with Vorbis sound plays silent, since no library game needs its sound: the one library movie with a Vorbis track, Magical Girl Riru's, is silent. `Engines/Shared/UPSTREAM.md` lists the libraries' versions and what was kept of each.

## Using it

An engine opens a movie with `vnvideo_open`, passing a read callback, so the movie streams from wherever the engine finds its files: an archive, a folder, any case. The callback runs on the movie's decode thread and must be safe there. `vnvideo_get_info` gives the picture's size and frame rate and the sound's rate and channels.

Each frame the engine shows, it asks `vnvideo_frame` for the frame due at its clock, as RGBA pixels. The frame stays the same until a newer one is due. `vnvideo_read_audio` hands out the sound as interleaved floats and is safe to call from an audio thread. A movie with sound should use `vnvideo_audio_time`, the seconds of sound handed out, as its clock, so a slow decode holds the picture back with the sound rather than letting them drift apart. `vnvideo_ended` says when the last frame and the last sound are out. A looping movie starts over at its end and never ends.

The decode thread runs a few frames ahead and waits when its frame or sound queue is full, so a paused movie costs nothing. `vnvideo_close` stops it.

The Swift package links vnvideo through the `CVNVideo` target. `scripts/build.sh test` builds `vnvideo_test.c` and runs it on the small movies in `Engines/Shared/testdata`, which were generated with FFmpeg. `vnvideo_test -play <movie>` decodes any movie as fast as it can and prints its format and decoding speed, which also works on Linux.

## Engines that use it

- Artemis: fullscreen movies and movies on layers. See [artemis.md](artemis.md#movies).
- RPG Maker VX Ace: `Graphics.play_movie`. See [rgss.md](rgss.md#movies).
- NekoSDK and SiglusEngine use vnffmovie instead, below.

## vnffmovie: WMV through the app's FFmpeg

NekoSDK games' movies are WMV in ASF, which neither vnvideo nor Apple's frameworks decode. `Engines/Shared/vnffmovie.[h,cpp]` decodes them with FFmpeg, the LGPL copy the app already ships for KiriKiri, whose build includes the ASF demuxer and the WMV and WMA decoders (`Engines/KiriKiri/UPSTREAM.md`). FFmpeg is `FFmpeg.framework`, which a changed FFmpeg can replace ([relinking.md](relinking.md)). vnffmovie is compiled into the KiriKiri framework, which links FFmpeg.framework, and exported from it, so the app has FFmpeg once and the NekoSDK runtime never links it at all. The KiriKiri engine doesn't call it, and its own movie player is unchanged.

Its interface is five C functions, with no part of KiriKiri in them: `vnffmovie_open` takes a read callback, as `vnvideo_open` does, and gives the picture's size and frame rate, the duration, and the sound's rate and channels; `vnffmovie_frame` gives the frame due at the host's clock as RGBA, whether it changed, and whether the movie has ended; `vnffmovie_audio` hands out the sound as interleaved floats, safe on an audio thread; `vnffmovie_seek` decodes again from a time; `vnffmovie_close` stops the decode thread. A looping movie starts over at its end with its clock running on.

The picture is the clock. The decode thread runs four frames ahead and never waits on sound, so a movie whose sound nobody takes, such as one behind another movie with sound, still moves; past two seconds its oldest sound is dropped. Sound goes where its timestamps put it, with silence over a gap of up to four seconds and overlaps cut. A longer jump in its timestamps, as a damaged file has, counts as a break: the sound carries on where the queued sound ends until the next seek or loop, rather than queuing the whole gap as silence and holding the picture (#125). WMA, which stamps only a packet's first frame, is timed from that stamp onward. Some ASF files store their video well ahead of their sound: BUNNY BLACK 3's movies by 1.4 s, planetarian HD's by a few frames. Four decoded frames would hold that sound back until it was too late to play, so while the queued sound doesn't reach half a second past the picture, the thread reads on and keeps the video packets undecoded (up to 600) until there is room for their frames. A movie has ended once its last frame and the end of its sound are due, by the host's clock rather than by what was taken. A read error, or a loop that can't seek back or whose pass decoded nothing, ends it rather than spinning. `vnffmovie_test -mute` plays a movie taking no sound.

In a NekoSDK game, the runtime stops reading a movie's sound while the story's clock stops, under a menu, the log, a dialog or a pause, and when the story ends. It picks the sound up again on the next step.

The NekoSDK runtime finds the five functions with `dlsym` (`vnneko_find_movies`), as the app finds the KiriKiri engine's own, and EngineNekoSDK hands them to the runtime at start. When the framework or a function is missing, the app logs it, each movie is logged and left out, and the game goes on. The Linux test host builds the same file against the system's FFmpeg, and `vnffmovie_test <movie>` plays a movie against the wall clock, taking its sound as an audio device would, and prints what came out.

SiglusEngine finds the same five functions with `dlsym` in VNDegen's `vnsiglus` crate and hands them to siglus_rs, which plays `.wmv` movies through them in place of its own WMV and WMA decoder, which is LGPL and left out. siglus_rs asks for the frame due at the movie's clock and pulls the sound into kira through a streaming decoder over `vnffmovie_audio`. See [siglus.md](siglus.md#movies).

Known limit: after a seek, ASF sound stored ahead of the keyframe the seek lands on is lost, so a little sound after the seek point is missing.
