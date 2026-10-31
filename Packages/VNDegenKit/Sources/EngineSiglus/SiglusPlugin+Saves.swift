import Foundation
import VNCore

extension SiglusPlugin {
    /// SiglusEngine keeps its saves in `savedata/` in the game's folder, as on a PC, in the PC engine's own format, so
    /// Ludusavi moves them between VNDegen and a PC.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = SiglusLayout(scanning: folder), layout.engine == .siglus else { return [] }
        // The folder as it is on disk, matched without regard to case as the engine matches it.
        let directory = FileCase.resolve("savedata", in: layout.url(in: folder))
        return [SaveLocation(directory: directory) { path in Self.saveRelativePath(path, folder: "savedata") }]
    }

    /// The part of a path after its last `folder` component, matched ignoring case as Windows does. Nil for a file
    /// outside such a folder.
    nonisolated static func saveRelativePath(_ path: String, folder: String) -> String? {
        let parts = path.replacingOccurrences(of: "\\", with: "/").split(separator: "/").map(String.init)
        guard let index = parts.lastIndex(where: { $0.lowercased() == folder }), index + 1 < parts.count else { return nil }
        let rest = parts[(index + 1)...]
        guard !rest.contains(where: { $0 == "." || $0 == ".." }) else { return nil }
        return rest.joined(separator: "/")
    }
}

extension RealLivePlugin {
    /// siglus_rs writes RealLive saves in a format of its own, which VNDegen keeps in `savedata_rs/` in the game's data
    /// folder. The PC version's `SAVEDATA/` files are a different format and stay where they are.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        let directory = Self.saveDirectory(dataDirectory)
        return [SaveLocation(directory: directory) { path in SiglusPlugin.saveRelativePath(path, folder: "savedata_rs") }]
    }

    nonisolated static func saveDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appending(path: "savedata_rs", directoryHint: .isDirectory)
    }

    nonisolated public func backupWarning(for game: Game, dataDirectory: URL) -> String? {
        "RealLive saves made in VNDegen are in a format of VNDegen's engine. They restore to VNDegen on another device, "
            + "but the PC version can't load them, and VNDegen can't load the PC version's saves."
    }
}
