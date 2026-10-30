import Foundation
import VNCore

extension WolfRPGPlugin {
    /// Wolf RPG Editor saves in the Save folder beside Game.exe on every platform, and the runtime does the same, so a PC
    /// backup restores into the folder the game reads.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = WolfRPGLayout(FolderProbe(scanning: folder)) else { return [] }
        let root = layout.url(in: folder)
        // The folder as it is on disk, matched without regard to case as the runtime matches it.
        let directory = FileCase.resolve("Save", in: root)
        return [SaveLocation(directory: directory) { path in Self.saveRelativePath(path) }]
    }

    /// `C:/Games/Game/Save/SaveData01.sav` maps to `SaveData01.sav`: every file directly in the Save folder.
    nonisolated static func saveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/").map(String.init)
        guard parts.count >= 2, parts[parts.count - 2].lowercased() == "save", let name = parts.last,
              ![".", ".."].contains(name) else { return nil }
        return name
    }
}
