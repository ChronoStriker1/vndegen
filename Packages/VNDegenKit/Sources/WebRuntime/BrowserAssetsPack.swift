import Foundation

/// The `browser_assets` pack of a Unity game that shows a web page through Zen Fulcrum's Embedded Browser, read in place.
/// It is a .NET `BinaryWriter` stream: the string `zfbRes_v1`, an Int32 entry count, then each entry's path from `/`,
/// its Int64 offset from the start of the file and its Int32 length, all little-endian, with the files after them.
/// Strings are UTF-8 behind their length in 7-bit groups.
nonisolated public struct BrowserAssetsPack: Sendable {
    /// Each member's span, by its path without the leading `/`.
    private let members: [String: FileSlice]
    /// The paths by their lowercased form, for lookups that ignore case. The smallest path wins a collision.
    private let folded: [String: String]

    /// The pack's name in `<Game>_Data/Resources/`.
    public static let fileName = "browser_assets"
    static let magic = "zfbRes_v1"
    /// Limits checked while the index is read. Real packs hold a few hundred files.
    static let maxEntries = 1_000_000
    static let maxNameBytes = 4096

    /// Reads the index through `handle`. Throws for anything malformed, including a member whose bytes would lie outside
    /// the file.
    init(reading handle: FileHandle) throws {
        let fileLength = try handle.seekToEnd()
        try handle.seek(toOffset: 0)
        var reader = Reader(handle: handle)
        guard try reader.string() == Self.magic else { throw Self.corrupt }
        let count = try reader.int32()
        guard count >= 0, count <= Self.maxEntries else { throw Self.corrupt }
        var members: [String: FileSlice] = [:]
        var folded: [String: String] = [:]
        for _ in 0..<count {
            let name = try reader.string()
            let offset = try reader.int64(), length = try reader.int32()
            guard offset >= 0, length >= 0, UInt64(offset) <= fileLength, UInt64(length) <= fileLength - UInt64(offset)
            else { throw Self.corrupt }
            let path = String(name.drop { $0 == "/" })
            members[path] = FileSlice(offset: UInt64(offset), length: Int64(length))
            let lower = path.lowercased()
            if let other = folded[lower], other < path { continue }
            folded[lower] = path
        }
        self.members = members
        self.folded = folded
    }

    /// The member at `path`, relative to the pack with forward slashes. Names match exactly, or else ignoring case, as
    /// `GameFolder` does on disk. `.` and `..` resolve lexically and may not climb out of the pack.
    func member(_ path: String) -> AsarArchive.Member? {
        var parts: [Substring] = []
        for part in path.split(separator: "/") where part != "." {
            if part == ".." {
                guard !parts.isEmpty else { return nil }
                parts.removeLast()
            } else {
                parts.append(part)
            }
        }
        let key = parts.joined(separator: "/")
        guard let slice = members[key] ?? folded[key.lowercased()].flatMap({ members[$0] }) else { return nil }
        return .packed(offset: slice.offset, size: slice.length)
    }

    /// Reads the index a chunk at a time, so opening a pack never reads its files.
    private struct Reader {
        let handle: FileHandle
        var buffer = Data()
        var position = 0

        mutating func bytes(_ count: Int) throws -> Data {
            while buffer.count - position < count {
                guard let chunk = try handle.read(upToCount: max(64 << 10, count)), !chunk.isEmpty else { throw BrowserAssetsPack.corrupt }
                buffer = buffer.subdata(in: position..<buffer.count) + chunk
                position = 0
            }
            defer { position += count }
            return buffer.subdata(in: position..<position + count)
        }

        mutating func int32() throws -> Int32 {
            Int32(littleEndian: try bytes(4).withUnsafeBytes { $0.loadUnaligned(as: Int32.self) })
        }

        mutating func int64() throws -> Int64 {
            Int64(littleEndian: try bytes(8).withUnsafeBytes { $0.loadUnaligned(as: Int64.self) })
        }

        /// .NET's `BinaryWriter.Write(String)`: the UTF-8 length in 7-bit groups, low first, then the bytes.
        mutating func string() throws -> String {
            var length = 0, shift = 0
            while true {
                let byte = try bytes(1)[0]
                length |= Int(byte & 0x7F) << shift
                if byte & 0x80 == 0 { break }
                shift += 7
                guard shift < 28 else { throw BrowserAssetsPack.corrupt }
            }
            guard length <= BrowserAssetsPack.maxNameBytes, let text = String(data: try bytes(length), encoding: .utf8) else {
                throw BrowserAssetsPack.corrupt
            }
            return text
        }
    }

    private static let corrupt = CocoaError(.fileReadCorruptFile)
}
