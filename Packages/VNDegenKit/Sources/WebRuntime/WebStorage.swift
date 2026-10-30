import Foundation

/// Where a page's localStorage lives. The shim calls it before each change reaches the page, so every write is on disk
/// when setItem returns. Keys and values are the strings the page used.
nonisolated public protocol PageStorage: Sendable {
    func load() throws -> [String: String]
    func set(_ key: String, _ value: String) throws
    func remove(_ key: String) throws
    func clear() throws
}

nonisolated extension PageStorage {
    func handle(_ request: BridgeRequest) -> BridgeReply {
        do {
            switch request {
            case .storageLoad: return .value(.dictionary(try load()))
            case let .storageSet(key, value): try set(key, value)
            case .storageRemove(let key): try remove(key)
            case .storageClear: try clear()
            default: return .failure(BridgeError(code: "ENOSYS", message: "not a storage request"))
            }
            return .value(.null)
        } catch {
            return .failure(BridgeError(code: "EIO", message: error.localizedDescription))
        }
    }
}

/// A page's localStorage as files in an app-owned directory, one file per key. Keys are percent-encoded into file names,
/// so no key can name a path outside the directory. Upper-case letters are encoded too, so keys that differ only in case
/// get different files on a case-insensitive volume such as the Mac's.
nonisolated public struct WebStorage: PageStorage {
    public let directory: URL

    public init(directory: URL) {
        self.directory = directory
    }

    /// Every key and value. Files named the way earlier versions named them, with upper-case letters left as they were,
    /// move to their current names first. When a restore brings back an old name beside a current one, the newer file wins.
    public func load() throws -> [String: String] {
        let names: [String]
        do {
            names = try FileManager.default.contentsOfDirectory(atPath: directory.path)
        } catch CocoaError.fileReadNoSuchFile {
            return [:]
        }
        var items: [String: String] = [:]
        for name in names where name.hasSuffix(Self.suffix) {
            guard let key = String(name.dropLast(Self.suffix.count)).removingPercentEncoding else { continue }
            let url = directory.appendingPathComponent(name), current = file(for: key)
            if url.lastPathComponent != current.lastPathComponent {
                if let currentDate = Self.modified(current) {
                    guard let legacyDate = Self.modified(url), legacyDate > currentDate else {
                        try FileManager.default.removeItem(at: url)
                        continue
                    }
                    try FileManager.default.removeItem(at: current)
                }
                try FileManager.default.moveItem(at: url, to: current)
            }
            items[key] = try String(contentsOf: current, encoding: .utf8)
        }
        return items
    }

    public func value(for key: String) throws -> String? {
        do {
            return try String(contentsOf: file(for: key), encoding: .utf8)
        } catch CocoaError.fileReadNoSuchFile {
            return nil
        }
    }

    public func set(_ key: String, _ value: String) throws {
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        try Data(value.utf8).write(to: file(for: key), options: .atomic)
    }

    public func remove(_ key: String) throws {
        do {
            try FileManager.default.removeItem(at: file(for: key))
        } catch CocoaError.fileNoSuchFile {}
    }

    public func clear() throws {
        for key in try load().keys { try remove(key) }
    }

    private static let suffix = ".value"
    private static let allowed = CharacterSet(charactersIn: "abcdefghijklmnopqrstuvwxyz0123456789-_")

    private static func modified(_ url: URL) -> Date? {
        try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate
    }

    /// The name of the file that holds `key`.
    public static func fileName(for key: String) -> String {
        (key.addingPercentEncoding(withAllowedCharacters: allowed) ?? "") + suffix
    }

    private func file(for key: String) -> URL {
        directory.appendingPathComponent(Self.fileName(for: key))
    }
}
