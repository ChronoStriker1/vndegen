// Inno Setup 5 header reader for RuntimePackages.
//
// The format knowledge here, including which fields each Inno Setup version stores and in what order, comes from
// innoextract (https://github.com/dscharrer/innoextract), whose loading code this file ports to Swift. Altered from the
// original: only Inno Setup 5 is read, and only the entries up to the file entries plus the data entries.
// innoextract's notice follows, as its license requires.
//
//   Copyright (C) 2011-2020 Daniel Scharrer <daniel@constexpr.org>
//
//   This software is provided 'as-is', without any express or implied
//   warranty.  In no event will the author(s) be held liable for any damages
//   arising from the use of this software.
//
//   Permission is granted to anyone to use this software for any purpose,
//   including commercial applications, and to alter it and redistribute it
//   freely, subject to the following restrictions:
//
//   1. The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software
//      in a product, an acknowledgment in the product documentation would be
//      appreciated but is not required.
//   2. Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
//   3. This notice may not be removed or altered from any source distribution.

import CLzmaDec
import Foundation

/// The setup data version an installer was built with, such as 5.4.2 (unicode).
struct InnoVersion: Comparable, CustomStringConvertible {
    let major: Int, minor: Int, patch: Int
    var isUnicode = false

    init(_ major: Int, _ minor: Int, _ patch: Int, unicode: Bool = false) {
        (self.major, self.minor, self.patch, isUnicode) = (major, minor, patch, unicode)
    }

    static func < (lhs: InnoVersion, rhs: InnoVersion) -> Bool {
        (lhs.major, lhs.minor, lhs.patch) < (rhs.major, rhs.minor, rhs.patch)
    }

    static func == (lhs: InnoVersion, rhs: InnoVersion) -> Bool {
        (lhs.major, lhs.minor, lhs.patch) == (rhs.major, rhs.minor, rhs.patch)
    }

    var description: String { "\(major).\(minor).\(patch)\(isUnicode ? " (unicode)" : "")" }

    /// Parses the 64-byte data version string, such as "Inno Setup Setup Data (5.4.2) (u)".
    init(stored bytes: [UInt8]) throws {
        let text = String(decoding: bytes.prefix { $0 != 0 }, as: UTF8.self)
        let prefix = "Inno Setup Setup Data ("
        guard text.hasPrefix(prefix) else {
            throw RuntimeInstallError.unsupportedInstaller("unknown setup data version \"\(text)\"")
        }
        let numbers = text.dropFirst(prefix.count).prefix { $0 != ")" }.split(separator: ".").map { Int($0) }
        guard numbers.count == 3, let major = numbers[0], let minor = numbers[1], let patch = numbers[2] else {
            throw RuntimeInstallError.unsupportedInstaller("unknown setup data version \"\(text)\"")
        }
        let rest = text.dropFirst(prefix.count).drop { $0 != ")" }.dropFirst()
        guard rest.isEmpty || rest == " (u)" || rest == " (U)" else {
            throw RuntimeInstallError.unsupportedInstaller("unsupported Inno Setup variant \"\(text)\"")
        }
        self.init(major, minor, patch, unicode: !rest.isEmpty)
    }
}

/// Little-endian reads from an in-memory buffer. Running past the end means the installer is damaged.
struct ByteReader {
    let bytes: [UInt8]
    private(set) var position = 0

    init(_ bytes: [UInt8]) { self.bytes = bytes }

    var isAtEnd: Bool { position == bytes.count }

    mutating func skip(_ count: Int) throws {
        guard count >= 0, count <= bytes.count - position else { throw RuntimeInstallError.corrupt("setup header ends early") }
        position += count
    }

    mutating func take(_ count: Int) throws -> ArraySlice<UInt8> {
        let start = position
        try skip(count)
        return bytes[start..<position]
    }

