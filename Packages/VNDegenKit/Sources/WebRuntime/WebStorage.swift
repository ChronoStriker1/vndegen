import Foundation

/// A page's localStorage as files in an app-owned directory, one file per key. Keys are percent-encoded into file names,
/// so no key can name a path outside the directory.
nonisolated struct WebStorage: Sendable {
    let directory: URL

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

    func load() throws -> [String: String] {
        guard let names = try? FileManager.default.contentsOfDirectory(atPath: directory.path) else { return [:] }
        var items: [String: String] = [:]
        for name in names where name.hasSuffix(Self.suffix) {
            guard let key = String(name.dropLast(Self.suffix.count)).removingPercentEncoding else { continue }
            items[key] = try String(contentsOf: directory.appendingPathComponent(name), encoding: .utf8)
        }
        return items
    }

    func set(_ key: String, _ value: String) throws {
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        try Data(value.utf8).write(to: file(for: key), options: .atomic)
    }

    func remove(_ key: String) throws {
        do {
            try FileManager.default.removeItem(at: file(for: key))
        } catch CocoaError.fileNoSuchFile {}
    }

    func clear() throws {
        for key in try load().keys { try remove(key) }
    }

    private static let suffix = ".value"
    private static let allowed = CharacterSet(charactersIn: "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")

    private func file(for key: String) -> URL {
        directory.appendingPathComponent((key.addingPercentEncoding(withAllowedCharacters: Self.allowed) ?? "") + Self.suffix)
    }
}
