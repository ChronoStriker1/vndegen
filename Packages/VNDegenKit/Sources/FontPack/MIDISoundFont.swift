import Foundation

/// The General MIDI SoundFont the app bundles for engines that play MIDI music with vnmidi (`Engines/Shared/vnmidi.h`,
/// the `CVNMidi` module). It lives in `Engines/Shared/soundfonts`, which the app copies to its resources as it does the
/// fonts. See docs/midi.md.
nonisolated public enum MIDISoundFont {
    /// The SoundFont file, which an engine passes to `vnmidi_font_load`. Nil when the app's resources don't have it,
    /// as in package tests.
    public static var url: URL? {
        Bundle.main.url(forResource: "FluidR3Mono_GM", withExtension: "sf3", subdirectory: "soundfonts")
    }
}
