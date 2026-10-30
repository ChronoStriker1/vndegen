import CommonCrypto
import CryptoKit
import Foundation
import Synchronization

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
    /// The key the game encrypted its pack with, found in its `.exe`. The file list and settings were read through it.
    var encryptionKey: Data?
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

    /// Reads the pack in `url`, which is a `.pck` or an `.exe`. Nil when the file holds no pack. An encrypted pack is
    /// read with the key found in `executable`, the game's `.exe`.
    init?(contentsOf url: URL, executable: URL? = nil) {
        guard let file = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? file.close() }
        guard let offset = Self.findPack(file), let pack = Self.read(file, at: offset, executable: executable) else { return nil }
        self = pack
    }

    /// Whether the game encrypted any of its pack.
    var encrypted: Bool { encryptedDirectory || encryptedFiles > 0 }

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
    private static func read(_ file: FileHandle, at offset: UInt64, executable: URL?) -> GodotPack? {
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
        guard let count = file.readUInt32(at: cursor) else { return pack }
        cursor &+= 4
        // The entries follow the count, which Godot leaves unencrypted when it encrypts the file list.
        let entries: (UInt64, Int) -> Data?
        if pack.encryptedDirectory {
            guard let executable, let key = encryptionKey(file, sample: cursor, plaintext: .fileList, executable: executable),
                  let list = decrypt(file, at: cursor, key: key) else { return pack }
            pack.encryptionKey = key
            entries = { offset, count in
                offset <= UInt64(list.count) && count <= list.count - Int(offset) ? list.subdata(in: Int(offset)..<(Int(offset) + count)) : nil
            }
            cursor = 0
        } else {
            entries = { file.read(at: $0, count: $1) }
        }
        let u32 = { entries($0, 4).map { $0.uint32(at: 0) } }
        let u64 = { entries($0, 8).map { $0.uint64(at: 0) } }
        // Each entry: path length, path, offset, size, MD5, and from format 2 on, flags.
        let entryTail: UInt64 = 8 + 8 + 16 + (format >= 2 ? 4 : 0)
        var files: [String] = []
        files.reserveCapacity(Int(min(count, 200_000)))
        var encryptedFiles: [(name: String, offset: UInt64, size: UInt64)] = []
        var project: (offset: UInt64, size: UInt64, encrypted: Bool)?
        for _ in 0..<min(count, 200_000) {
            guard let length = u32(cursor), length < 4096, let path = entries(cursor &+ 4, Int(length)) else { break }
            let entry = cursor &+ 4 &+ UInt64(length)
            cursor = entry &+ entryTail
            var name = String(decoding: path.prefix { $0 != 0 }, as: UTF8.self)
            if name.hasPrefix("res://") { name.removeFirst(6) }
            guard let start = u64(entry), let size = u64(entry &+ 8) else { break }
            // Bit 0 of an entry's flags marks an encrypted file.
            let encrypted = format >= 2 && (u32(cursor &- 4) ?? 0) & 1 != 0
            if encrypted {
                pack.encryptedFiles += 1
                encryptedFiles.append((name, fileBase &+ start, size))
            }
            if name == "project.binary" { project = (fileBase &+ start, size, encrypted) }
            files.append(name)
        }
        pack.files = files
        if pack.encryptionKey == nil, let executable {
            pack.encryptionKey = encryptionKey(file, among: encryptedFiles, executable: executable)
        }
        if let project, project.size < 1 << 22 {
            let data = project.encrypted ? pack.encryptionKey.flatMap { decrypt(file, at: project.offset, key: $0) }
                : file.read(at: project.offset, count: Int(project.size))
            pack.settings = data.map(settings) ?? [:]
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

/// Finding the key of an encrypted pack. Godot builds the key into the game's own engine as a 32-byte array, which an
/// export keeps in the `.exe`'s `.data` section. Each 32 bytes there, at every fourth byte, is tried on one encrypted
/// part of the pack, and the key is the one that reproduces the MD5 Godot stored with that part.
nonisolated extension GodotPack {
    /// How a correct key's first block of a part reads. Each candidate decrypts only that block first, so the search
    /// costs one AES block per candidate and decrypts the whole part only for the few that pass.
    enum Plaintext: Equatable {
        /// A file that starts with one of these.
        case magic([Data])
        /// A text file in ASCII.
        case text
        /// The file list after its count: the first path's length, then the path.
        case fileList
        /// A file whose start can't be told, which every candidate decrypts in full.
        case unknown

        init(path: String) {
            let name = path.lowercased()
            switch name.split(separator: ".").last ?? "" {
            case _ where name.hasSuffix("project.binary"): self = .magic([Data("ECFG".utf8)])
            case "gdc": self = .magic([Data("GDSC".utf8)])
            case "res", "scn": self = .magic([Data("RSRC".utf8), Data("RSCC".utf8)])
            case "ctex", "stex": self = .magic([Data("GST2".utf8), Data("GDST".utf8)])
            case "gd", "tscn", "tres", "remap", "import", "cfg", "json", "txt", "gdshader", "csv": self = .text
            default: self = .unknown
            }
        }

        func matches(_ block: Data) -> Bool {
            let printable = { (bytes: Data) in bytes.allSatisfy { (0x20..<0x7F).contains($0) || [0x09, 0x0A, 0x0D].contains($0) } }
            switch self {
            case let .magic(starts): return starts.contains { block.starts(with: $0) }
            case .text: return printable(block)
            case .fileList:
                // Godot pads each path with zeros to a multiple of four bytes.
                let length = Int(block.uint32(at: 0))
                return (1..<4096).contains(length) && printable(block.dropFirst(4).prefix(length).filter { $0 != 0 })
            case .unknown: return true
            }
        }
    }

    /// The key for one of the encrypted files. The smallest file of each kind is tried, the surest kind first: a file
    /// with a magic number, then a text file, which may not be ASCII after all, then a file of another kind. Every
    /// candidate decrypts all of that last kind, so only one of 512 bytes or less is tried.
    fileprivate static func encryptionKey(_ file: FileHandle, among files: [(name: String, offset: UInt64, size: UInt64)],
                                          executable: URL) -> Data? {
        let rank = { (plaintext: Plaintext) in
            switch plaintext {
            case .magic: 0
            case .text: 1
            case .fileList, .unknown: 2
            }
        }
        var tried: Set<Int> = []
        let samples = files.filter { $0.size > 0 }.map { (file: $0, plaintext: Plaintext(path: $0.name)) }
        for (sample, plaintext) in samples.sorted(by: { (rank($0.plaintext), $0.file.size) < (rank($1.plaintext), $1.file.size) }) {
            let kind = rank(plaintext)
            guard !tried.contains(kind), plaintext != .unknown || sample.size <= 512 else { continue }
            tried.insert(kind)
            if let key = encryptionKey(file, sample: sample.offset, plaintext: plaintext, executable: executable) { return key }
        }
        return nil
    }

    /// Keys already searched for, by executable and part, since the game's page and its launch both read the pack.
    private static let keys = Mutex<[String: Data?]>([:])

    /// The key in `executable` that decrypts the encrypted part at `sample`, or nil.
    static func encryptionKey(_ file: FileHandle, sample: UInt64, plaintext: Plaintext, executable: URL) -> Data? {
        let attributes = try? FileManager.default.attributesOfItem(atPath: executable.path)
        let id = "\(executable.path)|\(attributes?[.size] ?? "")|\(attributes?[.modificationDate] ?? "")|\(sample)"
        if let known = keys.withLock({ $0[id] }) { return known }
        let key = searchKey(file, sample: sample, plaintext: plaintext, executable: executable)
        keys.withLock { $0[id] = key }
        return key
    }

    private static func searchKey(_ file: FileHandle, sample: UInt64, plaintext: Plaintext, executable: URL) -> Data? {
        guard let part = EncryptedPart(file, at: sample), let data = WindowsExecutable(executable)?.contents(of: ".data"),
              data.count >= 32 else { return nil }
        let first = part.cipher.prefix(16)
        for offset in stride(from: data.startIndex, through: data.endIndex - 32, by: 4) {
            let key = Data(data[offset..<(offset + 32)])
            // CFB's first block is the IV encrypted with the key, XORed with the data.
            // A part shorter than a block is padded, so only its own bytes count.
            if plaintext != .unknown {
                guard let stream = aes(key, part.iv, part.iv, CCOperation(kCCEncrypt), CCMode(kCCModeECB)),
                      plaintext.matches(Data(zip(stream, first).map { $0 ^ $1 }.prefix(part.length))) else { continue }
            }
            if part.decrypt(key) != nil { return key }
        }
        return nil
    }

    /// The encrypted part at `offset` decrypted with `key`, or nil when its MD5 doesn't match.
    fileprivate static func decrypt(_ file: FileHandle, at offset: UInt64, key: Data) -> Data? {
        EncryptedPart(file, at: offset)?.decrypt(key)
    }

    /// An encrypted file or file list as Godot writes it in a pack: its MD5, its length, and its IV, then the data in
    /// AES-256 CFB, in whole blocks.
    private struct EncryptedPart {
        var md5: Data, length: Int, iv: Data, cipher: Data

        init?(_ file: FileHandle, at offset: UInt64) {
            guard let header = file.read(at: offset, count: 40) else { return nil }
            let length = header.uint64(at: 16)
            guard length > 0, length < 1 << 26, let cipher = file.read(at: offset &+ 40, count: Int((length + 15) / 16 * 16)) else { return nil }
            self.md5 = Data(header.prefix(16))
            self.length = Int(length)
            self.iv = Data(header.suffix(16))
            self.cipher = cipher
        }

        func decrypt(_ key: Data) -> Data? {
            guard let plain = GodotPack.aes(key, iv, cipher, CCOperation(kCCDecrypt), CCMode(kCCModeCFB))?.prefix(length),
                  Data(Insecure.MD5.hash(data: plain)) == md5 else { return nil }
            return Data(plain)
        }
    }

    /// AES-256 over whole blocks, in Godot's CFB mode, or in ECB mode, which ignores `iv`.
    fileprivate static func aes(_ key: Data, _ iv: Data, _ input: Data, _ operation: CCOperation, _ mode: CCMode) -> Data? {
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