    mutating func integer<T: FixedWidthInteger>(_: T.Type = T.self) throws -> T {
        let slice = try take(MemoryLayout<T>.size)
        return slice.reversed().reduce(T.zero) { $0 << 8 | T(truncatingIfNeeded: $1) }
    }

    mutating func u8() throws -> UInt8 { try integer() }
    mutating func u32() throws -> UInt32 { try integer() }
    mutating func u64() throws -> UInt64 { try integer() }

    /// A length-prefixed string's raw bytes.
    mutating func binaryString() throws -> ArraySlice<UInt8> {
        try take(Int(u32()))
    }

    mutating func skipStrings(_ count: Int) throws {
        for _ in 0..<count { _ = try binaryString() }
    }

    /// A Delphi set whose members depend on the version: `present` lists, in stored order, whether each possible
    /// member exists in this version. Returns, for the same list, whether each member is set.
    mutating func flags(_ present: [Bool]) throws -> [Bool] {
        let count = present.filter { $0 }.count
        let byteCount = (count + 7) / 8
        let stored = try take(byteCount)
        // Delphi stores a 3-byte set in 4 bytes.
        if byteCount == 3 { try skip(1) }
        var bit = 0
        return present.map { exists in
            guard exists else { return false }
            defer { bit += 1 }
            return stored[stored.startIndex + bit / 8] & (1 << (bit % 8)) != 0
        }
    }
}

/// Where the setup program keeps its headers and embedded data.
struct InnoLoaderOffsets {
    /// File offset of the 64-byte setup data version, followed by the compressed headers.
    let headerOffset: UInt64
    /// File offset of the embedded file data, or 0 when the data lives in separate slice files.
    let dataOffset: UInt64

    /// Inno Setup 5.1.5 and later keep the offset table in an RCDATA resource; earlier versions point to it at 0x30.
    init(file: RandomAccessFile) throws {
        if let offset = try Self.resourceOffset(file: file) ?? Self.legacyOffset(file: file) {
            (headerOffset, dataOffset) = try Self.offsetTable(file: file, at: offset)
        } else {
            throw RuntimeInstallError.unsupportedInstaller("not an Inno Setup installer")
        }
    }

    private static let knownMagic: [[UInt8]] = [
        Array("rDlPtS07".utf8) + [0x87] + Array("eVx".utf8),
        Array("rDlPtS".utf8) + [0xcd, 0xe6, 0xd7] + Array("{".utf8) + [0x0b] + Array("*".utf8),
        Array("nS5W7dT".utf8) + [0x83, 0xaa, 0x1b, 0x0f] + Array("j".utf8),
    ]

    private static func offsetTable(file: RandomAccessFile, at offset: UInt64) throws -> (UInt64, UInt64) {
        var table = try ByteReader(file.read(at: offset, count: 12 + 4 * 8))
        let magic = Array(try table.take(12))
        guard let kind = knownMagic.firstIndex(of: magic) else {
            throw RuntimeInstallError.unsupportedInstaller("unsupported setup loader")
        }
        // Loaders from 5.1.5 on add a revision field.
        if kind > 0 { guard try table.u32() == 1 else { throw RuntimeInstallError.unsupportedInstaller("setup loader revision") } }
        _ = try table.u32()  // unused
        _ = try table.u32()  // offset of the compressed setup program
        _ = try table.u32()  // its uncompressed size
        _ = try table.u32()  // its CRC32
        let headerOffset = try table.u32()
        let dataOffset = try table.u32()
        let checked = table.position
        let expected = try table.u32()
        let actual = table.bytes[0..<checked].withUnsafeBufferPointer { crc32(0, $0.baseAddress, uInt($0.count)) }
        guard UInt32(actual) == expected else { throw RuntimeInstallError.corrupt("setup loader checksum mismatch") }
        return (UInt64(headerOffset), UInt64(dataOffset))
    }

