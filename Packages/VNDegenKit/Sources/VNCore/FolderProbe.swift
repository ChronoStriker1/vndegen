import Foundation

/// A read-only listing of a game folder that detection runs against.
public struct FolderProbe: Sendable {
    public let root: URL
    /// Relative paths with forward slashes, in their on-disk case. Directories have no trailing slash.
    public let entries: Set<String>
    private let folded: Set<String>

    public init(root: URL, entries: Set<String>) {
        self.root = root
        self.entries = entries
        self.folded = Set(entries.map { $0.lowercased() })
    }

    /// Walks `root` to `maxDepth` levels. Deep enough for `www/js/rpg_core.js` and `game/script.rpy`.
    public init(scanning root: URL, maxDepth: Int = 3) {
        var found = Set<String>()
        func walk(_ dir: URL, prefix: String, depth: Int) {
            guard depth < maxDepth,
                  let names = try? FileManager.default.contentsOfDirectory(atPath: dir.path) else { return }
            for name in names where !name.hasPrefix(".") {
                let rel = prefix.isEmpty ? name : "\(prefix)/\(name)"
                found.insert(rel)
                var isDir: ObjCBool = false
                let url = dir.appendingPathComponent(name)
                if FileManager.default.fileExists(atPath: url.path, isDirectory: &isDir), isDir.boolValue {
                    walk(url, prefix: rel, depth: depth + 1)
                }
            }
        }
        walk(root, prefix: "", depth: 0)
        self.init(root: root, entries: found)
    }

    /// Case-insensitive, because Windows games are inconsistent about case.
    public func contains(_ path: String) -> Bool { folded.contains(path.lowercased()) }

    /// Paths whose extension matches, case-insensitively, such as `entries(withExtension: "rpy")`.
    public func entries(withExtension ext: String) -> [String] {
        let suffix = "." + ext.lowercased()
        return entries.filter { $0.lowercased().hasSuffix(suffix) }.sorted()
    }

    /// Reads up to `limit` bytes of a file in the folder.
    public func read(_ path: String, limit: Int = 1 << 20) -> Data? {
        let actual = entries.first { $0.lowercased() == path.lowercased() } ?? path
        guard let handle = try? FileHandle(forReadingFrom: root.appendingPathComponent(actual)) else { return nil }
        defer { try? handle.close() }
        return try? handle.read(upToCount: limit)
    }
}
