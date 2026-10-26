import Foundation
import VNCore

extension RenPyPlugin {
    /// VNDegen's launcher keeps a game's saves directly in `<data directory>/saves`. On a PC, Ren'Py saves to
    /// `<user folder>/<config.save_directory>/` and also to `game/saves/`. Both map into the one folder, so restoring
    /// never needs the game's `config.save_directory`.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        let saves = Self.savesDirectory(dataDirectory)
        let local = saves.standardizedFileURL.path + "/"
        return [SaveLocation(directory: saves) { path in
            if path.hasPrefix(local) { return String(path.dropFirst(local.count)) }
            return Self.userSaveRelativePath(path) ?? Self.gameSaveRelativePath(path)
        }]
    }

    nonisolated static func savesDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("saves", isDirectory: true)
    }

    /// `C:/Users/x/AppData/Roaming/RenPy/Game-1505403882/1-1-LT1.save` (Windows), `~/Library/RenPy/...` (macOS) and
    /// `/home/deck/.renpy/Game-1505403882/1-1-LT1.save` (Linux) all map to `1-1-LT1.save`. Ren'Py's `tokens` and
    /// `backups` folders sit beside the save folders and hold no saves.
    nonisolated static func userSaveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/")
        guard let root = parts.lastIndex(where: { ["renpy", ".renpy"].contains($0.lowercased()) }),
              parts.count - root >= 3, !["tokens", "backups"].contains(parts[root + 1].lowercased()) else { return nil }
        return parts[(root + 2)...].joined(separator: "/")
    }

    /// `<install>/game/saves/1-1-LT1.save` maps to `1-1-LT1.save`.
    nonisolated static func gameSaveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/")
        guard let saves = parts.lastIndex(where: { $0.lowercased() == "saves" }), saves > 0,
              parts[saves - 1].lowercased() == "game", saves < parts.count - 1 else { return nil }
        return parts[(saves + 1)...].joined(separator: "/")
    }

    /// A game copied from a PC can carry its saves in `game/saves/`. VNDegen never writes to the game folder, so the
    /// first launch copies them into the empty save folder, where the game finds them.
    nonisolated static func importGameSaves(base: URL, dataDirectory: URL) {
        let fileManager = FileManager.default
        let saves = savesDirectory(dataDirectory)
        let gameSaves = base.appendingPathComponent("game/saves", isDirectory: true)
        guard (try? fileManager.contentsOfDirectory(atPath: saves.path))?.isEmpty ?? true,
              let items = try? fileManager.contentsOfDirectory(at: gameSaves, includingPropertiesForKeys: nil, options: .skipsHiddenFiles),
              !items.isEmpty else { return }
        try? fileManager.createDirectory(at: saves, withIntermediateDirectories: true)
        for item in items {
            try? fileManager.copyItem(at: item, to: saves.appendingPathComponent(item.lastPathComponent))
        }
    }
}
