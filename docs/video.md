# Movies

RPG Maker MV and MZ transparent VP9 WebM movies use a separate native converter, `vnalpha.[h,mm]`. It decodes color and alpha with libvpx and writes HEVC with alpha through AVAssetWriter, with Opus audio converted to AAC. Other audio codecs and non-I420 video are currently unsupported. Games keep their original files and media controls.

Conversion runs serially off the main thread. The original request stays pending until the completed movie is renamed into the cache. The video then reloads with an explicit QuickTime source type so WebKit selects the HEVC reader for the original WebM URL. DOM videos stay hidden until their first decoded frame. PIXI uploads the corrected video through one reusable canvas because WebKit drops alpha on direct video uploads. Quitting cancels conversion and removes its partial file. The app logs each conversion's clip length, elapsed time and process peak resident memory, which includes earlier app activity.

The output pixel pool holds at most eight buffers, about 63 MiB at 1080p. Conversion waits when those buffers are in use. Decoder reference frames and the encoder's internal allocations are additional memory; the pool limit is not a limit on the whole conversion's memory.

Cold conversion can add substantial resident memory to the native app. A single 1080p check added about 164 MiB on a Mac and 389 MiB on an iPad simulator, beyond each paused game's baseline. Those figures exclude the game's separate WebContent heap. Physical iPad memory and conversion speed remain unmeasured; the simulator figures are not device estimates. Cached playback avoids this conversion cost.

The cache lives under the app's Caches directory, is excluded from backups, and evicts the least recently used movies before exceeding 512 MiB. Settings has a Clear video cache button. The old WebCodecs alpha pre-decode path is removed. The short opaque-loop smoothing path retains its separate frame budget.

vnvideo is the movie decoder VNDegen's native engines share, in `Engines/Shared/vnvideo.[ch]`. It plays:

- MPEG-1 video with MPEG-1 Layer II sound, through pl_mpeg.
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
