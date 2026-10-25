import Foundation

/// The game folder as the page and the NW.js shim see it.
///
/// The shim works in a virtual POSIX tree rooted at `virtualRoot`. The game never learns a real path (MZ's main.js
/// refuses to start when its path begins with `/private/var`, which is where iOS keeps Documents), and every path the
/// game hands back is confined to this folder before it touches disk.
nonisolated struct GameFolder: Sendable {
    static let virtualRoot = "/game"
    static let scheme = "vndegen"
    static let origin = "\(scheme)://game"

    /// The game folder on disk, as the user copied it in.
    let root: URL
    /// Where index.html lives, relative to `root`: empty for MZ and "www" for deployed MV games.
    let webRoot: String

    init(root: URL, webRoot: String) {
        self.root = root
        self.webRoot = webRoot
    }

    /// Picks the web root the way `RPGMakerPlugin.detect` does.
    init(root: URL) {
        let top = root.appendingPathComponent("index.html").path
        self.init(root: root, webRoot: FileManager.default.fileExists(atPath: top) ? "" : "www")
    }

    private var virtualWebRoot: String { webRoot.isEmpty ? Self.virtualRoot : "\(Self.virtualRoot)/\(webRoot)" }
    /// What NW.js reports as `process.mainModule.filename`.
    var mainModule: String { "\(virtualWebRoot)/index.html" }
    var indexURL: URL { URL(string: "\(Self.origin)/index.html")! }

    /// Maps a virtual absolute path to disk. Nil when the path is relative or climbs out of the game folder.
    func resolve(virtualPath: String) -> URL? {
        guard let relative = Self.confine(virtualPath) else { return nil }
        return relative.isEmpty ? root : root.appendingPathComponent(relative)
    }

    /// Maps the path of a `vndegen://game/...` request to disk. Nil when it escapes the web root.
    func resolve(servedPath: String) -> URL? {
        let path = servedPath.isEmpty || servedPath == "/" ? "/index.html" : servedPath
        return resolve(virtualPath: virtualWebRoot + (path.hasPrefix("/") ? path : "/" + path))
    }

    /// Normalizes `.` and `..` lexically and returns the path relative to the virtual root. Nil when the path is not
    /// absolute or ends up outside the root. Symlinks are not followed: the folder is the user's own content.
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

nonisolated struct FileResponse: Equatable, Sendable {
    var status: Int
    var headers: [String: String]
    var body: Data

    static func empty(_ status: Int, headers: [String: String] = [:]) -> FileResponse {
        FileResponse(status: status, headers: headers.merging(["Content-Length": "0"]) { a, _ in a }, body: Data())
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
        guard let url = resolve(servedPath: path), !Self.isDirectory(url),
              let handle = try? FileHandle(forReadingFrom: url) else { return .empty(404) }
        defer { try? handle.close() }
        guard let end = try? handle.seekToEnd() else { return .empty(500) }
        let size = Int64(end)
        var headers = ["Content-Type": Self.mimeType(forExtension: url.pathExtension), "Accept-Ranges": "bytes"]
        let status: Int, first: Int64, last: Int64
        switch Self.byteRange(header: range, size: size) {
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
        if method == "HEAD" || length == 0 { return FileResponse(status: status, headers: headers, body: Data()) }
        guard (try? handle.seek(toOffset: UInt64(first))) != nil,
              let body = try? handle.read(upToCount: Int(length)) else { return .empty(500) }
        return FileResponse(status: status, headers: headers, body: body)
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

    static func isDirectory(_ url: URL) -> Bool {
        var isDirectory: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory) && isDirectory.boolValue
    }
}
