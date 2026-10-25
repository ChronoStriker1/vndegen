import Foundation

/// One synchronous call from the shim's `fs`, `nw`, or `process` to the app, carried as JSON through `window.prompt`.
/// `prompt` is the only channel WebKit offers that blocks the page until the host answers, and Node's sync `fs` calls
/// need exactly that.
nonisolated enum BridgeRequest: Equatable, Sendable, Decodable {
    case exists(path: String)
    case stat(path: String)
    /// `text` true returns UTF-8 text; false returns base64 bytes.
    case readFile(path: String, text: Bool)
    /// `text` true writes `data` as UTF-8, which is what Node does for a string; false decodes base64 bytes.
    case writeFile(path: String, data: String, text: Bool)
    case mkdir(path: String, recursive: Bool)
    case readdir(path: String)
    case rename(from: String, to: String)
    case unlink(path: String)
    case copyFile(from: String, to: String)
    case openExternal(url: String)
    case quit

    private enum Key: String, CodingKey { case op, path, text, data, recursive, from, to, url }

    init(from decoder: any Decoder) throws {
        let c = try decoder.container(keyedBy: Key.self)
        let op = try c.decode(String.self, forKey: .op)
        switch op {
        case "exists": self = .exists(path: try c.decode(String.self, forKey: .path))
        case "stat": self = .stat(path: try c.decode(String.self, forKey: .path))
        case "readFile":
            self = .readFile(path: try c.decode(String.self, forKey: .path),
                             text: try c.decodeIfPresent(Bool.self, forKey: .text) ?? false)
        case "writeFile":
            self = .writeFile(path: try c.decode(String.self, forKey: .path),
                              data: try c.decode(String.self, forKey: .data),
                              text: try c.decodeIfPresent(Bool.self, forKey: .text) ?? false)
        case "mkdir":
            self = .mkdir(path: try c.decode(String.self, forKey: .path),
                          recursive: try c.decodeIfPresent(Bool.self, forKey: .recursive) ?? false)
        case "readdir": self = .readdir(path: try c.decode(String.self, forKey: .path))
        case "rename": self = .rename(from: try c.decode(String.self, forKey: .from), to: try c.decode(String.self, forKey: .to))
        case "unlink": self = .unlink(path: try c.decode(String.self, forKey: .path))
        case "copyFile": self = .copyFile(from: try c.decode(String.self, forKey: .from), to: try c.decode(String.self, forKey: .to))
        case "openExternal": self = .openExternal(url: try c.decode(String.self, forKey: .url))
        case "quit": self = .quit
        default: throw DecodingError.dataCorruptedError(forKey: .op, in: c, debugDescription: "unknown op \(op)")
        }
    }
}

nonisolated struct FileStat: Equatable, Sendable, Encodable {
    var isFile: Bool
    var isDirectory: Bool
    var size: Int64
    /// Milliseconds since the epoch, like Node's `mtimeMs`.
    var mtimeMs: Double
}

nonisolated enum BridgeValue: Equatable, Sendable, Encodable {
    case null
    case bool(Bool)
    case string(String)
    case strings([String])
    case stat(FileStat)

    func encode(to encoder: any Encoder) throws {
        var c = encoder.singleValueContainer()
        switch self {
        case .null: try c.encodeNil()
        case .bool(let value): try c.encode(value)
        case .string(let value): try c.encode(value)
        case .strings(let value): try c.encode(value)
        case .stat(let value): try c.encode(value)
        }
    }
}

/// A Node-style failure. The shim turns `code` into `err.code`, which is what MZ and plugins branch on.
nonisolated struct BridgeError: Error, Equatable, Sendable, Encodable {
    var code: String
    var message: String
}

