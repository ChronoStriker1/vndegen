import Foundation

/// The game folder as the page and the Node shim see it.
///
/// The shim works in a virtual POSIX tree rooted at `virtualRoot`. The game never learns a real path (RPG Maker MZ's
/// main.js refuses to start when its path begins with `/private/var`, which is where iOS keeps Documents), and every
/// path the game hands back is confined to this folder before it touches disk.
nonisolated public struct GameFolder: Sendable {
    public static let virtualRoot = "/game"
    public static let scheme = "vndegen"
    public static let origin = "\(scheme)://game"

    /// The game folder on disk, as the user copied it in.
    public let root: URL
    /// Where index.html lives, relative to `root`, such as "", "www", or "resources/app.asar".
    public let webRoot: String
    /// The web root's archive when it is an Electron app.asar. Pages are served from inside it, read only.
    private(set) var archive: AsarArchive?
    /// Folders the app ships, by the served path the page sees them at, such as `/__vndegen/` for a player. Requests
    /// under that path read from the folder, never from the game. The longest matching path wins.
    public var mounts: [String: URL] = [:]
    /// Files the page sees as part of a file in the game folder, by served path, such as the movie appended to a Flash
    /// projector. The path is the containing file's, with its case on disk.
    public var embedded: [String: FileSlice] = [:]
    /// `root` with every symlink resolved. Every path this folder hands out must lead somewhere under it.
    private let realRoot: String
    /// `root` as the kernel names it for an open descriptor. `open` compares descriptors against this, not `realRoot`,
    /// so both sides of the comparison come from `F_GETPATH`.
    private let descriptorRoot: String

    public init(root: URL, webRoot: String) {
        self.root = root
        self.webRoot = webRoot
        realRoot = Self.realPath(root.path) ?? root.path
        let fd = Darwin.open(root.path, O_RDONLY | O_DIRECTORY | O_CLOEXEC)
        descriptorRoot = (fd >= 0 ? Self.descriptorPath(fd) : nil) ?? realRoot
        if fd >= 0 { close(fd) }
        if webRoot.lowercased().hasSuffix(".asar") {
            archive = try? archiveHandle().flatMap { try AsarArchive(reading: $0) }
        }
    }

    /// The web root as a virtual path.
    public var virtualWebRoot: String { webRoot.isEmpty ? Self.virtualRoot : "\(Self.virtualRoot)/\(webRoot)" }
    /// What NW.js reports as `process.mainModule.filename`.
    public var mainModule: String { "\(virtualWebRoot)/index.html" }
    public var indexURL: URL { URL(string: "\(Self.origin)/index.html")! }

    /// Maps a virtual absolute path to disk. Nil when the path is relative, climbs out of the game folder, or passes
    /// through a symlink that leads out of it.
    ///
    /// A name missing on disk matches an entry that differs from it only in case. Windows games often get the case of
    /// their own files wrong, which Windows and the Mac's disk forgive and an iPhone's disk does not.
    public func resolve(virtualPath: String) -> URL? {
        guard let relative = Self.confine(virtualPath) else { return nil }
        var path = root.path
        var deepestExisting = Self.exists(path) ? path : nil
        for name in relative.split(separator: "/").map(String.init) {
            if deepestExisting == path, let actual = Self.entry(name, in: path) {
                path += "/" + actual
                deepestExisting = path
            } else {
                path += "/" + name
            }
        }
        // Names below the deepest existing one do not exist yet, so they cannot be symlinks.
        if let deepestExisting {
            guard let real = Self.realPath(deepestExisting), real == realRoot || real.hasPrefix(realRoot + "/") else { return nil }
        }
        return URL(fileURLWithPath: path)
    }

    /// Maps the path of a `vndegen://game/...` request to disk. Nil when it escapes the web root.
    public func resolve(servedPath: String) -> URL? {
        let path = servedPath.isEmpty || servedPath == "/" ? "/index.html" : servedPath
        return resolve(virtualPath: virtualWebRoot + (path.hasPrefix("/") ? path : "/" + path))
    }

    /// Normalizes `.` and `..` lexically and returns the path relative to the virtual root. Nil when the path is not
    /// absolute or ends up outside the root. `resolve` then checks where symlinks lead.
    static func confine(_ virtualPath: String) -> String? {
        guard virtualPath.hasPrefix("/") else { return nil }
        var parts: [Substring] = []
        for part in virtualPath.split(separator: "/") {
            switch part {
            case ".":
                continue
            case "..":
                guard !parts.isEmpty else { return nil }
                parts.removeLast()
            default:
                parts.append(part)
            }
        }
        guard parts.first == Substring(virtualRoot.dropFirst()) else { return nil }
        return parts.dropFirst().joined(separator: "/")
    }

    /// The name of `name` as it is on disk in `directory`: itself when it exists, otherwise the first entry that equals
    /// it ignoring case. Nil when neither exists.
    private static func entry(_ name: String, in directory: String) -> String? {
        if exists(directory + "/" + name) { return name }
        return (try? FileManager.default.contentsOfDirectory(atPath: directory))?
            .sorted()
            .first { $0.caseInsensitiveCompare(name) == .orderedSame }
    }

    /// True when anything is at `path`, including a symlink whose target is missing.
    private static func exists(_ path: String) -> Bool {
        var info = stat()
        return lstat(path, &info) == 0
    }

    /// Where an open descriptor's file is now, which a rename or symlink swap after the open cannot change.
    static func descriptorPath(_ fd: Int32) -> String? {
        var buffer = [CChar](repeating: 0, count: Int(MAXPATHLEN))
        guard fcntl(fd, F_GETPATH, &buffer) != -1 else { return nil }
        return String(cString: buffer)
    }

    private static func realPath(_ path: String) -> String? {
        guard let resolved = realpath(path, nil) else { return nil }
        defer { free(resolved) }
        return String(cString: resolved)
    }

    static func mimeType(forExtension ext: String) -> String {
        switch ext.lowercased() {
        case "html", "htm": "text/html"
        case "js", "mjs": "text/javascript"
        case "css": "text/css"
        case "json": "application/json"
        case "wasm": "application/wasm"
        case "png": "image/png"
        case "jpg", "jpeg": "image/jpeg"
        case "gif": "image/gif"
        case "webp": "image/webp"
        case "svg": "image/svg+xml"
        case "ico": "image/x-icon"
        case "ogg", "oga": "audio/ogg"
        case "mp3": "audio/mpeg"
        case "m4a": "audio/mp4"
        case "wav": "audio/wav"
        case "webm": "video/webm"
        case "mp4": "video/mp4"
        case "ttf": "font/ttf"
        case "otf": "font/otf"
        case "woff": "font/woff"
        case "woff2": "font/woff2"
        case "txt": "text/plain"
        case "csv": "text/csv"
        case "xml": "application/xml"
        default: "application/octet-stream"
        }
    }
}

