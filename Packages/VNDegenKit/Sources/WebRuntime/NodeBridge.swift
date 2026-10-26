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
    /// Every localStorage key and value, for a page whose storage lives in files.
    case storageLoad
    case storageSet(key: String, value: String)
    case storageRemove(key: String)
    case storageClear

    private enum Key: String, CodingKey { case op, path, text, data, recursive, from, to, url, key, value }

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
        case "storageLoad": self = .storageLoad
        case "storageSet": self = .storageSet(key: try c.decode(String.self, forKey: .key), value: try c.decode(String.self, forKey: .value))
        case "storageRemove": self = .storageRemove(key: try c.decode(String.self, forKey: .key))
        case "storageClear": self = .storageClear
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
    case dictionary([String: String])

    func encode(to encoder: any Encoder) throws {
        var c = encoder.singleValueContainer()
        switch self {
        case .null: try c.encodeNil()
        case .bool(let value): try c.encode(value)
        case .string(let value): try c.encode(value)
        case .strings(let value): try c.encode(value)
        case .stat(let value): try c.encode(value)
        case .dictionary(let value): try c.encode(value)
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
///
/// Every operation works on descriptors that `GameFolder.open` checked after opening: reads go through the opened file,
/// and changes go through `*at` calls on the opened parent directory with a single name that is never followed as a
/// symlink. A game that swaps a directory for an escaping symlink mid-operation therefore cannot reach outside.
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
        switch request {
        case .exists(let path):
            guard let url = folder.resolve(virtualPath: path) else { return .bool(false) }
            return .bool((try? folder.open(url)) != nil)

        case .stat(let path):
            let info = try Self.info(try open(path))
            let mtime = Double(info.st_mtimespec.tv_sec) * 1000 + Double(info.st_mtimespec.tv_nsec) / 1_000_000
            return .stat(FileStat(isFile: Self.kind(info) == S_IFREG, isDirectory: Self.kind(info) == S_IFDIR,
                                  size: Int64(info.st_size), mtimeMs: mtime))

        case let .readFile(path, text):
            let file = try open(path)
            guard Self.kind(try Self.info(file)) != S_IFDIR else { throw BridgeError(code: "EISDIR", message: "illegal operation on a directory") }
            let data = try file.readToEnd() ?? Data()
            return .string(text ? String(decoding: data, as: UTF8.self) : data.base64EncodedString())

        case let .writeFile(path, payload, text):
            let (directory, name) = try openParent(path)
            guard let data = text ? Data(payload.utf8) : Data(base64Encoded: payload) else {
                throw BridgeError(code: "EINVAL", message: "data is not base64")
            }
            try Self.replace(name, in: directory) { try $0.write(contentsOf: data) }
            return .null

        case let .mkdir(path, recursive):
            let url = try confined(path)
            if (try? folder.open(url)) != nil {
                if recursive { return .null }
                throw BridgeError(code: "EEXIST", message: "file already exists")
            }
            if recursive {
                try makeDirectories(url, path: path)
            } else {
                let (directory, name) = try openParent(path)
                guard mkdirat(directory.fileDescriptor, name, 0o755) == 0 else { throw Self.posixError(path) }
            }
            return .null

        case .readdir(let path):
            let directory = try open(path)
            guard Self.kind(try Self.info(directory)) == S_IFDIR else { throw BridgeError(code: "ENOTDIR", message: "not a directory") }
            return .strings(try Self.names(in: directory).sorted())

        case let .rename(from, to):
            let (sourceDirectory, sourceName) = try openParent(from)
            let (targetDirectory, targetName) = try openParent(to)
            // rename(2) replaces the target atomically, which FileManager.moveItem refuses to do.
            guard renameat(sourceDirectory.fileDescriptor, sourceName, targetDirectory.fileDescriptor, targetName) == 0 else {
                throw Self.posixError(from)
            }
            return .null

        case .unlink(let path):
            let (directory, name) = try openParent(path)
            var info = stat()
            guard fstatat(directory.fileDescriptor, name, &info, AT_SYMLINK_NOFOLLOW) == 0 else { throw Self.posixError(path) }
            guard Self.kind(info) != S_IFDIR else { throw BridgeError(code: "EPERM", message: "operation not permitted on a directory") }
            guard unlinkat(directory.fileDescriptor, name, 0) == 0 else { throw Self.posixError(path) }
            return .null

        case let .copyFile(from, to):
            let source = try open(from)
            let sourceInfo = try Self.info(source)
            let isDirectory = BridgeError(code: "EISDIR", message: "illegal operation on a directory")
            guard Self.kind(sourceInfo) != S_IFDIR else { throw isDirectory }
            if let target = try? folder.open(try confined(to)) {
                let targetInfo = try Self.info(target)
                guard Self.kind(targetInfo) != S_IFDIR else { throw isDirectory }
                // Node treats a copy onto the same file as done. Replacing it would delete the source.
                if targetInfo.st_dev == sourceInfo.st_dev && targetInfo.st_ino == sourceInfo.st_ino { return .null }
            }
            let (directory, name) = try openParent(to)
            try Self.replace(name, in: directory) { target in
                while let chunk = try source.read(upToCount: FileSpan.chunkSize), !chunk.isEmpty {
                    try target.write(contentsOf: chunk)
                }
            }
            return .null

        case .openExternal, .quit, .storageLoad, .storageSet, .storageRemove, .storageClear:
            throw BridgeError(code: "ENOSYS", message: "handled by the session, not the bridge")
        }
    }

    private func confined(_ path: String) throws -> URL {
        guard let url = folder.resolve(virtualPath: path) else { throw Self.outside(path) }
        return url
    }

    private func open(_ path: String) throws -> FileHandle {
        do {
            return try folder.open(try confined(path))
        } catch let failure as OpenFailure {
            throw Self.error(failure, path: path)
        }
    }

    private func openParent(_ path: String) throws -> (FileHandle, String) {
        try openParent(of: try confined(path), path: path)
    }

    private func openParent(of url: URL, path: String) throws -> (FileHandle, String) {
        do {
            return try folder.openParent(of: url)
        } catch let failure as OpenFailure {
            throw Self.error(failure, path: path)
        }
    }

    /// Creates each missing directory on the way to `url`, one verified parent at a time. `url` comes from `resolve`, so
    /// it lies under the game root.
    private func makeDirectories(_ url: URL, path: String) throws {
        var missing: [URL] = []
        var current = url
        while (try? folder.open(current)) == nil, current.path.count > folder.root.path.count {
            missing.append(current)
            current = current.deletingLastPathComponent()
        }
        for directory in missing.reversed() {
            let (parent, name) = try openParent(of: directory, path: path)
            guard mkdirat(parent.fileDescriptor, name, 0o755) == 0 || errno == EEXIST else { throw Self.posixError(path) }
        }
    }

    /// Writes `name` in `directory` through a staging file beside it, then renames the staging file over it, so a failed
    /// write leaves the old file in place. A symlink at `name` is replaced, not followed.
    private static func replace(_ name: String, in directory: FileHandle, write: (FileHandle) throws -> Void) throws {
        let staging = ".\(name).\(UUID().uuidString)"
        let fd = openat(directory.fileDescriptor, staging, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0o644)
        guard fd >= 0 else { throw posixError(name) }
        do {
            try write(FileHandle(fileDescriptor: fd, closeOnDealloc: true))
            guard renameat(directory.fileDescriptor, staging, directory.fileDescriptor, name) == 0 else { throw posixError(name) }
        } catch {
            unlinkat(directory.fileDescriptor, staging, 0)
            throw error
        }
    }

    private static func names(in directory: FileHandle) throws -> [String] {
        // fdopendir takes ownership of the descriptor it is given, so it gets a duplicate.
        guard let stream = fdopendir(dup(directory.fileDescriptor)) else { throw posixError("") }
        defer { closedir(stream) }
        var names: [String] = []
        while let entry = readdir(stream) {
            let name = withUnsafeBytes(of: entry.pointee.d_name) { String(decoding: $0.prefix(Int(entry.pointee.d_namlen)), as: UTF8.self) }
            if name != "." && name != ".." { names.append(name) }
        }
        return names
    }

    private static func info(_ file: FileHandle) throws -> stat {
        var info = stat()
        guard fstat(file.fileDescriptor, &info) == 0 else { throw posixError("") }
        return info
    }

    private static func kind(_ info: stat) -> mode_t { info.st_mode & S_IFMT }

    private static func error(_ failure: OpenFailure, path: String) -> BridgeError {
        switch failure {
        case .outside: outside(path)
        case .posix(let code): posixError(path, code: code)
        }
    }

    private static func outside(_ path: String) -> BridgeError {
        BridgeError(code: "EACCES", message: "path is outside the game folder: \(path)")
    }

    private static func missing(_ path: String) -> BridgeError {
        BridgeError(code: "ENOENT", message: "no such file or directory, '\(path)'")
    }

    private static func posixError(_ path: String, code: Int32 = errno) -> BridgeError {
        let name = switch code {
        case ENOENT: "ENOENT"
        case EACCES, EPERM: "EACCES"
        case EEXIST: "EEXIST"
        case ENOTDIR: "ENOTDIR"
        case EISDIR: "EISDIR"
        case ENOTEMPTY: "ENOTEMPTY"
        default: "EIO"
        }
        if name == "ENOENT" { return missing(path) }
        return BridgeError(code: name, message: String(cString: strerror(code)))
    }
}
