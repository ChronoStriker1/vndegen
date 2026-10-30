import Foundation

/// The parts of a Windows executable VNDegen reads: its sections and its PE import table, read without the rest of the
/// file.
nonisolated struct WindowsExecutable {
    struct Section {
        var name: String
        /// Where the section's addresses start once loaded, and how many it spans.
        var address: UInt64, size: UInt64
        /// Where its bytes are in the file, and how many.
        var fileOffset: UInt64, fileSize: UInt64
    }

    private let file: FileHandle
    private let pe: UInt64
    private let optionalHeaderSize: UInt64
    let sections: [Section]

    /// Nil when `url` is not a Windows executable.
    init?(_ url: URL) {
        guard let file = try? FileHandle(forReadingFrom: url) else { return nil }
        self.file = file
        guard Self.u16(file, 0) == 0x5A4D, let pe = Self.u32(file, 0x3C), Self.u32(file, pe) == 0x4550,
              let count = Self.u16(file, pe + 6), let optionalHeaderSize = Self.u16(file, pe + 20) else { return nil }
        self.pe = pe
        self.optionalHeaderSize = optionalHeaderSize
        var sections: [Section] = []
        for index in 0..<min(count, 96) {
            let header = pe + 24 + optionalHeaderSize + index * 40
            guard let name = Self.read(file, header, 8), let virtualSize = Self.u32(file, header + 8),
                  let address = Self.u32(file, header + 12), let rawSize = Self.u32(file, header + 16),
                  let raw = Self.u32(file, header + 20) else { return nil }
            // A section's addresses span its virtual size. Godot's "pck" section has a tiny one and a huge raw size.
            sections.append(Section(name: String(decoding: name.prefix { $0 != 0 }, as: UTF8.self), address: address,
                                    size: virtualSize != 0 ? virtualSize : rawSize, fileOffset: raw, fileSize: rawSize))
        }
        self.sections = sections
    }

    /// The bytes the file holds for the section named `name`.
    func contents(of name: String) -> Data? {
        sections.first { $0.name == name }.flatMap { Self.read(file, $0.fileOffset, Int($0.fileSize)) }
    }

    /// The names of the DLLs the executable imports, lowercased.
    var importedLibraries: [String] {
        guard let magic = Self.u16(file, pe + 24) else { return [] }
        // The data directories follow the optional header's fixed part, which is longer in 64-bit executables. The
        // import table is the second directory.
        let directories = pe + 24 + (magic == 0x20B ? 112 : 96)
        guard directories + 16 <= pe + 24 + optionalHeaderSize, let importTable = Self.u32(file, directories + 8),
              importTable != 0, var descriptor = fileOffset(importTable) else { return [] }
        var names: [String] = []
        // Each descriptor is 20 bytes, with the library's name at 12. A zero descriptor ends the table.
        for _ in 0..<512 {
            guard let nameRVA = Self.u32(file, descriptor + 12), nameRVA != 0, let name = fileOffset(nameRVA),
                  let bytes = Self.read(file, name, 256) ?? Self.read(file, name, 64) else { break }
            names.append(String(decoding: bytes.prefix { $0 != 0 }, as: UTF8.self).lowercased())
            descriptor += 20
        }
        return names
    }

    private func fileOffset(_ address: UInt64) -> UInt64? {
        sections.first { address >= $0.address && address < $0.address + $0.size }.map { $0.fileOffset + address - $0.address }
    }

    private static func read(_ file: FileHandle, _ offset: UInt64, _ count: Int) -> Data? {
        guard (try? file.seek(toOffset: offset)) != nil, let data = try? file.read(upToCount: count), data.count == count else { return nil }
        return data
    }

    private static func u16(_ file: FileHandle, _ offset: UInt64) -> UInt64? {
        read(file, offset, 2).map { UInt64($0[$0.startIndex]) | UInt64($0[$0.startIndex + 1]) << 8 }
    }

    private static func u32(_ file: FileHandle, _ offset: UInt64) -> UInt64? {
        read(file, offset, 4).map { data in (0..<4).reduce(0) { $0 | UInt64(data[data.startIndex + $1]) << (8 * $1) } }
    }
}