nonisolated enum BridgeReply: Equatable, Sendable, Encodable {
    case value(BridgeValue)
    case failure(BridgeError)

    private enum Key: String, CodingKey { case value, error }

    func encode(to encoder: any Encoder) throws {
        var c = encoder.container(keyedBy: Key.self)
        switch self {
        case .value(let value): try c.encode(value, forKey: .value)
        case .failure(let error): try c.encode(error, forKey: .error)
        }
    }

    var json: String {
        let encoder = JSONEncoder()
        encoder.outputFormatting = .sortedKeys
        guard let data = try? encoder.encode(self) else { return #"{"error":{"code":"EIO","message":"reply encoding failed"}}"# }
        return String(decoding: data, as: UTF8.self)
    }
}

/// Runs the file operations of `BridgeRequest` against the game folder. It touches nothing else, so tests drive it
/// with a temporary directory.
nonisolated struct NodeBridge: Sendable {
    let folder: GameFolder

    func handle(_ request: BridgeRequest) -> BridgeReply {
        do {
            return .value(try perform(request))
        } catch let error as BridgeError {
            return .failure(error)
        } catch {
            return .failure(BridgeError(code: "EIO", message: error.localizedDescription))
        }
    }

    private func perform(_ request: BridgeRequest) throws -> BridgeValue {
        let fm = FileManager.default
        switch request {
        case .exists(let path):
            guard let url = folder.resolve(virtualPath: path) else { return .bool(false) }
            return .bool(fm.fileExists(atPath: url.path))

        case .stat(let path):
            let url = try confined(path)
            guard fm.fileExists(atPath: url.path) else { throw Self.missing(path) }
            let values = try url.resourceValues(forKeys: [.isRegularFileKey, .isDirectoryKey, .fileSizeKey, .contentModificationDateKey])
            return .stat(FileStat(isFile: values.isRegularFile ?? false,
                                  isDirectory: values.isDirectory ?? false,
                                  size: Int64(values.fileSize ?? 0),
                                  mtimeMs: (values.contentModificationDate ?? .distantPast).timeIntervalSince1970 * 1000))

        case let .readFile(path, text):
            let url = try confined(path)
            guard !GameFolder.isDirectory(url) else { throw BridgeError(code: "EISDIR", message: "illegal operation on a directory") }
            guard let data = fm.contents(atPath: url.path) else { throw Self.missing(path) }
            return .string(text ? String(decoding: data, as: UTF8.self) : data.base64EncodedString())

        case let .writeFile(path, payload, text):
            let url = try confined(path)
            guard GameFolder.isDirectory(url.deletingLastPathComponent()) else { throw Self.missing(path) }
            guard let data = text ? Data(payload.utf8) : Data(base64Encoded: payload) else {
                throw BridgeError(code: "EINVAL", message: "data is not base64")
            }
            try data.write(to: url, options: .atomic)
            return .null

        case let .mkdir(path, recursive):
            let url = try confined(path)
            if fm.fileExists(atPath: url.path) {
                if recursive { return .null }
                throw BridgeError(code: "EEXIST", message: "file already exists")
            }
            guard recursive || GameFolder.isDirectory(url.deletingLastPathComponent()) else { throw Self.missing(path) }
            try fm.createDirectory(at: url, withIntermediateDirectories: recursive)
            return .null

        case .readdir(let path):
            let url = try confined(path)
            guard fm.fileExists(atPath: url.path) else { throw Self.missing(path) }
            guard GameFolder.isDirectory(url) else { throw BridgeError(code: "ENOTDIR", message: "not a directory") }
            return .strings(try fm.contentsOfDirectory(atPath: url.path).sorted())

        case let .rename(from, to):
            let source = try confined(from), target = try confined(to)
            guard fm.fileExists(atPath: source.path) else { throw Self.missing(from) }
            // rename(2) replaces the target atomically, which FileManager.moveItem refuses to do.
            guard rename(source.path, target.path) == 0 else { throw Self.posixError() }
            return .null

        case .unlink(let path):
            let url = try confined(path)
            guard fm.fileExists(atPath: url.path) else { throw Self.missing(path) }
            guard !GameFolder.isDirectory(url) else { throw BridgeError(code: "EPERM", message: "operation not permitted on a directory") }
            try fm.removeItem(at: url)
            return .null

        case let .copyFile(from, to):
            let source = try confined(from), target = try confined(to)
            guard fm.fileExists(atPath: source.path) else { throw Self.missing(from) }
            guard !GameFolder.isDirectory(source), !GameFolder.isDirectory(target) else {
                throw BridgeError(code: "EISDIR", message: "illegal operation on a directory")
            }
            // Node treats a copy onto the same file as done. Replacing it would delete the source.
            if Self.isSameFile(source, target) { return .null }
            guard GameFolder.isDirectory(target.deletingLastPathComponent()) else { throw Self.missing(to) }
            // Copies beside the target and renames over it, so a failed copy leaves the old target in place.
            let staging = target.deletingLastPathComponent().appendingPathComponent(".\(target.lastPathComponent).\(UUID().uuidString)")
            do {
                try fm.copyItem(at: source.resolvingSymlinksInPath(), to: staging)
                guard rename(staging.path, target.path) == 0 else { throw Self.posixError() }
            } catch {
                try? fm.removeItem(at: staging)
                throw error
            }
            return .null

        case .openExternal, .quit:
            throw BridgeError(code: "ENOSYS", message: "handled by the session, not the bridge")
        }
    }

    private func confined(_ path: String) throws -> URL {
        guard let url = folder.resolve(virtualPath: path) else {
            throw BridgeError(code: "EACCES", message: "path is outside the game folder: \(path)")
        }
        return url
    }

    private static func isSameFile(_ a: URL, _ b: URL) -> Bool {
        var first = stat(), second = stat()
        guard stat(a.path, &first) == 0, stat(b.path, &second) == 0 else { return false }
        return first.st_dev == second.st_dev && first.st_ino == second.st_ino
    }

    private static func missing(_ path: String) -> BridgeError {
        BridgeError(code: "ENOENT", message: "no such file or directory, '\(path)'")
    }

    private static func posixError() -> BridgeError {
        let code = errno
        let name = switch code {
        case ENOENT: "ENOENT"
        case EACCES, EPERM: "EACCES"
        case EEXIST: "EEXIST"
        case ENOTDIR: "ENOTDIR"
        case EISDIR: "EISDIR"
        case ENOTEMPTY: "ENOTEMPTY"
        default: "EIO"
        }
        return BridgeError(code: name, message: String(cString: strerror(code)))
    }
}
