import Foundation
import VNCore

extension ArtemisPlugin {
    /// The core keeps a game's saves in `saves/` of its data directory, in the folder `system.ini`'s `SAVEPATH` names,
    /// as the PC engine keeps them in that folder under AppData or beside the game. The files are the PC engine's own,
    /// so they move to and from a PC as they are.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let settings = Self.settings(folder: folder) else { return [] }
        let directory = Self.saveDirectory(dataDirectory)
        let spellings = [settings.savePath] + (settings.windowsSavePath.map { [$0] } ?? [])
        return [SaveLocation(directory: directory) { path in
            Self.saveRelativePath(path, directory: directory, spellings: spellings)
        }]
    }

    nonisolated static func saveDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appending(path: "saves", directoryHint: .isDirectory)
    }

    /// The game's settings, read from its archives through the core. Nil without the engine or a readable game.
    nonisolated static func settings(folder: URL) -> ArtemisSettings? {
        guard let core = ArtemisCore.shared, let layout = ArtemisLayout(scanning: folder),
              let files = ArtemisCore.Files(core: core, folder: folder, layout: layout),
              let ini = files.read("system.ini") else { return nil }
        return ArtemisSettings(ini: ini)
    }

    /// A save's path relative to the save directory: from the save folder on, in whichever of its spellings the path
    /// uses, matched ignoring case as Windows does. Nil for a file outside a save folder.
    nonisolated static func saveRelativePath(_ path: String, directory: URL, spellings: [String]) -> String? {
        var path = path.replacingOccurrences(of: "\\", with: "/")
        let local = directory.path.hasSuffix("/") ? directory.path : directory.path + "/"
        if path.hasPrefix(local) { path = "/" + path.dropFirst(local.count) }
        let lowered = path.lowercased()
        for spelling in spellings {
            guard let found = lowered.range(of: "/" + spelling.lowercased() + "/", options: .backwards) else { continue }
            // Past the slash, at the same character in `path`.
            let offset = lowered.distance(from: lowered.startIndex, to: found.lowerBound) + 1
            let relative = String(path.dropFirst(offset))
            if relative.count > spelling.count + 1 { return relative }
        }
        return nil
    }
}