    private static func legacyOffset(file: RandomAccessFile) throws -> UInt64? {
        guard file.size >= 0x3c else { return nil }
        var reader = try ByteReader(file.read(at: 0x30, count: 12))
        guard try reader.u32() == 0x6f6e_6e49 else { return nil }  // "Inno"
        let offset = try reader.u32()
        guard try reader.u32() == ~offset else { return nil }
        return UInt64(offset)
    }

    /// Finds resource RCDATA 11111 in a PE file.
    private static func resourceOffset(file: RandomAccessFile) throws -> UInt64? {
        func u16(_ at: UInt64) throws -> UInt16 { var r = try ByteReader(file.read(at: at, count: 2)); return try r.integer() }
        func u32(_ at: UInt64) throws -> UInt32 { var r = try ByteReader(file.read(at: at, count: 4)); return try r.u32() }
        guard file.size > 0x40, try u16(0) == 0x5a4d else { return nil }  // "MZ"
        let pe = UInt64(try u32(0x3c))
        guard pe + 24 < file.size, try u32(pe) == 0x0000_4550 else { return nil }  // "PE\0\0"
        let sectionCount = Int(try u16(pe + 6))
        let optionalHeader = pe + 24
        let optionalSize = UInt64(try u16(pe + 20))
        let directories: UInt64 = try u16(optionalHeader) == 0x20b ? optionalHeader + 112 : optionalHeader + 96
        guard try u32(directories - 4) > 2 else { return nil }
        let resourceAddress = try u32(directories + 16)
        guard resourceAddress != 0 else { return nil }

        var sections: [(virtualAddress: UInt32, virtualSize: UInt32, raw: UInt32)] = []
        for index in 0..<sectionCount {
            var entry = try ByteReader(file.read(at: optionalHeader + optionalSize + UInt64(index) * 40, count: 40))
            try entry.skip(8)
            let virtualSize = try entry.u32(), virtualAddress = try entry.u32()
            try entry.skip(4)
            sections.append((virtualAddress, virtualSize, try entry.u32()))
        }
        func fileOffset(_ address: UInt32) -> UInt64? {
            sections.first { address >= $0.virtualAddress && address - $0.virtualAddress < $0.virtualSize }
                .map { UInt64(address - $0.virtualAddress) + UInt64($0.raw) }
        }
        guard let root = fileOffset(resourceAddress) else { return nil }

        // Each directory level returns an entry whose high bit marks another directory. nil id takes the first entry.
        func entry(in directory: UInt64, id: UInt32?) throws -> UInt32? {
            let named = UInt64(try u16(directory + 12)), ids = UInt64(try u16(directory + 14))
            for index in 0..<(named + ids) {
                let position = directory + 16 + index * 8
                if let id {
                    guard index >= named, try u32(position) == id else { continue }
                }
                return try u32(position + 4)
            }
            return nil
        }
        guard let type = try entry(in: root, id: 10), type & 0x8000_0000 != 0,
              let name = try entry(in: root + UInt64(type & 0x7fff_ffff), id: 11111), name & 0x8000_0000 != 0,
              let leaf = try entry(in: root + UInt64(name & 0x7fff_ffff), id: nil), leaf & 0x8000_0000 == 0
        else { return nil }
        return fileOffset(try u32(root + UInt64(leaf)))
    }
}

