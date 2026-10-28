import Foundation

/// An Electron app.asar, read in place. The file starts with two Chromium pickles: an 8-byte one holding the size of the
/// next, then one holding the JSON index as a length-prefixed string. File bytes follow, and each index entry gives its
/// offset from the end of the header and its size. Format: https://github.com/electron/asar#format
nonisolated struct AsarArchive: Sendable {
    /// Where a member's bytes are.
    enum Member: Equatable, Sendable {
        /// Inside the archive file, at an absolute offset. The whole span lies inside the file as it was when read.
        case packed(offset: UInt64, size: Int64)
        /// Beside the archive in `app.asar.unpacked/`, at this path relative to it.
        case unpacked(String)
    }

    private indirect enum Node: Sendable {
        /// `offset` is absolute.
        case file(offset: UInt64, size: Int64, unpacked: Bool)
        case directory([String: Node])
        case link(String)
    }

    private let root: [String: Node]

    /// Limits on the index, checked before it is read or parsed. Real game archives have indexes of a few MiB at most.
    static let maxIndexBytes = 32 << 20
    static let maxEntries = 1_000_000
    static let maxDepth = 64

    /// Reads the index through `handle`, positioned anywhere. Throws for anything malformed, including a member whose
    /// bytes would lie outside the file.
    init(reading handle: FileHandle) throws {
        let fileLength = try handle.seekToEnd()
        try handle.seek(toOffset: 0)
        let prefix = try handle.read(upToCount: 16) ?? Data()
        guard prefix.count == 16, Self.uint32(prefix, at: 0) == 4 else { throw Self.corrupt }
        let headerPickleSize = UInt64(Self.uint32(prefix, at: 4))
        let jsonSize = Int(Self.uint32(prefix, at: 12))
        let dataStart = 8 + headerPickleSize
        guard jsonSize > 0, jsonSize <= Self.maxIndexBytes, UInt64(jsonSize) + 8 <= headerPickleSize, dataStart <= fileLength,
              let json = try handle.read(upToCount: jsonSize), json.count == jsonSize, Self.nesting(of: json) <= 2 * Self.maxDepth + 2,
              let index = try JSONSerialization.jsonObject(with: json) as? [String: Any]
        else { throw Self.corrupt }
        var parser = IndexParser(dataStart: dataStart, dataLength: fileLength - dataStart)
        guard case .directory(let root) = try parser.node(index, depth: 0) else { throw Self.corrupt }
        self.root = root
    }

    /// The member at `path`, relative to the archive root with forward slashes. Names match exactly, or else ignoring
    /// case, as `GameFolder` does on disk. Links resolve inside the archive only. Nil for directories and misses.
    func member(_ path: String) -> Member? {
        guard var parts = Self.normalize(path) else { return nil }
        var hops = 0
        var directory = root
        var walked: [String] = []
        while !parts.isEmpty {
            guard let (actual, node) = Self.entry(parts.removeFirst(), in: directory) else { return nil }
            switch node {
            case .directory(let entries):
                directory = entries
                walked.append(actual)
            case let .file(offset, size, unpacked):
                guard parts.isEmpty else { return nil }
                return unpacked ? .unpacked((walked + [actual]).joined(separator: "/")) : .packed(offset: offset, size: size)
            case .link(let target):
                // A link names a path from the archive root. One that climbs out of the archive finds nothing.
                hops += 1
                guard hops <= 16, let resolved = Self.normalize(([target] + parts).joined(separator: "/")) else { return nil }
                (parts, directory, walked) = (resolved, root, [])
            }
        }
        return nil
    }

    /// Drops `.` and resolves `..` lexically. Nil when the path climbs above the archive root.
    private static func normalize(_ path: String) -> [String]? {
        var parts: [String] = []
        for part in path.split(separator: "/") where part != "." {
            if part == ".." {
                guard !parts.isEmpty else { return nil }
                parts.removeLast()
            } else {
                parts.append(String(part))
            }
        }
        return parts
    }

    private static func entry(_ name: String, in directory: [String: Node]) -> (String, Node)? {
        if let node = directory[name] { return (name, node) }
        return directory.keys.filter { $0.caseInsensitiveCompare(name) == .orderedSame }.min().map { ($0, directory[$0]!) }
    }

    /// Turns the parsed JSON into nodes, counting entries and depth and checking each packed span against the file.
    private struct IndexParser {
        let dataStart: UInt64
        let dataLength: UInt64
        var entries = 0

        mutating func node(_ value: [String: Any], depth: Int) throws -> Node {
            entries += 1
            guard entries <= AsarArchive.maxEntries, depth <= AsarArchive.maxDepth else { throw AsarArchive.corrupt }
            if let files = value["files"] as? [String: Any] {
                var children: [String: Node] = [:]
                for (name, child) in files {
                    guard let child = child as? [String: Any], !name.contains("/") else { throw AsarArchive.corrupt }
                    children[name] = try node(child, depth: depth + 1)
                }
                return .directory(children)
            }
            if let link = value["link"] as? String { return .link(link) }
            guard let number = value["size"] as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
                  let size = Int64(exactly: number), size >= 0
            else { throw AsarArchive.corrupt }
            if value["unpacked"] as? Bool == true { return .file(offset: 0, size: size, unpacked: true) }
            // The offset is a decimal string because a JavaScript number cannot hold every UInt64.
            guard let offset = (value["offset"] as? String).flatMap(UInt64.init),
                  offset <= dataLength, UInt64(size) <= dataLength - offset
            else { throw AsarArchive.corrupt }
            return .file(offset: dataStart + offset, size: size, unpacked: false)
        }
    }

    /// The deepest nesting of objects and arrays in `json`, found without parsing it, so a deeply nested index is
    /// refused before the JSON parser recurses into it.
    static func nesting(of json: Data) -> Int {
        var depth = 0, deepest = 0, inString = false, escaped = false
        for byte in json {
            if inString {
                if escaped {
                    escaped = false
                } else if byte == UInt8(ascii: "\\") {
                    escaped = true
                } else if byte == UInt8(ascii: "\"") {
                    inString = false
                }
                continue
            }
            switch byte {
            case UInt8(ascii: "\""): inString = true
            case UInt8(ascii: "{"), UInt8(ascii: "["):
                depth += 1
                deepest = max(deepest, depth)
            case UInt8(ascii: "}"), UInt8(ascii: "]"): depth -= 1
            default: break
            }
        }
        return deepest
    }

    private static func uint32(_ data: Data, at offset: Int) -> UInt32 {
        data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: offset, as: UInt32.self) }.littleEndian
    }

    private static let corrupt = CocoaError(.fileReadCorruptFile)
}