/// Why `GameFolder.open` refused a path.
nonisolated enum OpenFailure: Error, Equatable {
    /// What opened lies outside the game folder.
    case outside
    case posix(Int32)
}

nonisolated extension GameFolder {
    /// Opens what `url` names, then confirms the opened file itself lies inside the game folder. The check runs on the
    /// descriptor, not the path, so a symlink swapped in after `resolve` validated the path cannot lead the read or write
    /// out of the folder. Callers read and write through the returned handle only.
    func open(_ url: URL, flags: Int32 = O_RDONLY) throws(OpenFailure) -> FileHandle {
        // O_NONBLOCK keeps a FIFO in the folder from blocking the open. Regular files and directories ignore it.
        let fd = Darwin.open(url.path, flags | O_CLOEXEC | O_NONBLOCK)
        guard fd >= 0 else { throw .posix(errno) }
        let handle = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        guard let path = Self.descriptorPath(fd), path == descriptorRoot || path.hasPrefix(descriptorRoot + "/") else { throw .outside }
        return handle
    }

    /// Opens the directory that holds `url`, verified as `open` does, for `*at` calls on the returned name. Those calls
    /// look up only that one name, without following a symlink, so nothing between the check and the write can move.
    func openParent(of url: URL) throws(OpenFailure) -> (directory: FileHandle, name: String) {
        (try open(url.deletingLastPathComponent(), flags: O_RDONLY | O_DIRECTORY), url.lastPathComponent)
    }
}

