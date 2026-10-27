import Foundation

/// A Godot resource pack's header and file list. The pack is a `.pck` file, or sits inside the game's Windows `.exe`:
/// in its "pck" section for Godot 4, or appended to it with its size and magic at the very end.
nonisolated struct GodotPack: Equatable, Sendable {
    /// The Godot version that exported the game, such as 4.3.0.
    var major: Int, minor: Int, patch: Int
    /// The pack's format: 1 for Godot 3, 2 for Godot 4.0 to 4.4, 3 and later for Godot 4.5 and later.
    var format: Int
    /// Where the pack starts in its file.
    var offset: UInt64
    /// The game encrypted its file list, with a key built into its own engine.
    var encryptedDirectory: Bool
    /// How many files the game encrypted that way.
    var encryptedFiles = 0
    /// Paths inside the pack, without `res://`. Empty when the list is encrypted.
    var files: [String]

    static let magic: UInt32 = 0x4350_4447 // "GDPC"

    var version: String { "\(major).\(minor).\(patch)" }

    /// Native libraries the game loads, which VNDegen can't: GDExtension in Godot 4 and GDNative in Godot 3.
    var nativeLibraries: [String] {
        files.filter { $0.hasSuffix(".gdextension") || $0.hasSuffix(".gdnlib") }
    }

    /// Reads the pack in `url`, which is a `.pck` or an `.exe`. Nil when the file holds no pack.
    init?(contentsOf url: URL) {
        guard let file = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? file.close() }
        guard let offset = Self.findPack(file), let pack = Self.read(file, at: offset) else { return nil }
        self = pack
    }

    init(major: Int, minor: Int, patch: Int, format: Int, offset: UInt64, encryptedDirectory: Bool, files: [String]) {
        self.major = major
        self.minor = minor
        self.patch = patch
        self.format = format
        self.offset = offset
        self.encryptedDirectory = encryptedDirectory
        self.files = files
    }

    private static func findPack(_ file: FileHandle) -> UInt64? {
        guard let start = file.readUInt32(at: 0) else { return nil }
        if start == magic { return 0 }
        // A Godot 4 executable keeps its pack in a section named "pck". Signed executables have data after the pack, so
        // the section is the reliable way to find it.
        if start & 0xFFFF == 0x5A4D, let pe = file.readUInt32(at: 0x3C).map(UInt64.init), file.readUInt32(at: pe) == 0x0000_4550,
           let sections = file.readUInt16(at: pe + 6), let optionalSize = file.readUInt16(at: pe + 20) {
            for index in 0..<UInt64(sections) {
                let header = pe + 24 + UInt64(optionalSize) + index * 40
                guard let name = file.read(at: header, count: 8), name == Data("pck".utf8) + Data(count: 5),
                      let rawOffset = file.readUInt32(at: header + 20) else { continue }
                // The header can start a few bytes in, as Godot's own search allows.
                for skip in 0..<UInt64(8) where file.readUInt32(at: UInt64(rawOffset) + skip) == magic {
                    return UInt64(rawOffset) + skip
                }
            }
        }
        // Godot 3, and Godot 4 executables from other tools: the pack's size and magic end the file.
        guard let end = try? file.seekToEnd(), end >= 12, file.readUInt32(at: end - 4) == magic,
              let size = file.readUInt64(at: end - 12), size + 12 <= end else { return nil }
        let offset = end - 12 - size
        return file.readUInt32(at: offset) == magic ? offset : nil
    }

    private static func read(_ file: FileHandle, at offset: UInt64) -> GodotPack? {
        // Magic, format, version, then from format 2 on flags and file base, and from format 3 on the directory offset.
        guard let header = file.read(at: offset, count: 40) else { return nil }
        let format = Int(header.uint32(at: 4)), major = Int(header.uint32(at: 8)), minor = Int(header.uint32(at: 12))
        let patch = Int(header.uint32(at: 16))
        guard (1...4).contains(format), (2...4).contains(major) else { return nil }
        var flags: UInt32 = 0
        var cursor = offset + 20
        if format >= 2 {
            flags = header.uint32(at: 20)
            cursor += 4 + 8 // flags, file base
        }
        if format >= 3 {
            cursor = offset + header.uint64(at: 32) // directory offset
        } else {
            cursor += 16 * 4 // reserved
        }
        var pack = GodotPack(major: major, minor: minor, patch: patch, format: format, offset: offset,
                             encryptedDirectory: flags & 1 != 0, files: [])
        guard !pack.encryptedDirectory, let count = file.readUInt32(at: cursor) else { return pack }
        cursor += 4
        // Each entry: path length, path, offset, size, MD5, and from format 2 on, flags.
        let entryTail: UInt64 = 8 + 8 + 16 + (format >= 2 ? 4 : 0)
        var files: [String] = []
        files.reserveCapacity(Int(min(count, 200_000)))
        for _ in 0..<min(count, 200_000) {
            guard let length = file.readUInt32(at: cursor), length < 4096,
                  let path = file.read(at: cursor + 4, count: Int(length)) else { break }
            cursor += 4 + UInt64(length) + entryTail
            // Bit 0 of an entry's flags marks an encrypted file.
            if format >= 2, let flags = file.readUInt32(at: cursor - 4), flags & 1 != 0 { pack.encryptedFiles += 1 }
            let name = String(decoding: path.prefix { $0 != 0 }, as: UTF8.self)
            files.append(name.hasPrefix("res://") ? String(name.dropFirst(6)) : name)
        }
        pack.files = files
        return pack
    }
}

nonisolated private extension FileHandle {
    func read(at offset: UInt64, count: Int) -> Data? {
        guard (try? seek(toOffset: offset)) != nil, let data = try? read(upToCount: count), data.count == count else { return nil }
        return data
    }

    func readUInt16(at offset: UInt64) -> UInt16? { read(at: offset, count: 2).map { UInt16($0[$0.startIndex]) | UInt16($0[$0.startIndex + 1]) << 8 } }
    func readUInt32(at offset: UInt64) -> UInt32? { read(at: offset, count: 4).map { $0.uint32(at: 0) } }
    func readUInt64(at offset: UInt64) -> UInt64? { read(at: offset, count: 8).map { $0.uint64(at: 0) } }
}

nonisolated private extension Data {
    func uint32(at index: Int) -> UInt32 {
        (0..<4).reduce(0) { $0 | UInt32(self[startIndex + index + $1]) << (8 * $1) }
    }

    func uint64(at index: Int) -> UInt64 {
        (0..<8).reduce(0) { $0 | UInt64(self[startIndex + index + $1]) << (8 * $1) }
    }
}