/// Reads one of the setup headers' compressed blocks: a CRC32-checked size and compression flag, then the stored
/// bytes in 4096-byte pieces that each carry their own CRC32. Inno Setup 5 compresses headers with LZMA1.
func readHeaderBlock(file: RandomAccessFile, at offset: UInt64) throws -> (data: [UInt8], end: UInt64) {
    var info = try ByteReader(file.read(at: offset, count: 9))
    let expected = try info.u32()
    let actual = info.bytes[4..<9].withUnsafeBufferPointer { crc32(0, $0.baseAddress, uInt($0.count)) }
    guard UInt32(actual) == expected else { throw RuntimeInstallError.corrupt("setup header block checksum mismatch") }
    let storedSize = Int(try info.u32())
    let compressed = try info.u8() != 0
    guard storedSize <= 64 << 20 else { throw RuntimeInstallError.corrupt("setup header block too large") }
    let stored = try file.read(at: offset + 9, count: storedSize)

    var payload: [UInt8] = []
    payload.reserveCapacity(storedSize)
    var position = 0
    while position < stored.count {
        guard stored.count - position > 4 else { throw RuntimeInstallError.corrupt("setup header block ends early") }
        let length = min(4096, stored.count - position - 4)
        let piece = stored[(position + 4)..<(position + 4 + length)]
        let crc = stored[position..<(position + 4)].reversed().reduce(UInt32(0)) { $0 << 8 | UInt32($1) }
        guard piece.withUnsafeBufferPointer({ UInt32(crc32(0, $0.baseAddress, uInt($0.count))) }) == crc else {
            throw RuntimeInstallError.corrupt("setup header CRC32 mismatch")
        }
        payload += piece
        position += 4 + length
    }
    let end = offset + 9 + UInt64(storedSize)
    guard compressed else { return (payload, end) }

    let decoder = LZMA1Decoder()
    var output: [UInt8] = []
    var buffer = [UInt8](repeating: 0, count: 1 << 16)
    var consumed = 0
    while true {
        let (used, produced, ended) = try payload.withUnsafeBytes { input in
            try buffer.withUnsafeMutableBytes { try decoder.decode(UnsafeRawBufferPointer(rebasing: input[consumed...]), into: $0) }
        }
        consumed += used
        output += buffer[0..<produced]
        guard output.count <= 256 << 20 else { throw RuntimeInstallError.corrupt("setup header too large") }
        if ended || (used == 0 && produced == 0) { break }
    }
    return (output, end)
}

/// The parts of an Inno Setup installer's headers that extraction needs.
struct InnoHeaders {
    struct Directory {
        let name: String
        let deleteAfterInstall: Bool
    }

    struct File {
        let destination: String
        /// Index into `dataEntries`, or out of range for files the installer copies from elsewhere.
        let location: Int
        let deleteAfterInstall: Bool
        let isUninstaller: Bool
    }

    enum Checksum: Equatable {
        case md5([UInt8]), sha1([UInt8])
    }

    enum Compression: Comparable {
        case stored, zlib, bzip2, lzma1, lzma2
    }

    /// x86 CALL and JMP address transforms Inno Setup applies to executables.
    enum ExecutableFilter {
        case noFilter, v4108, v5200(flipHighByte: Bool)
    }

    struct DataEntry {
        let firstSlice: Int
        let chunkOffset: UInt64
        let chunkSize: UInt64
        let fileOffset: UInt64
        let fileSize: UInt64
        let checksum: Checksum
        let compression: Compression
        let encrypted: Bool
        let filter: ExecutableFilter
    }

    let version: InnoVersion
    let offsets: InnoLoaderOffsets
    let baseFilename: String
    let slicesPerDisk: Int
    let licenseText: String?
    let directories: [Directory]
    let files: [File]
    let dataEntries: [DataEntry]