/// Part of a file: `length` bytes from `offset`.
nonisolated public struct FileSlice: Equatable, Sendable {
    public var offset: UInt64
    public var length: Int64

    public init(offset: UInt64, length: Int64) {
        self.offset = offset
        self.length = length
    }
}

nonisolated struct FileResponse: Equatable, Sendable {
    var status: Int
    var headers: [String: String]
    /// The part of the file the body carries. Nil for an empty body.
    var body: FileSpan?

    static func empty(_ status: Int, headers: [String: String] = [:]) -> FileResponse {
        FileResponse(status: status, headers: headers.merging(["Content-Length": "0"]) { a, _ in a }, body: nil)
    }
}

/// Part of a file that `GameFolder.serve` already opened and checked.
nonisolated struct FileSpan: Equatable, Sendable {
    var handle: FileHandle
    var offset: UInt64
    var length: UInt64

    static let chunkSize = 256 << 10

    static func == (a: FileSpan, b: FileSpan) -> Bool {
        a.handle === b.handle && a.offset == b.offset && a.length == b.length
    }

    /// Reads the span one chunk at a time and hands each chunk to `deliver`, which returns false to stop early, as when
    /// WebKit stops the request. The read never holds more than one chunk of the file.
    func stream(chunkSize: Int = Self.chunkSize, to deliver: (Data) async -> Bool) async throws {
        try handle.seek(toOffset: offset)
        var remaining = length
        while remaining > 0 {
            guard let chunk = try handle.read(upToCount: Int(min(UInt64(chunkSize), remaining))), !chunk.isEmpty else {
                throw CocoaError(.fileReadCorruptFile)
            }
            remaining -= UInt64(chunk.count)
            guard await deliver(chunk) else { return }
        }
    }
}

nonisolated enum ByteRange: Equatable, Sendable {
    case full
    case partial(start: Int64, end: Int64)
    case unsatisfiable
}

