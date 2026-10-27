import Foundation

/// What a Windows executable imports, read from its PE import table, without reading the rest of the file.
nonisolated enum WindowsExecutable {
    /// The names of the DLLs `url` imports, lowercased. Empty when it is not a Windows executable.
    static func importedLibraries(_ url: URL) -> [String] {
        guard let file = try? FileHandle(forReadingFrom: url) else { return [] }
        defer { try? file.close() }
        func read(_ offset: UInt64, _ count: Int) -> Data? {
            guard (try? file.seek(toOffset: offset)) != nil, let data = try? file.read(upToCount: count), data.count == count else { return nil }
            return data
        }
        func u16(_ offset: UInt64) -> UInt64? { read(offset, 2).map { UInt64($0[$0.startIndex]) | UInt64($0[$0.startIndex + 1]) << 8 } }
        func u32(_ offset: UInt64) -> UInt64? { read(offset, 4).map { data in (0..<4).reduce(0) { $0 | UInt64(data[data.startIndex + $1]) << (8 * $1) } } }

        guard u16(0) == 0x5A4D, let pe = u32(0x3C), u32(pe) == 0x4550, let sectionCount = u16(pe + 6),
              let optionalSize = u16(pe + 20), let magic = u16(pe + 24) else { return [] }
        // The data directories follow the optional header's fixed part, which is longer in 64-bit executables. The
        // import table is the second directory.
        let directories = pe + 24 + (magic == 0x20B ? 112 : 96)
        guard let importTable = u32(directories + 8), importTable != 0 else { return [] }

        struct Section { var address: UInt64, size: UInt64, raw: UInt64 }
        var sections: [Section] = []
        for index in 0..<min(sectionCount, 96) {
            let header = pe + 24 + optionalSize + index * 40
            guard let virtualSize = u32(header + 8), let address = u32(header + 12), let rawSize = u32(header + 16),
                  let raw = u32(header + 20) else { return [] }
            // A section's addresses span its virtual size. Godot's "pck" section has a tiny one and a huge raw size.
            sections.append(Section(address: address, size: virtualSize != 0 ? virtualSize : rawSize, raw: raw))
        }
        func fileOffset(_ rva: UInt64) -> UInt64? {
            sections.first { rva >= $0.address && rva < $0.address + $0.size }.map { $0.raw + rva - $0.address }
        }

        var names: [String] = []
        guard var descriptor = fileOffset(importTable) else { return [] }
        // Each descriptor is 20 bytes, with the library's name at 12. A zero descriptor ends the table.
        for _ in 0..<512 {
            guard let nameRVA = u32(descriptor + 12), nameRVA != 0, let name = fileOffset(nameRVA),
                  let bytes = read(name, 256) ?? read(name, 64) else { break }
            names.append(String(decoding: bytes.prefix { $0 != 0 }, as: UTF8.self).lowercased())
            descriptor += 20
        }
        return names
    }
}
