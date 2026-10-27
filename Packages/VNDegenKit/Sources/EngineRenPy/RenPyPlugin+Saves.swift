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
            let relative = path.hasPrefix(local) ? String(path.dropFirst(local.count))
                : Self.vndegenSaveRelativePath(path) ?? Self.userSaveRelativePath(path) ?? Self.gameSaveRelativePath(path)
            return relative.flatMap(Self.insideSaveFolder)
        }]
    }

    nonisolated static func savesDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("saves", isDirectory: true)
    }

    /// Nil for a path that names no file or could leave the save folder, such as `../library.json`. A backup's paths
    /// come from another machine, and restore writes each file at the path this returns.
    nonisolated static func insideSaveFolder(_ relative: String) -> String? {
        let parts = relative.split(separator: "/", omittingEmptySubsequences: false)
        return parts.contains { ["", ".", ".."].contains($0) } ? nil : relative
    }

    /// `/VNDegen/<game>/saves/1-1-LT1.save` maps to `1-1-LT1.save`. It is the path a first backup records, the same
    /// on every device. `.../GameData/<game>/saves/1-1-LT1.save`, the data directory of any VNDegen install, maps the
    /// same way, which covers backups made before VNDegen recorded the portable path.
    nonisolated static func vndegenSaveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/")
        let root = parts.first == "VNDegen" ? 0 : parts.firstIndex(of: "GameData")
        guard let root, parts.count - root >= 4, parts[root + 2] == "saves" else { return nil }
        return parts[(root + 3)...].joined(separator: "/")
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
    /// first launch copies them into the empty save folder, where the game finds them. The copy goes to a staging
    /// folder first and replaces the empty save folder only once every file arrived, so a failed copy leaves the save
    /// folder empty and the next launch tries again. A folder that can't be listed fails the launch the same way. Taken
    /// for an empty one, it would start the game without the PC saves, and the game's new saves would block a retry.
    nonisolated static func importGameSaves(base: URL, dataDirectory: URL) throws {
        let fileManager = FileManager.default
        let saves = savesDirectory(dataDirectory)
        let gameSaves = FileCase.resolve("game/saves", in: base)
        guard try items(in: saves).isEmpty else { return }
        let pcSaves = try items(in: gameSaves)
        guard !pcSaves.isEmpty else { return }
        let staging = dataDirectory.appendingPathComponent("saves-import", isDirectory: true)
        try? fileManager.removeItem(at: staging)
        defer { try? fileManager.removeItem(at: staging) }
        do {
            try fileManager.createDirectory(at: staging, withIntermediateDirectories: true)
            for item in pcSaves {
                try fileManager.copyItem(at: item, to: staging.appendingPathComponent(item.lastPathComponent))
            }
            guard fileSizes(in: staging) == fileSizes(in: gameSaves) else { throw CocoaError(.fileWriteUnknown) }
            try? fileManager.removeItem(at: saves)
            try fileManager.moveItem(at: staging, to: saves)
        } catch {
            throw RenPyError.savesNotImported(error.localizedDescription)
        }
    }

    /// The folder's visible items. None when it does not exist.
    nonisolated private static func items(in folder: URL) throws -> [URL] {
        do {
            return try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil, options: .skipsHiddenFiles)
        } catch CocoaError.fileReadNoSuchFile {
            return []
        } catch {
            throw RenPyError.savesNotImported(error.localizedDescription)
        }
    }

    /// Every regular file under `folder` by relative path, with its size. Hidden top-level items are left out, as the
    /// import leaves them out.
    nonisolated private static func fileSizes(in folder: URL) -> [String: Int] {
        var sizes: [String: Int] = [:]
        guard let walker = FileManager.default.enumerator(atPath: folder.path) else { return sizes }
        while let path = walker.nextObject() as? String {
            guard !path.hasPrefix("."), walker.fileAttributes?[.type] as? FileAttributeType == .typeRegular else { continue }
            sizes[path] = (walker.fileAttributes?[.size] as? NSNumber)?.intValue
        }
        return sizes
    }
}
