import Foundation
import VNCore

extension RPGMakerPlugin {
    /// NW.js builds save with `fs` to `<web root>/save/`, the folder holding index.html.
    nonisolated public func saveLocations(folder: URL, dataDirectory: URL) -> [SaveLocation] {
        [SaveLocation(directory: webRoot(folder).appendingPathComponent("save", isDirectory: true),
                      relativePath: Self.saveRelativePath)]
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

    /// Mirrors `detect`: a top-level `js/` wins, otherwise MV uses `www/`.
    nonisolated private func webRoot(_ folder: URL) -> URL {
        let fm = FileManager.default
        guard flavor == .mv, !fm.fileExists(atPath: folder.appendingPathComponent("js/rpg_core.js").path),
              let www = (try? fm.contentsOfDirectory(atPath: folder.path))?.first(where: { $0.lowercased() == "www" })
        else { return folder }
        return folder.appendingPathComponent(www, isDirectory: true)
    }
}
