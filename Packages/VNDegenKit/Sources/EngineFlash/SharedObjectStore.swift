import Foundation
import WebRuntime

/// Ruffle's localStorage, with each Flash shared object as the `.sol` file Flash Player writes, so saves move to and
/// from the PC unchanged.
///
/// Ruffle names a shared object `<host>/<local path>/<name>` and stores its `.sol` bytes in Base64. The local path is
/// the movie's path in the game folder, or the shorter one the movie asked for, and is empty for `/`. It comes from the
/// movie's URL, so it is percent-encoded, as `My%20Game/game.swf`, and `flash-page.js` gives every key the encoding
/// `FlashPlugin.urlComponent` gives. A name with slashes in it starts with `#`, which a URL path never holds unencoded.
/// The file is `<local path>/<name>.sol` here, decoded, as Flash Player keeps it under `#SharedObjects/<id>/localhost/`,
/// except that a folder of the local path that starts with `#` starts with `%23` on disk, so it never reads back as a
/// name, and one that already starts like that is escaped once more. Any other key goes to `others`.
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

    /// `game/My%20Game/movie.swf/save` is `My Game/movie.swf/save`, `game//save`, from a local path of `/`, is `save`,
    /// and `game/movie.swf/#saves/slot1` is `movie.swf/#saves/slot1`. Nil for keys of another host, and for any part
    /// that is empty, `.`, `..`, hidden, or holds a `/` once decoded, which Ruffle never makes.
    static func file(forKey key: String) -> String? {
        guard key.hasPrefix(host + "/") else { return nil }
        var parts = key.dropFirst(host.count + 1).split(separator: "/", omittingEmptySubsequences: false).map(String.init)
        // An empty local path, from `/`, leaves an empty first part.
        if parts.first == "", parts.count >= 2 { parts.removeFirst() }
        let split = nameStart(parts)
        let path = parts[..<split].map { $0.removingPercentEncoding ?? $0 }
        guard !parts.isEmpty, path.allSatisfy(isSafe), parts[split...].allSatisfy(isSafe) else { return nil }
        return (path.map(diskName) + parts[split...]).joined(separator: "/")
    }

    /// The inverse of `file(forKey:)`, for a file path relative to the directory without `.sol`.
    static func key(forFile relative: String) -> String? {
        let parts = relative.split(separator: "/", omittingEmptySubsequences: false).map(String.init)
        guard !parts.isEmpty, parts.allSatisfy(isSafe) else { return nil }
        let split = nameStart(parts)
        let path = parts[..<split].map { FlashPlugin.urlComponent(pathName($0)) }
        let key = (path + parts[split...]).joined(separator: "/")
        return path.isEmpty ? "\(host)//\(key)" : "\(host)/\(key)"
    }

    /// Where the shared object's name starts: at the first part that starts with `#`, or else the last part. In a key the
    /// local path encodes #, and on disk it starts with `%23`, so this finds the same place in both.
    static func nameStart(_ parts: [String]) -> Int {
        parts.firstIndex { $0.hasPrefix("#") } ?? max(parts.count - 1, 0)
    }

    /// A folder of the local path as a file name. `#bonus` is `%23bonus`, and a name that starts like an escaped one,
    /// `%`, any number of `25`, then `23`, gets one more `25`, so `%23bonus` is `%2523bonus`. Every other name is as Flash
    /// Player writes it.
    static func diskName(_ folder: String) -> String {
        if folder.hasPrefix("#") { return "%23" + folder.dropFirst() }
        return looksEscaped(folder) ? "%25" + folder.dropFirst() : folder
    }

    /// The inverse of `diskName`.
    static func pathName(_ disk: String) -> String {
        guard looksEscaped(disk) else { return disk }
        let rest = disk.dropFirst()
        return rest.hasPrefix("23") ? "#" + rest.dropFirst(2) : "%" + rest.dropFirst(2)
    }

    /// `%` followed by any number of `25`, then `23`.
    private static func looksEscaped(_ name: String) -> Bool {
        guard name.hasPrefix("%") else { return false }
        var rest = name.dropFirst()
        while rest.hasPrefix("25") { rest = rest.dropFirst(2) }
        return rest.hasPrefix("23")
    }

    private static func isSafe(_ name: String) -> Bool {
        !name.isEmpty && !name.hasPrefix(".") && !name.contains("/") && !name.contains("\0")
    }
}
