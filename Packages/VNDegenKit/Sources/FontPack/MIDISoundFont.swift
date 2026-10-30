import Foundation

/// The General MIDI SoundFont for engines that play MIDI music with vnmidi (`Engines/Shared/vnmidi.h`, the `CVNMidi`
/// module): one the player chose in Settings, or the one the app bundles. The bundled font lives in
/// `Engines/Shared/soundfonts`, which the app copies to its resources as it does the fonts. See docs/midi.md.
nonisolated public enum MIDISoundFont {
    /// The font engines load: the player's when there is one, else the bundled one.
    public static var url: URL? { userFont() ?? bundledURL }

    /// The bundled font when the player's is in use, which an engine loads instead if the player's can't be read.
    public static var fallbackURL: URL? { userFont() == nil ? nil : bundledURL }

    /// The bundled font. Nil when the app's resources don't have it, as in package tests.
    public static var bundledURL: URL? {
        Bundle.main.url(forResource: "FluidR3Mono_GM", withExtension: "sf3", subdirectory: "soundfonts")
    }

    /// The file types a player can choose.
    public static let fileExtensions = ["sf2", "sf3"]

    /// Where the player's font is kept: a copy in VNDegen's Application Support folder, so it doesn't depend on access
    /// to the file it came from. The folder holds at most that one file.
    public static var userDirectory: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("VNDegen/SoundFont", isDirectory: true)
    }

    /// The player's font in `directory`, if one is there.
    public static func userFont(in directory: URL = userDirectory) -> URL? {
        let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
        return files.filter { fileExtensions.contains($0.pathExtension.lowercased()) }
            .min { $0.lastPathComponent < $1.lastPathComponent }
    }

    /// Copies `source` into `directory` as the player's font, replacing any earlier one, and returns the copy.
    @discardableResult
    public static func install(_ source: URL, in directory: URL = userDirectory) throws -> URL {
        guard fileExtensions.contains(source.pathExtension.lowercased()) else {
            throw CocoaError(.fileReadCorruptFile, userInfo: [NSFilePathErrorKey: source.path])
        }
        let fm = FileManager.default
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        // Copy beside the folder first, so a failed copy leaves the current font in place.
        let staging = directory.deletingLastPathComponent()
            .appendingPathComponent(".SoundFont-\(UUID().uuidString)", isDirectory: true)
        try fm.createDirectory(at: staging, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: staging) }
        let copy = staging.appendingPathComponent(source.lastPathComponent)
        try fm.copyItem(at: source, to: copy)
        try? fm.removeItem(at: directory)
        try fm.moveItem(at: staging, to: directory)
        return directory.appendingPathComponent(source.lastPathComponent)
    }

    /// Removes the player's font, so engines go back to the bundled one.
    public static func reset(in directory: URL = userDirectory) throws {
        guard FileManager.default.fileExists(atPath: directory.path) else { return }
        try FileManager.default.removeItem(at: directory)
    }
}
