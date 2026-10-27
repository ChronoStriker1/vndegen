import Foundation
import VNCore

extension GodotPlugin {
    /// Godot keeps a game's saves in `user://`, which on Windows is `%APPDATA%\Godot\app_userdata\<project name>`, or
    /// `%APPDATA%\<folder>` for a project that names its own folder. VNDegen keeps it the same way in the game's data
    /// directory, so Ludusavi backups from Windows, a Steam Deck's Proton, or Godot's Linux and Mac builds restore there.
    /// Games that save beside their `.exe` instead keep those saves in their own folder, as on PC.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        guard let layout = GodotLayout(FolderProbe(scanning: folder)) else { return [] }
        let userDirectory = layout.header.userDirectory
        let directory = Self.appDataDirectory(dataDirectory).appendingPathComponent(userDirectory, isDirectory: true)
        let local = directory.standardizedFileURL.path + "/"
        return [SaveLocation(directory: directory) { path in
            if path.hasPrefix(local) { return Self.insideFolder(String(path.dropFirst(local.count))) }
            return Self.userDataRelativePath(path, userDirectory: userDirectory)
        }]
    }

    /// The rest of a path in a copy of `user://`, compared without case: after `AppData/Roaming/<user directory>/` on
    /// Windows, in Proton, and in VNDegen's own data directories, after `.local/share/<user directory>/` in Godot's Linux
    /// builds, whose `Godot` folder is `godot`, and after `Library/Application Support/<user directory>/` on a Mac.
    nonisolated static func userDataRelativePath(_ path: String, userDirectory: String) -> String? {
        let parts = path.split(separator: "/", omittingEmptySubsequences: false)
        let lower = parts.map { $0.lowercased() }
        let user = userDirectory.lowercased().split(separator: "/").map(String.init)
        for prefix in [["appdata", "roaming"], [".local", "share"], ["library", "application support"]] {
            let wanted = prefix + user
            guard wanted.count < lower.count else { continue }
            for start in 0...(lower.count - wanted.count - 1) where Array(lower[start..<(start + wanted.count)]) == wanted {
                return insideFolder(parts[(start + wanted.count)...].joined(separator: "/"))
            }
        }
        return nil
    }

    /// Nil for a path that names no file or could leave its folder, such as `../save.json`. A backup's paths come from
    /// another machine, and restore writes each file at the path this returns.
    nonisolated static func insideFolder(_ relative: String) -> String? {
        let parts = relative.split(separator: "/", omittingEmptySubsequences: false)
        return parts.isEmpty || parts.contains { ["", ".", ".."].contains($0) } ? nil : relative
    }
}
