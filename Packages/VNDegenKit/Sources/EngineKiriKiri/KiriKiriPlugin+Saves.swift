import Foundation
import VNCore

extension KiriKiriPlugin {
    /// KiriKiri saves to `savedata/` in the game's root, the folder holding its `.exe`, on every platform. Some games
    /// save to `System.appDataPath` or `System.personalPath` instead, the AppData and Documents folders on Windows,
    /// which VNDegen keeps in the game's data directory.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = KiriKiriLayout(FolderProbe(scanning: folder)) else { return [] }
        let savedata = folder.appendingPathComponent(layout.path("savedata"), isDirectory: true)
        return [
            // A game that keeps a `savedata` folder under AppData saves there, not in its root.
            SaveLocation(directory: savedata) { Self.appDataRelativePath($0) == nil ? Self.saveRelativePath($0) : nil },
            Self.userFolderLocation(Self.appDataDirectory(dataDirectory), rule: Self.appDataRelativePath),
            Self.userFolderLocation(Self.documentsDirectory(dataDirectory), rule: Self.documentsRelativePath),
        ]
    }

    nonisolated static func appDataDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("AppData", isDirectory: true)
    }

    nonisolated static func documentsDirectory(_ dataDirectory: URL) -> URL {
        dataDirectory.appendingPathComponent("Documents", isDirectory: true)
    }

    /// `/home/deck/Games/vn/<game>/savedata/datasc.ksd` and `C:/Games/<game>/savedata/datasc.ksd` map to
    /// `datasc.ksd`, and subfolders of `savedata/` are kept. Nil for a path that names no file or could leave the save
    /// folder, such as `savedata/../startup.tjs`, because restore writes each file at the path this returns.
    nonisolated static func saveRelativePath(_ path: String) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        guard let saves = parts.lastIndex(where: { $0.lowercased() == "savedata" }) else { return nil }
        return insideFolder(parts[(saves + 1)...].joined(separator: "/"))
    }

    /// `C:/Users/<user>/AppData/Roaming/<rest>` on Windows and `.../drive_c/users/steamuser/AppData/Roaming/<rest>` in
    /// Proton map to `<rest>`.
    nonisolated static func appDataRelativePath(_ path: String) -> String? {
        userFolderRelativePath(path, folders: [["appdata", "roaming"]])
    }

    /// `C:/Users/<user>/Documents/<rest>` on Windows and `.../drive_c/users/steamuser/My Documents/<rest>` in Proton map
    /// to `<rest>`.
    nonisolated static func documentsRelativePath(_ path: String) -> String? {
        userFolderRelativePath(path, folders: [["documents"], ["my documents"]])
    }

    /// A location in the game's data directory that stands in for a Windows user folder. Besides `rule`, it recognizes
    /// its own files and `/VNDegen/<game>/<folder>/<rest>`, the path a first backup records, or a data directory's
    /// `GameData/<game>/<folder>/<rest>` from any VNDegen install.
    nonisolated private static func userFolderLocation(_ directory: URL, rule: @escaping @Sendable (String) -> String?) -> SaveLocation {
        let local = directory.standardizedFileURL.path + "/"
        let name = directory.lastPathComponent
        return SaveLocation(directory: directory) { path in
            if path.hasPrefix(local) { return insideFolder(String(path.dropFirst(local.count))) }
            let parts = path.split(separator: "/", omittingEmptySubsequences: false)
            let root = parts.dropFirst().first == "VNDegen" ? 1 : parts.firstIndex(of: "GameData")
            if let root, parts.count > root + 3, parts[root + 2] == name {
                return insideFolder(parts[(root + 3)...].joined(separator: "/"))
            }
            return rule(path)
        }
    }

    /// The rest of `path` after `users/<user>/` and one of `folders`, compared without case.
    nonisolated private static func userFolderRelativePath(_ path: String, folders: [[String]]) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        let lower = parts.map { $0.lowercased() }
        guard let users = lower.firstIndex(of: "users") else { return nil }
        let start = users + 2
        for folder in folders where lower.count > start + folder.count && Array(lower[start..<(start + folder.count)]) == folder {
            return insideFolder(parts[(start + folder.count)...].joined(separator: "/"))
        }
        return nil
    }

    /// Nil for a path that names no file or could leave its folder, such as `../library.json`. A backup's paths come
    /// from another machine, and restore writes each file at the path this returns.
    nonisolated private static func insideFolder(_ relative: String) -> String? {
        let parts = relative.split(separator: "/", omittingEmptySubsequences: false)
        return parts.contains { ["", ".", ".."].contains($0) } ? nil : relative
    }
}