    init(file: RandomAccessFile) throws {
        offsets = try InnoLoaderOffsets(file: file)
        version = try InnoVersion(stored: file.read(at: offsets.headerOffset, count: 64))
        guard version >= InnoVersion(5, 0, 0), version < InnoVersion(6, 0, 0) else {
            throw RuntimeInstallError.unsupportedInstaller("Inno Setup \(version) is not supported")
        }
        let (primary, primaryEnd) = try readHeaderBlock(file: file, at: offsets.headerOffset + 64)
        var reader = ByteReader(primary)
        let v = version

        // The setup header.
        try reader.skipStrings(6)  // app name, versioned name, id, copyright, publisher, publisher URL
        if v >= InnoVersion(5, 1, 13) { try reader.skipStrings(1) }  // support phone
        try reader.skipStrings(5)  // support URL, updates URL, version, default dir name, default group name
        let baseFilename = try reader.binaryString()
        var license: ArraySlice<UInt8> = []
        if v < InnoVersion(5, 2, 5) {
            license = try reader.binaryString()
            try reader.skipStrings(2)  // info before, info after
        }
        try reader.skipStrings(1 + 2 + 1 + 2 + 1)  // uninstall files dir, name, icon, mutex, user name, organisation, serial
        if v < InnoVersion(5, 2, 5) { try reader.skipStrings(1) }  // compiled code
        try reader.skipStrings(4)  // readme file, contact, comments, modify path
        if v >= InnoVersion(5, 3, 8) { try reader.skipStrings(1) }  // create uninstall registry key
        if v >= InnoVersion(5, 3, 10) { try reader.skipStrings(1) }  // uninstallable
        if v >= InnoVersion(5, 5, 0) { try reader.skipStrings(1) }  // close applications filter
        if v >= InnoVersion(5, 5, 6) { try reader.skipStrings(1) }  // setup mutex
        if v >= InnoVersion(5, 6, 1) { try reader.skipStrings(2) }  // changes environment, changes associations
        if v >= InnoVersion(5, 2, 5) {
            license = try reader.binaryString()
            try reader.skipStrings(2)
        }
        if v >= InnoVersion(5, 2, 1), v < InnoVersion(5, 3, 10) { try reader.skipStrings(1) }  // uninstaller signature
        if v >= InnoVersion(5, 2, 5) { try reader.skipStrings(1) }  // compiled code
        if !v.isUnicode { try reader.skip(32) }  // lead bytes
        let languageCount = Int(try reader.u32())
        let messageCount = Int(try reader.u32())
        let permissionCount = Int(try reader.u32())
        let typeCount = Int(try reader.u32())
        let componentCount = Int(try reader.u32())
        let taskCount = Int(try reader.u32())
        let directoryCount = Int(try reader.u32())
        let fileCount = Int(try reader.u32())
        let dataEntryCount = Int(try reader.u32())
        try reader.skip(7 * 4)  // icon, ini, registry, delete, uninstall delete, run, uninstall run counts
        try reader.skip(20)  // minimum and only-below Windows versions
        try reader.skip(8)  // back colors
        if v < InnoVersion(5, 5, 7) { try reader.skip(4) }  // image back color
        if v < InnoVersion(5, 0, 4) { try reader.skip(4) }  // small image back color
        if v >= InnoVersion(5, 5, 7) { try reader.skip(1) }  // image alpha format
        try reader.skip(v >= InnoVersion(5, 3, 9) ? 20 : 16)  // password hash
        try reader.skip(8)  // password salt
        try reader.skip(8)  // extra disk space
        let slicesPerDisk = Int(try reader.u32())
        try reader.skip(1 + 1 + 1 + 1 + 1)  // uninstall log mode, dir exists warning, privileges, language dialog, detection
        let compression: Compression
        switch try reader.u8() {
        case 0: compression = .stored
        case 1: compression = .zlib
        case 2: compression = .bzip2
        case 3: compression = .lzma1
        case 4 where v >= InnoVersion(5, 3, 9): compression = .lzma2
        case let value: throw RuntimeInstallError.corrupt("unknown compression method \(value)")
        }
        if v >= InnoVersion(5, 1, 0) { try reader.skip(2) }  // architectures allowed, installed in 64-bit mode
        if v >= InnoVersion(5, 2, 1), v < InnoVersion(5, 3, 10) { try reader.skip(8) }  // signed uninstaller sizes
        if v >= InnoVersion(5, 3, 3) { try reader.skip(2) }  // disable dir page, disable program group page
        if v >= InnoVersion(5, 5, 0) { try reader.skip(8) } else if v >= InnoVersion(5, 3, 6) { try reader.skip(4) }
        let options = try reader.flags([
            true, v < InnoVersion(5, 3, 10), true, v < InnoVersion(5, 3, 3), v < InnoVersion(5, 3, 3),
            true, true, true, true, true, true, true, true, true, true, true,
            v < InnoVersion(5, 6, 1), v < InnoVersion(5, 3, 8),
            true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true,
            true, true, true, true, true, true, true,
            true,  // EncryptionUsed, index 41
            v >= InnoVersion(5, 0, 4) && v < InnoVersion(5, 6, 1), v >= InnoVersion(5, 1, 7) && !v.isUnicode,
            v >= InnoVersion(5, 1, 13), v >= InnoVersion(5, 2, 1), v >= InnoVersion(5, 3, 8), v >= InnoVersion(5, 3, 9),
            v >= InnoVersion(5, 5, 0), v >= InnoVersion(5, 5, 0), v >= InnoVersion(5, 5, 0), v >= InnoVersion(5, 5, 7),
        ])
        if options[41] {
            throw RuntimeInstallError.unsupportedInstaller("encrypted installers are not supported")
        }

        // Languages. Non-unicode installers store text in the codepage of their languages.
        var codepages: [UInt32] = []
        var languageLicenses: [ArraySlice<UInt8>] = []
        for _ in 0..<languageCount {
            try reader.skipStrings(7)  // name, language name, four fonts, data
            languageLicenses.append(try reader.binaryString())
            try reader.skipStrings(2)  // info before, info after
            try reader.skip(4)  // language id
            if !v.isUnicode {
                let codepage = try reader.u32()
                codepages.append(codepage == 0 ? 1252 : codepage)
            } else if v < InnoVersion(5, 3, 0) {
                try reader.skip(4)
            }
            try reader.skip(4 * 4)  // font sizes
            if v >= InnoVersion(5, 2, 3) { try reader.skip(1) }  // right to left
        }
        let text = InnoText(
            unicode: v.isUnicode, codepage: codepages.contains(1252) ? 1252 : codepages.first ?? 1252)

        for _ in 0..<messageCount {
            try reader.skipStrings(2)
            try reader.skip(4)
        }
        try reader.skipStrings(permissionCount)
        for _ in 0..<typeCount {
            try reader.skipStrings(4)  // name, description, languages, check
            try reader.skip(20 + 1 + 1 + 8)  // Windows versions, flags, setup type, size
        }
        for _ in 0..<componentCount {
            try reader.skipStrings(5)  // name, description, types, languages, check
            try reader.skip(8 + 4 + 1 + 20 + 1 + 8)  // disk space, level, used, Windows versions, flags, size
        }
        for _ in 0..<taskCount {
            try reader.skipStrings(6)  // name, description, group description, components, languages, check
            try reader.skip(4 + 1 + 20 + 1)  // level, used, Windows versions, flags
        }

        var directories: [Directory] = []
        for _ in 0..<directoryCount {
            let name = text.decode(try reader.binaryString())
            try reader.skipStrings(6)  // components, tasks, languages, check, after install, before install
            try reader.skip(4 + 20 + 2)  // attributes, Windows versions, permission
            let flags = try reader.flags([true, true, true, v >= InnoVersion(5, 2, 0), v >= InnoVersion(5, 2, 0)])
            directories.append(Directory(name: name, deleteAfterInstall: flags[1]))
        }

        var files: [File] = []
        for _ in 0..<fileCount {
            try reader.skipStrings(1)  // source
            let destination = text.decode(try reader.binaryString())
            try reader.skipStrings(v >= InnoVersion(5, 2, 5) ? 2 : 1)  // font name, strong assembly name
            try reader.skipStrings(6)  // components, tasks, languages, check, after install, before install
            try reader.skip(20)  // Windows versions
            let location = Int(try reader.u32())
            try reader.skip(4 + 8 + 2)  // attributes, external size, permission
            let flags = try reader.flags(
                Array(repeating: true, count: 24)
                    + [v >= InnoVersion(5, 0, 3), v >= InnoVersion(5, 1, 0), v >= InnoVersion(5, 1, 2), v >= InnoVersion(5, 1, 2)]
                    + [v >= InnoVersion(5, 2, 0), v >= InnoVersion(5, 2, 0), v >= InnoVersion(5, 2, 0)]
                    + [v >= InnoVersion(5, 2, 5)])
            let type = try reader.u8()
            files.append(File(destination: destination, location: location, deleteAfterInstall: flags[3],
                              isUninstaller: type == 1))
        }
        // Icons, ini, registry, delete and run entries, then the wizard images, follow. Extraction needs none of them,
        // and the block's stored size says where the next block starts.

        var dataReader = ByteReader(try readHeaderBlock(file: file, at: primaryEnd).data)
        var dataEntries: [DataEntry] = []
        for _ in 0..<dataEntryCount {
            let firstSlice = Int(try dataReader.u32())
            _ = try dataReader.u32()  // last slice
            let chunkOffset = UInt64(try dataReader.u32())
            let fileOffset = try dataReader.u64()
            let fileSize = try dataReader.u64()
            let chunkSize = try dataReader.u64()
            let checksum: Checksum = v >= InnoVersion(5, 3, 9)
                ? .sha1(Array(try dataReader.take(20))) : .md5(Array(try dataReader.take(16)))
            try dataReader.skip(8 + 8)  // timestamp, file version
            let flags = try dataReader.flags(
                Array(repeating: true, count: 8)
                    + [v >= InnoVersion(5, 1, 13), v >= InnoVersion(5, 5, 7), v >= InnoVersion(5, 5, 7)])
            let filter: ExecutableFilter = !flags[4] ? .noFilter
                : v < InnoVersion(5, 2, 0) ? .v4108 : .v5200(flipHighByte: v >= InnoVersion(5, 3, 9))
            dataEntries.append(DataEntry(
                firstSlice: firstSlice, chunkOffset: chunkOffset, chunkSize: chunkSize, fileOffset: fileOffset,
                fileSize: fileSize, checksum: checksum, compression: flags[7] ? compression : .stored,
                encrypted: flags[6], filter: filter))
        }
        guard dataReader.isAtEnd else { throw RuntimeInstallError.corrupt("unexpected data after the data entries") }

        self.baseFilename = text.decode(baseFilename)
        self.slicesPerDisk = slicesPerDisk
        self.directories = directories
        self.files = files
        self.dataEntries = dataEntries
        let licenseBytes = license.isEmpty ? languageLicenses.first { !$0.isEmpty } ?? [] : license
        self.licenseText = licenseBytes.isEmpty ? nil : InnoText.decodeDocument(licenseBytes, codepage: text.codepage)
    }
}

/// Decodes the installer's strings: UTF-16LE in unicode installers, a Windows codepage otherwise.
struct InnoText {
    let unicode: Bool
    let codepage: UInt32

    func decode(_ bytes: ArraySlice<UInt8>) -> String {
        unicode ? String(data: Data(bytes), encoding: .utf16LittleEndian) ?? "" : Self.decode(bytes, codepage: codepage)
    }

    static func decode(_ bytes: ArraySlice<UInt8>, codepage: UInt32) -> String {
        let encoding = CFStringConvertEncodingToNSStringEncoding(CFStringConvertWindowsCodepageToEncoding(codepage))
        return String(data: Data(bytes), encoding: String.Encoding(rawValue: encoding))
            ?? String(decoding: bytes, as: UTF8.self)
    }

    /// License files are stored as the raw bytes of the file the author named, whatever its encoding.
    static func decodeDocument(_ bytes: ArraySlice<UInt8>, codepage: UInt32) -> String {
        if bytes.starts(with: [0xef, 0xbb, 0xbf]) { return String(decoding: bytes.dropFirst(3), as: UTF8.self) }
        if let text = String(data: Data(bytes), encoding: .utf8) { return text }
        return decode(bytes, codepage: codepage)
    }
}
