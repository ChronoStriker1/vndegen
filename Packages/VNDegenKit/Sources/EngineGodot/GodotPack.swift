import CommonCrypto
import CryptoKit
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
    /// Where an encrypted part of the pack starts in its file: the file list when that is encrypted, else the smallest
    /// encrypted file. The key search decrypts it.
    var encryptedSample: UInt64?
    /// Paths inside the pack, without `res://`. Empty when the list is encrypted.
    var files: [String]
    /// The project settings VNDegen reads, from the pack's `project.binary`: strings and booleans, by key.
    var settings: [String: Setting] = [:]

    enum Setting: Equatable, Sendable {
        case string(String)
        case bool(Bool)

        var string: String? {
            if case let .string(value) = self { value } else { nil }
        }
    }

    /// The settings `settings` holds.
    static let settingKeys: Set<String> = [
        "application/config/name", "application/config/use_custom_user_dir", "application/config/custom_user_dir_name",
    ]

    /// Where the game keeps `user://`, relative to the Windows %APPDATA% folder, as the Godot that runs it names it:
    /// `Godot/app_userdata/<project name>`, or the project's own folder name. A project without a name gets no folder
    /// of its own.
    var userDirectory: String {
        let name = Self.safeDirectoryName(settings["application/config/name"].flatMap(\.string) ?? "", major: major)
        guard !name.isEmpty else { return "Godot/app_userdata/[unnamed project]" }
        if settings["application/config/use_custom_user_dir"] == .bool(true) {
            let custom = Self.safeDirectoryName(settings["application/config/custom_user_dir_name"].flatMap(\.string) ?? "",
                                                allowPaths: true, major: major)
            return custom.isEmpty ? name : custom
        }
        return "Godot/app_userdata/" + name
    }

    /// `OS::get_safe_dir_name` of the Godot that runs the game, 4.7 or 3.6: invalid characters become dashes, and
    /// Godot 4 also drops trailing periods, as Windows does, and renames `.` and `..`.
    static func safeDirectoryName(_ name: String, allowPaths: Bool = false, major: Int) -> String {
        // Godot's strip_edges drops every character up to the space.
        let edges = CharacterSet(charactersIn: "\u{0}"..."\u{20}")
        var safe = name.replacingOccurrences(of: "\\", with: "/")
        if allowPaths, major >= 4 { safe = safe.replacingOccurrences(of: "//", with: "/") }
        safe = safe.trimmingCharacters(in: edges)
        if !allowPaths, major >= 4, let renamed = [".": "dot", "..": "twodots"][safe] { safe = renamed }
        for character in [":", "*", "?", "\"", "<", ">", "|"] + (allowPaths ? [".."] : ["/"]) {
            safe = safe.replacingOccurrences(of: character, with: "-")
        }
        if major >= 4 {
            while safe.hasSuffix(".") { safe.removeLast() }
        }
        return safe
    }

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
              let size = file.readUInt64(at: end - 12), size <= end - 12 else { return nil }
        let offset = end - 12 - size
        return file.readUInt32(at: offset) == magic ? offset : nil
    }

    /// Offsets in a pack come from the file, so they add with wrapping: a bad one points past the end, where every read
    /// fails, rather than trapping.
    private static func read(_ file: FileHandle, at offset: UInt64) -> GodotPack? {
        // Magic, format, version, then from format 2 on flags and file base, and from format 3 on the directory offset.
        guard let header = file.read(at: offset, count: 40) else { return nil }
        let format = Int(header.uint32(at: 4)), major = Int(header.uint32(at: 8)), minor = Int(header.uint32(at: 12))
        let patch = Int(header.uint32(at: 16))
        guard (1...4).contains(format), (2...4).contains(major) else { return nil }
        var flags: UInt32 = 0
        var cursor = offset + 20
        // Where the offsets in the file list count from.
        var fileBase = offset
        if format >= 2 {
            flags = header.uint32(at: 20)
            fileBase = header.uint64(at: 24) &+ (flags & 2 != 0 || format >= 3 ? offset : 0)
            cursor += 4 + 8 // flags, file base
        }
        if format >= 3 {
            cursor = offset &+ header.uint64(at: 32) // directory offset
        } else {
            cursor += 16 * 4 // reserved
        }
        var pack = GodotPack(major: major, minor: minor, patch: patch, format: format, offset: offset,
                             encryptedDirectory: flags & 1 != 0, files: [])
        if pack.encryptedDirectory { pack.encryptedSample = cursor }
        guard !pack.encryptedDirectory, let count = file.readUInt32(at: cursor) else { return pack }
        cursor &+= 4
        // Each entry: path length, path, offset, size, MD5, and from format 2 on, flags.
        let entryTail: UInt64 = 8 + 8 + 16 + (format >= 2 ? 4 : 0)
        var files: [String] = []
        files.reserveCapacity(Int(min(count, 200_000)))
        var project: (offset: UInt64, size: UInt64)?
        var smallestEncrypted = UInt64.max
        for _ in 0..<min(count, 200_000) {
            guard let length = file.readUInt32(at: cursor), length < 4096,
                  let path = file.read(at: cursor &+ 4, count: Int(length)) else { break }
            let entry = cursor &+ 4 &+ UInt64(length)
            cursor = entry &+ entryTail
            // Bit 0 of an entry's flags marks an encrypted file.
            if format >= 2, let flags = file.readUInt32(at: cursor &- 4), flags & 1 != 0 {
                pack.encryptedFiles += 1
                if let start = file.readUInt64(at: entry), let size = file.readUInt64(at: entry &+ 8), size > 0, size < smallestEncrypted {
                    smallestEncrypted = size
                    pack.encryptedSample = fileBase &+ start
                }
            }
            var name = String(decoding: path.prefix { $0 != 0 }, as: UTF8.self)
            if name.hasPrefix("res://") { name.removeFirst(6) }
            if name == "project.binary", let start = file.readUInt64(at: entry), let size = file.readUInt64(at: entry &+ 8) {
                project = (fileBase &+ start, size)
            }
            files.append(name)
        }
        pack.files = files
        if let project, project.size < 1 << 22, let data = file.read(at: project.offset, count: Int(project.size)) {
            pack.settings = settings(data)
        }
        return pack
    }

    /// The keys in `settingKeys` from a `project.binary`: "ECFG", a count, then each setting as its key's length and
    /// UTF-8, the value's length, and the value as Godot encodes a Variant.
    static func settings(_ data: Data) -> [String: Setting] {
        guard data.count >= 8, data.prefix(4) == Data("ECFG".utf8) else { return [:] }
        var result: [String: Setting] = [:]
        var cursor = 8
        for _ in 0..<min(Int(data.uint32(at: 4)), 100_000) {
            guard cursor + 4 <= data.count else { break }
            let keyLength = Int(data.uint32(at: cursor))
            guard cursor + 4 + keyLength + 4 <= data.count else { break }
            let key = String(decoding: data[(data.startIndex + cursor + 4)..<(data.startIndex + cursor + 4 + keyLength)], as: UTF8.self)
            cursor += 4 + keyLength
            let valueLength = Int(data.uint32(at: cursor))
            cursor += 4
            guard cursor + valueLength <= data.count else { break }
            if settingKeys.contains(key), valueLength >= 8 {
                // The type is the low byte of the Variant's header: 1 is a boolean, 4 a string, the same in Godot 3 and 4.
                switch data.uint32(at: cursor) & 0xFF {
                case 1: result[key] = .bool(data.uint32(at: cursor + 4) != 0)
                case 4:
                    let length = Int(data.uint32(at: cursor + 4))
                    if 8 + length <= valueLength {
                        let start = data.startIndex + cursor + 8
                        result[key] = .string(String(decoding: data[start..<(start + length)], as: UTF8.self))
                    }
                default: break
                }
            }
            cursor += valueLength
        }
        return result
    }
}

