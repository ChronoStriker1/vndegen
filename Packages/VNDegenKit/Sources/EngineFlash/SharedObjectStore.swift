import Foundation
import WebRuntime

/// Ruffle's localStorage, with each Flash shared object as the `.sol` file Flash Player writes, so saves move to and
/// from the PC unchanged.
///
/// Ruffle names a shared object `<host>/<local path>/<name>` and stores its `.sol` bytes in Base64. The local path is
/// the movie's path in the game folder, or the shorter one the movie asked for, and is empty for `/`. The file is
/// `<local path>/<name>.sol` here, as Flash Player keeps it under `#SharedObjects/<id>/localhost/`. Any other key goes
/// to `others`.
nonisolated struct SharedObjectStore: PageStorage {
    let directory: URL
    let others: WebStorage
    /// The host Ruffle names shared objects after, which is the game folder's.
    static let host = URL(string: GameFolder.origin)!.host()!
    static let suffix = ".sol"

    func load() throws -> [String: String] {
        var items = try others.load()
        guard let walker = FileManager.default.enumerator(at: directory, includingPropertiesForKeys: [.isRegularFileKey]) else { return items }
        let base = directory.standardizedFileURL.path + "/"
        for case let url as URL in walker where url.lastPathComponent.lowercased().hasSuffix(Self.suffix) {
            guard (try? url.resourceValues(forKeys: [.isRegularFileKey]))?.isRegularFile == true else { continue }
            let path = url.standardizedFileURL.path
            guard path.hasPrefix(base) else { continue }
            let relative = String(path.dropFirst(base.count).dropLast(Self.suffix.count))
            guard let key = Self.key(forFile: relative) else { continue }
            items[key] = try Data(contentsOf: url).base64EncodedString()
        }
        return items
    }

    func set(_ key: String, _ value: String) throws {
        guard let file = file(for: key) else { return try others.set(key, value) }
        guard let data = Data(base64Encoded: value) else { throw CocoaError(.fileWriteInapplicableStringEncoding) }
        try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try data.write(to: file, options: .atomic)
    }

    func remove(_ key: String) throws {
        guard let file = file(for: key) else { return try others.remove(key) }
        do {
            try FileManager.default.removeItem(at: file)
        } catch CocoaError.fileNoSuchFile {}
    }

    func clear() throws {
        for key in try load().keys { try remove(key) }
    }

    /// The file for one of Ruffle's shared object keys, or nil for any other key.
    func file(for key: String) -> URL? {
        Self.file(forKey: key).map { directory.appendingPathComponent($0 + Self.suffix) }
    }

    /// `game/sub/movie.swf/save` is `sub/movie.swf/save`, and `game//save`, from a local path of `/`, is `save`. Nil for
    /// keys of another host, and for any part that is empty, `.`, `..`, or hidden, which Ruffle never makes.
    static func file(forKey key: String) -> String? {
        guard key.hasPrefix(host + "/") else { return nil }
        var parts = key.dropFirst(host.count + 1).split(separator: "/", omittingEmptySubsequences: false)
        if parts.first == "", parts.count == 2 { parts.removeFirst() }
        guard !parts.isEmpty, parts.allSatisfy({ !$0.isEmpty && !$0.hasPrefix(".") }) else { return nil }
        return parts.joined(separator: "/")
    }

    /// The inverse of `file(forKey:)`, for a file path relative to the directory without `.sol`.
    static func key(forFile relative: String) -> String? {
        let parts = relative.split(separator: "/", omittingEmptySubsequences: false)
        guard !parts.isEmpty, parts.allSatisfy({ !$0.isEmpty && !$0.hasPrefix(".") }) else { return nil }
        return parts.count == 1 ? "\(host)//\(relative)" : "\(host)/\(relative)"
    }
}
