#!/usr/bin/env bash
# Writes build/midi/licenses/, the notices of vnmidi, the MIDI player in Engines/Shared that native engines compile:
# the General MIDI SoundFont the app bundles from Engines/Shared/soundfonts, and the libraries vnmidi.c compiles in.
#   scripts/midi-licenses.sh
set -euo pipefail
cd "$(dirname "$0")/.."
shared=Engines/Shared
out=build/midi/licenses
rm -rf "$out.part" && mkdir -p "$out.part"
cp "$shared/soundfonts/FluidR3Mono_License.md" "$out.part/FluidR3Mono General MIDI SoundFont 2.312 (MIDI music).txt"
cp "$shared/third_party/tsf/LICENSE.txt" "$out.part/TinySoundFont and TinyMidiLoader (MIDI music).txt"
cp "$shared/third_party/stb_vorbis/LICENSE.txt" "$out.part/stb_vorbis (MIDI SoundFont samples).txt"
rm -rf "$out" && mv "$out.part" "$out"
