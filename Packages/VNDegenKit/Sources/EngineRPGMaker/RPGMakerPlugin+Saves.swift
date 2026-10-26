import Foundation
import VNCore
import WebRuntime

extension RPGMakerPlugin {
    /// NW.js builds save with `fs` to `<web root>/save/`, the folder holding index.html. Resolved the way the game's own
    /// `fs` calls are, so a backup or restore touches the same folder the game does.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let directory = GameFolder(rpgMakerGameAt: folder).saveDirectory else { return [] }
        return [SaveLocation(directory: directory, relativePath: Self.saveRelativePath)]
    }

    /// `/home/deck/.../common/NoSocksRPG/save/file0.rmmzsave` maps to `file0.rmmzsave`. Saves sit directly in a
    /// `save/` folder that has an install directory above it.
    nonisolated static func saveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/")
        guard parts.count >= 3, parts[parts.count - 2].lowercased() == "save", let name = parts.last else { return nil }
        let lower = name.lowercased()
        guard lower.hasSuffix(".rmmzsave") || lower.hasSuffix(".rpgsave") else { return nil }
        return String(name)
    }
}
