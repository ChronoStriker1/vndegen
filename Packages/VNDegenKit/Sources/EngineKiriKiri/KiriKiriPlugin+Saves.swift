import Foundation
import VNCore

extension KiriKiriPlugin {
    /// KiriKiri saves to `savedata/` in the game's root, the folder holding its `.exe`, on every platform.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = KiriKiriLayout(FolderProbe(scanning: folder)) else { return [] }
        let directory = folder.appendingPathComponent(layout.path("savedata"), isDirectory: true)
        return [SaveLocation(directory: directory, relativePath: Self.saveRelativePath)]
    }

    /// `/home/deck/Games/vn/<game>/savedata/datasc.ksd` and `C:/Games/<game>/savedata/datasc.ksd` map to
    /// `datasc.ksd`, and subfolders of `savedata/` are kept. Nil for a path that names no file or could leave the save
    /// folder, such as `savedata/../startup.tjs`, because restore writes each file at the path this returns.
    nonisolated static func saveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        guard let saves = parts.lastIndex(where: { $0.lowercased() == "savedata" }) else { return nil }
        let rest = parts[(saves + 1)...]
        guard !rest.isEmpty, !rest.contains(where: { ["", ".", ".."].contains($0) }) else { return nil }
        return rest.joined(separator: "/")
    }
}
