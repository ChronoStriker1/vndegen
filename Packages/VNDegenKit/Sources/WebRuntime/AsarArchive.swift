import Foundation

/// An Electron app.asar, read in place. The file starts with two Chromium pickles: an 8-byte one holding the size of the
/// next, then one holding the JSON index as a length-prefixed string. File bytes follow, and each index entry gives its
/// offset from the end of the header and its size. Format: https://github.com/electron/asar#format
nonisolated struct AsarArchive: Sendable {
    /// Where a member's bytes are.
    enum Member: Equatable, Sendable {
        /// Inside the archive file, at an absolute offset.
        case packed(offset: UInt64, size: UInt64)
        /// Beside the archive in `app.asar.unpacked/`, at this path relative to it.
        case unpacked(String)
    }

    private indirect enum Node: Sendable {
        case file(offset: UInt64, size: UInt64, unpacked: Bool)
        case directory([String: Node])
        case link(String)
    }

    private let root: [String: Node]
    private let dataStart: UInt64

    /// Reads the index through `handle`, positioned anywhere.
    init(reading handle: FileHandle) throws {
        try handle.seek(toOffset: 0)
        let prefix = try handle.read(upToCount: 16) ?? Data()
        guard prefix.count == 16, Self.uint32(prefix, at: 0) == 4 else { throw Self.corrupt }
        let headerPickleSize = UInt64(Self.uint32(prefix, at: 4))
        let jsonSize = Int(Self.uint32(prefix, at: 12))
        guard jsonSize > 0, UInt64(jsonSize) + 8 <= headerPickleSize, let json = try handle.read(upToCount: jsonSize), json.count == jsonSize,
              let index = try JSONSerialization.jsonObject(with: json) as? [String: Any],
              case .directory(let root) = try Self.node(index)
        else { throw Self.corrupt }
        self.root = root
        dataStart = 8 + headerPickleSize
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
                return unpacked ? .unpacked((walked + [actual]).joined(separator: "/")) : .packed(offset: dataStart + offset, size: size)
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
        return directory.keys.sorted().first { $0.caseInsensitiveCompare(name) == .orderedSame }.map { ($0, directory[$0]!) }
    }

    private static func node(_ value: [String: Any]) throws -> Node {
        if let files = value["files"] as? [String: Any] {
            var entries: [String: Node] = [:]
            for (name, child) in files {
                guard let child = child as? [String: Any], !name.contains("/") else { throw corrupt }
                entries[name] = try node(child)
            }
            return .directory(entries)
        }
        if let link = value["link"] as? String { return .link(link) }
        guard let size = (value["size"] as? NSNumber)?.uint64Value else { throw corrupt }
        if value["unpacked"] as? Bool == true { return .file(offset: 0, size: size, unpacked: true) }
        // The offset is a decimal string because a JavaScript number cannot hold every UInt64.
        guard let offset = (value["offset"] as? String).flatMap(UInt64.init) else { throw corrupt }
        return .file(offset: offset, size: size, unpacked: false)
    }

    private static func uint32(_ data: Data, at offset: Int) -> UInt32 {
        data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: offset, as: UInt32.self) }.littleEndian
    }

    private static let corrupt = CocoaError(.fileReadCorruptFile)
}