nonisolated extension GodotPack {
    /// The AES-256 key the pack is encrypted with. Godot builds the key into the game's own engine as a 32-byte array,
    /// which an export keeps in the `.exe`'s `.data` section. Each 32 bytes there, at every fourth byte, decrypts
    /// `encryptedSample`, and the key is the one that reproduces the MD5 Godot stored with it. Nil when no candidate
    /// does, or the pack isn't encrypted.
    func encryptionKey(pack url: URL, executable: URL) -> Data? {
        // Godot writes an encrypted part as its MD5, its length, and its IV, then the data in whole AES blocks.
        guard let start = encryptedSample, let file = try? FileHandle(forReadingFrom: url),
              let header = file.read(at: start, count: 40) else { return nil }
        let md5 = Data(header.prefix(16)), length = header.uint64(at: 16), iv = Data(header.suffix(16))
        guard length > 0, length < 1 << 26, let cipher = file.read(at: start + 40, count: Int((length + 15) / 16 * 16)),
              let data = WindowsExecutable(executable)?.contents(of: ".data"), data.count >= 32 else { return nil }
        for offset in stride(from: data.startIndex, through: data.endIndex - 32, by: 4) {
            let key = Data(data[offset..<(offset + 32)])
            // A file list is large, so its first block rules most keys out first: CFB's first block is the IV encrypted
            // with the key, XORed with the data, and a file list starts with its count and the first path's length.
            if encryptedDirectory {
                guard let stream = Self.aes(key, iv, iv, CCOperation(kCCEncrypt), CCMode(kCCModeECB)) else { continue }
                let plain = Data(zip(stream, cipher.prefix(16)).map { $0 ^ $1 })
                guard (1...200_000).contains(plain.uint32(at: 0)), (1..<4096).contains(plain.uint32(at: 4)) else { continue }
            }
            if let plain = Self.aes(key, iv, cipher, CCOperation(kCCDecrypt), CCMode(kCCModeCFB)),
               Data(Insecure.MD5.hash(data: plain.prefix(Int(length)))) == md5 {
                return key
            }
        }
        return nil
    }

    /// AES-256 over whole blocks, in Godot's CFB mode, or in ECB mode, which ignores `iv`.
    private static func aes(_ key: Data, _ iv: Data, _ input: Data, _ operation: CCOperation, _ mode: CCMode) -> Data? {
        var cryptor: CCCryptorRef?
        let created = key.withUnsafeBytes { key in
            iv.withUnsafeBytes { iv in
                CCCryptorCreateWithMode(operation, mode, CCAlgorithm(kCCAlgorithmAES), CCPadding(ccNoPadding), iv.baseAddress,
                                        key.baseAddress, key.count, nil, 0, 0, CCModeOptions(0), &cryptor)
            }
        }
        guard created == kCCSuccess, let cryptor else { return nil }
        defer { CCCryptorRelease(cryptor) }
        var output = Data(count: input.count)
        var moved = 0
        let status = output.withUnsafeMutableBytes { output in
            input.withUnsafeBytes { input in
                CCCryptorUpdate(cryptor, input.baseAddress, input.count, output.baseAddress, output.count, &moved)
            }
        }
        return status == kCCSuccess && moved == input.count ? output : nil
    }
}

nonisolated private extension FileHandle {
    func read(at offset: UInt64, count: Int) -> Data? {
        // Offsets past Int64 can't be seeked to, and Foundation may trap on them.
        guard offset <= UInt64(Int64.max), (try? seek(toOffset: offset)) != nil, let data = try? read(upToCount: count),
              data.count == count else { return nil }
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
