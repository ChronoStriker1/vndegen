# MIDI music

Many PC games, RPG Maker and Wolf RPG Editor games above all, play their music from MIDI files and leave the sound to
Windows' General MIDI synthesizer. VNDegen's native engines play them with vnmidi, a small C player in
`Engines/Shared`, through a General MIDI SoundFont the app bundles.

## The player

`vnmidi.c` plays Standard MIDI Files, formats 0 and 1, with
[TinySoundFont](https://github.com/schellingb/TinySoundFont) (`tsf.h`, MIT) and TinyMidiLoader (`tml.h`, zlib) from the
same project. It reads .sf2 SoundFonts and .sf3 ones, whose samples are Ogg Vorbis, which it decodes with
[stb_vorbis](https://github.com/nothings/stb) (MIT or public domain) as the font loads. The three libraries are in
`Engines/Shared/third_party`, at the versions their `LICENSE.txt` files name. `vnmidi.c` compiles them into itself
through `third_party/vnmidi_libs.h`, with internal linkage or a `vnmidi_` prefix, so an engine that
compiles its own copies of them links beside it without duplicate symbols.

`tsf.h` has VNDegen changes, marked "VNDegen change", that refuse a font whose preset, instrument, generator or sample
indices point past their tables and an .sf3 sample whose byte range lies outside the sample chunk, which upstream
follows without checking. `tml.h` has one change of VNDegen's, marked "VNDegen change" in the file. Upstream TinyMidiLoader moves the first event
it puts in order into its first slot while it still reads the tracks. When that slot held a track's first event, the
track stops at once, and its later events are lost. In the Wolf RPG test games that lost every tempo change of 4 of
93 songs, which then played up to 36 s too short or 40 s too long.

## The SoundFont

The app bundles `Engines/Shared/soundfonts/FluidR3Mono_GM.sf3`, unmodified, and copies the folder to its resources as
`soundfonts`.

- What it is: FluidR3Mono GM 2.312, Michael Cowgill's mono version of Frank Wen's Fluid R3 General MIDI SoundFont, in
  MuseScore's SF3 packaging.
- Source: `share/sound/FluidR3Mono_GM.sf3` in MuseScore's `v2.3.2` tag, commit
  `45924076871483cb96b859cc374c5fbdbf76ba53`, from
  `https://raw.githubusercontent.com/musescore/MuseScore/v2.3.2/share/sound/FluidR3Mono_GM.sf3`. MuseScore 2.1 and 2.2
  ship the same file. Later MuseScore versions replaced it with MS General and then MS Basic.
- Size: 14,563,174 bytes. SHA-256 `cfcd66d89e8386823400eca64934b14fbea7bf48ba1f00d21189af1262794ec2`.
- License: MIT, in `FluidR3Mono_License.md` beside it, from the same folder of the same tag (SHA-256
  `09926f9451f751408abd65162c99343f840234d2a708584808fd46427633063f`). It allows use, copying, changing, and selling
  with the copyright and permission notice included. `scripts/midi-licenses.sh` puts it on the app's Licenses page.

The decoded font takes memory: TinySoundFont keeps every sample as a 32-bit float, so FluidR3Mono's 62 million
samples take 267 MB once loaded. Loading takes 0.66 s of CPU on the Linux build machine and 1.1 s on the Mac mini.
An engine should load the font when a game first plays MIDI, off its audio thread, and keep it for the session.

FluidR3Mono stays the bundled default (the user's decision, 2026-10-30). If its memory, load time or sound causes
problems, a smaller font is the next step. VNDegen doesn't store samples in 16 bits to save memory.

## The player's SoundFont

Settings › MIDI music lets the player choose an .sf2 or .sf3 file to use instead of the bundled font. VNDegen copies
it into `SoundFont/` in its support folder (`Application Support/VNDegen`, or the DEBUG `-libraryRoot`'s `Support`), so
it doesn't need access to the original later, and Settings shows
its name with a button that goes back to the bundled font. The choice is app-wide and applies to games started after
it.

It covers the engines that play MIDI through vnmidi: Wolf RPG Editor. The RPG Maker XP, VX and VX Ace engine
(mkxp-z) plays MIDI with its own synthesizer and embedded font and doesn't use it.

An engine loads the player's font with `vnmidi_font_load_fallback`, which loads the bundled font instead when the
player's can't be read and says why, which the engine logs.

## The C interface

`Engines/Shared/vnmidi.h`. Engines compile `vnmidi.c`, or depend on the `CVNMidi` package target, which links to it.

```c
vnmidi_font *vnmidi_font_load(const char *path, char *error, size_t error_size);
vnmidi_font *vnmidi_font_load_fallback(const char *path, const char *fallback, char *error, size_t error_size);
void vnmidi_font_free(vnmidi_font *font);

vnmidi_player *vnmidi_player_open(vnmidi_font *font, const void *data, size_t size, int sample_rate,
                                  char *error, size_t error_size);
void vnmidi_player_close(vnmidi_player *player);
const char *vnmidi_player_error(const vnmidi_player *player);
int vnmidi_player_render(vnmidi_player *player, float *out, int frames);

int vnmidi_player_length_ms(const vnmidi_player *player);
int vnmidi_player_position_ms(const vnmidi_player *player);
int vnmidi_player_playing(const vnmidi_player *player);
int vnmidi_player_marker_ms(const vnmidi_player *player);

void vnmidi_player_set_loop(vnmidi_player *player, int loop, int loop_start_ms);  // VNMIDI_LOOP_MARKER: CC#111
void vnmidi_player_set_volume(vnmidi_player *player, float gain);
void vnmidi_player_set_tempo(vnmidi_player *player, float factor);
void vnmidi_player_set_transpose(vnmidi_player *player, int semitones);
void vnmidi_player_seek(vnmidi_player *player, int ms);
void vnmidi_player_stop(vnmidi_player *player);
```

- One font serves any number of players on any threads. A player keeps the font's samples alive, so the font may be
  freed before its players close.
- `vnmidi_player_render` writes interleaved stereo float and may run on a real-time audio thread: it doesn't allocate,
  lock, or touch files. The other player calls may come from any thread while it renders, and take effect at the start
  of the next render call. Close a player once its render calls have stopped.
- Without a font, `vnmidi_player_open` with a NULL font still gives a player. It renders silence but keeps time, so a
  game that waits for its music to end still sees it end, and `vnmidi_player_error` returns one line saying why.
- Looping. RPG Maker's MIDI files mark the loop start with a CC#111 event, which `VNMIDI_LOOP_MARKER` follows. Wolf RPG
  Editor passes a loop start in ms instead. At the end the player releases the sounding notes, whose tails carry over
  the seam, and the channels take the programs and controllers the song sets before the loop start. Controllers it
  sets only later keep their values, as sequencers do, since resetting them would change the tails' level at once.
- Tempo changes speed without pitch. Transposition leaves channel 10, the drums, alone. Length and position count the
  file's own time, which tempo doesn't change.
- A song without a loop ends when its last event has passed and its notes have faded below -80 dBFS for 200 ms, or
  10 s after its last event at the latest.
- A file that ends early plays the events it has. A track length past the end of the data is cut to it.

The output is 12 dB below TinySoundFont's full level. At full level a General MIDI song is about four times as loud as
typical game music in Ogg or MP3, and loud passages clip. Engines scale it further with `vnmidi_player_set_volume`.

## The Swift side

`MIDISoundFont`, in the `FontPack` module: `url` is the font an engine loads, the player's when there is one, else the
bundled one, and `fallbackURL` the bundled font when the player's is in use, which an engine passes to
`vnmidi_font_load_fallback`. `install` and `reset` keep the player's copy. `project.yml` copies
`Engines/Shared/soundfonts` into the app.

## Tests

`vnmidi_test.c` checks the fallback load (a file that isn't a SoundFont falls back to the bundled one, and a missing
fallback reports both reasons) and the player on generated songs: length, CC#111 marker, the loop seam and where it lands, stop,
seek, volume, tempo, transposition, a player without a font, every truncation of the file, a font shared by players
on four threads, and a tempo track whose first event is a meta event. `scripts/build.sh test` runs it with the bundled
font. Given MIDI files and a folder, it renders each to a WAV file and the 3 s on each side of its loop seam to
another, and prints length, level, CPU time, and the step at the seam.

Checked on Linux with the 93 MIDI files of two Wolf RPG test games, all of which load and play:

- Lengths match an independent parser of the files to within 52 ms, median 2 ms. Notes fade out within 8.2 s of a
  song's end, 2.0 s in the median.
- Peak levels range from 0.067 to 0.839 of full scale, with no clipping, and RMS levels from -39.9 to -16.6 dBFS.
- Rendering takes 0.0064 s of CPU per second of audio on average, and 0.0174 s for the busiest song, on one core of the
  Linux build machine.
- At each loop seam the step between neighbouring samples stays below the song's own 99.9th percentile step, so the
  seams don't click.
- AddressSanitizer and UndefinedBehaviorSanitizer report nothing for the generated tests and for 70 truncated and
  corrupted copies of the test games' files.