nonisolated extension GameFolder {
    /// Answers one request the way a static file server would: 404 for misses and escapes, 206 for a byte range,
    /// 416 for a range past the end. WebKit's media loader relies on the range support and `Accept-Ranges`.
    func serve(path: String, method: String = "GET", range: String? = nil) -> FileResponse {
        guard let (handle, base, size, name) = open(servedPath: path) else { return .empty(404) }
        return Self.response(handle, base: base, size: size, contentType: Self.mimeType(forExtension: (name as NSString).pathExtension),
                             method: method, range: range)
    }

    /// The response for `size` bytes of `handle` starting at `base`.
    static func response(_ handle: FileHandle, base: UInt64, size: Int64, contentType: String, method: String, range: String?) -> FileResponse {
        var headers = ["Content-Type": contentType, "Accept-Ranges": "bytes"]
        let status: Int, first: Int64, last: Int64
        switch byteRange(header: range, size: size) {
        case .full:
            (status, first, last) = (200, 0, size - 1)
        case let .partial(start, end):
            (status, first, last) = (206, start, end)
            headers["Content-Range"] = "bytes \(start)-\(end)/\(size)"
        case .unsatisfiable:
            return .empty(416, headers: ["Content-Range": "bytes */\(size)"])
        }
        let length = max(0, last - first + 1)
        headers["Content-Length"] = String(length)
        let body = method == "HEAD" || length == 0 ? nil : FileSpan(handle: handle, offset: base + UInt64(first), length: UInt64(length))
        return FileResponse(status: status, headers: headers, body: body)
    }

    /// Up to `limit` bytes of the file a page would get for `path`, from disk or from the web root's app.asar.
    public func read(servedPath path: String, limit: Int) -> Data? {
        guard let (handle, base, size, _) = open(servedPath: path) else { return nil }
        try? handle.seek(toOffset: base)
        return try? handle.read(upToCount: Int(min(Int64(limit), size)))
    }

    /// Opens the regular file a request names: the handle to read, where its bytes start in it, its size, and its name.
    /// A web root that is an app.asar serves its members from the archive file, or from app.asar.unpacked/ beside it.
    func open(servedPath path: String) -> (FileHandle, UInt64, Int64, String)? {
        let path = path.isEmpty || path == "/" ? "/index.html" : path
        if let (prefix, folder) = mounts.filter({ path.hasPrefix($0.key) }).max(by: { $0.key.count < $1.key.count }) {
            return GameFolder(root: folder, webRoot: "").open(servedPath: "/" + path.dropFirst(prefix.count))
        }
        if let slice = embedded[path] {
            guard let url = resolve(servedPath: path), let (handle, size) = openRegularFile(url),
                  slice.offset <= UInt64(size), slice.length <= size - Int64(slice.offset) else { return nil }
            return (handle, slice.offset, slice.length, url.lastPathComponent)
        }
        if let archive, let relative = Self.confine(virtualWebRoot + (path.hasPrefix("/") ? path : "/" + path)),
           relative.lowercased().hasPrefix(webRoot.lowercased() + "/") {
            let inner = String(relative.dropFirst(webRoot.count + 1))
            switch archive.member(inner) {
            case let .packed(offset, size):
                guard let handle = try? archiveHandle() else { return nil }
                return (handle, offset, size, inner)
            case .unpacked(let unpacked):
                return openRegularFile(virtualPath: "\(virtualWebRoot).unpacked/\(unpacked)").map { ($0, 0, $1, inner) }
            case nil:
                return nil
            }
        }
        guard let url = resolve(servedPath: path) else { return nil }
        return openRegularFile(url).map { ($0, 0, $1, url.lastPathComponent) }
    }

    private func openRegularFile(virtualPath: String) -> (FileHandle, Int64)? {
        resolve(virtualPath: virtualPath).flatMap(openRegularFile)
    }

    private func openRegularFile(_ url: URL) -> (FileHandle, Int64)? {
        var info = stat()
        guard let handle = try? open(url), fstat(handle.fileDescriptor, &info) == 0, info.st_mode & S_IFMT == S_IFREG else { return nil }
        return (handle, Int64(info.st_size))
    }

    /// The web root's archive file, opened and checked like any other file.
    private func archiveHandle() throws -> FileHandle? {
        guard let url = resolve(virtualPath: virtualWebRoot) else { return nil }
        return try open(url)
    }

    /// Parses `bytes=a-b`, `bytes=a-`, and `bytes=-n`. A malformed header means the whole file, as RFC 9110 says.
    static func byteRange(header: String?, size: Int64) -> ByteRange {
        guard let header, size > 0, header.hasPrefix("bytes=") else { return .full }
        let spec = header.dropFirst("bytes=".count)
        guard !spec.contains(","), let dash = spec.firstIndex(of: "-") else { return .full }
        let first = spec[..<dash].trimmingCharacters(in: .whitespaces)
        let last = spec[spec.index(after: dash)...].trimmingCharacters(in: .whitespaces)
        if first.isEmpty {
            guard let suffix = Int64(last), suffix > 0 else { return .full }
            return .partial(start: max(0, size - suffix), end: size - 1)
        }
        guard let start = Int64(first) else { return .full }
        guard start < size else { return .unsatisfiable }
        let end = last.isEmpty ? size - 1 : min(Int64(last) ?? size - 1, size - 1)
        guard end >= start else { return .full }
        return .partial(start: start, end: end)
    }

    public static func isDirectory(_ url: URL) -> Bool {
        var isDirectory: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory) && isDirectory.boolValue
    }

    public static func isFile(_ url: URL) -> Bool {
        var isDirectory: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory) && !isDirectory.boolValue
    }
}
