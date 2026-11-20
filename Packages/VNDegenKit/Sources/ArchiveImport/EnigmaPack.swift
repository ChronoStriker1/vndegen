import Foundation

nonisolated public struct EnigmaPack: Sendable, Equatable {
    public enum Failure: Error, Equatable, Sendable {
        case notAPack
        case compressed
        case unsupported(String)
        case damaged(String)
    }

    public struct File: Sendable, Equatable {
        public let path: String
        public let size: Int64
        let stored: Int64
        let offset: Int64

        var isCompressed: Bool { stored != size }
    }

    public let files: [File]
    public let directories: [String]
    public let title: String?

    public var byteCount: Int64 { files.reduce(0) { $0 + $1.size } }

    static let magic: [UInt8] = Array("EVB\0".utf8)
    static let headerSize: UInt32 = 64
    private static let maxNodes = 1_000_000
    private static let maxDepth = 256
    private static let maxTable = 256 << 20
    private static let maxRecord = 1 << 20

    public static func isPack(_ url: URL) -> Bool {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return false }
        defer { try? handle.close() }
        return (try? locate(Reader(handle))) != nil
    }

    public init(_ url: URL) throws(Failure) {
        let handle: FileHandle
        do {
            handle = try FileHandle(forReadingFrom: url)
        } catch {
            throw .damaged(error.localizedDescription)
        }
        defer { try? handle.close() }
        let reader = Reader(handle)
        let start = try Self.locate(reader)
        let (files, directories) = try Self.readTree(reader, at: start)
        self.files = files
        self.directories = directories
        title = Self.gameTitle(files, reader: reader)
    }

    private init(files: [File], directories: [String], title: String?) {
        self.files = files
        self.directories = directories
        self.title = title
    }

    public func keeping(_ keep: (File) -> Bool) -> EnigmaPack {
        let kept = files.filter(keep)
        let holdingFiles = Set(files.flatMap { Self.folders(above: $0.path) })
        let holdingKept = Set(kept.flatMap { Self.folders(above: $0.path) })
        return EnigmaPack(files: kept, directories: directories.filter { !holdingFiles.contains($0) || holdingKept.contains($0) },
                          title: title)
    }

    public func copy(_ file: File, from handle: FileHandle, _ body: (UnsafeRawBufferPointer) throws -> Void) throws {
        guard !file.isCompressed else { throw Failure.compressed }
        try handle.seek(toOffset: UInt64(file.offset))
        var left = file.size
        while left > 0 {
            let chunk = try handle.read(upToCount: Int(min(left, 1 << 20))) ?? Data()
            guard !chunk.isEmpty else { throw Failure.damaged("the .exe ends inside \(file.path)") }
            try chunk.withUnsafeBytes(body)
            left -= Int64(chunk.count)
        }
    }

    private static func folders(above path: String) -> [String] {
        let parts = path.split(separator: "/").dropLast()
        return parts.indices.map { parts[...$0].joined(separator: "/") }
    }

    private final class Reader {
        let handle: FileHandle
        let size: Int64

        init(_ handle: FileHandle) {
            self.handle = handle
            size = Int64((try? handle.seekToEnd()) ?? 0)
        }

        func bytes(at offset: Int64, count: Int) throws(Failure) -> [UInt8] {
            guard offset >= 0, count >= 0, offset <= size, Int64(count) <= size - offset else {
                throw .damaged("a record points past the end of the .exe")
            }
            do {
                try handle.seek(toOffset: UInt64(offset))
                let data = try handle.read(upToCount: count) ?? Data()
                guard data.count == count else { throw Failure.damaged("short read") }
                return [UInt8](data)
            } catch let error as Failure {
                throw error
            } catch {
                throw .damaged(error.localizedDescription)
            }
        }
    }

    private static func locate(_ reader: Reader) throws(Failure) -> Int64 {
        guard let dos = try? reader.bytes(at: 0, count: 64), dos[0] == 0x4d, dos[1] == 0x5a else { throw .notAPack }
        let pe = Int64(dos.u32(0x3c))
        guard let header = try? reader.bytes(at: pe, count: 24), header[0..<4].elementsEqual(Array("PE\0\0".utf8)) else {
            throw .notAPack
        }
        let sectionCount = Int(header.u16(6))
        let optionalSize = Int64(header.u16(20))
        guard sectionCount <= 96, let table = try? reader.bytes(at: pe + 24 + optionalSize, count: sectionCount * 40) else {
            throw .notAPack
        }
        for index in 0..<sectionCount {
            let entry = Array(table[(index * 40)..<(index * 40 + 40)])
            guard entry[0..<8].elementsEqual(Array(".enigma1".utf8)) else { continue }
            let raw = Int64(entry.u32(20))
            let window = (try? reader.bytes(at: raw, count: Int(min(4096, max(0, reader.size - raw))))) ?? []
            var offset = 0
            while offset + 8 <= window.count {
                if window[offset..<(offset + 4)].elementsEqual(magic), window.u32(offset + 4) == headerSize {
                    return raw + Int64(offset)
                }
                offset += 1
            }
        }
        throw .notAPack
    }

    private struct Record {
        let name: String
        let type: UInt8
        let count: Int
        let size: Int64
        let stored: Int64
        let next: Int
    }

    private static func readTree(_ reader: Reader, at start: Int64) throws(Failure) -> ([File], [String]) {
        let header = try reader.bytes(at: start, count: Int(headerSize))
        let layout = header.u32(20)
        let total = header.u64(48)
        guard layout == 1 || layout == 3 else { throw .unsupported("layout \(layout)") }
        let legacy = layout == 1
        let mainStart = start + Int64(headerSize)
        let main = try reader.bytes(at: mainStart, count: 16)
        let mainSize = Int64(main.u32(0))
        let rootCount = Int(main[12]) | Int(main[13]) << 8 | Int(main[14]) << 16

        var files: [File] = []
        var directories: [String] = []
        var stack: [(prefix: [String]?, remaining: Int)] = [([], rootCount)]
        var nodes = 0
        var position: Int64
        var data: Int64
        var table: [UInt8] = []
        var cursor = 0
        if legacy {
            position = mainStart + mainSize + 4
            data = position
        } else {
            guard mainSize >= 11, mainSize <= maxTable else { throw .damaged("file table size \(mainSize)") }
            table = try reader.bytes(at: mainStart, count: Int(mainSize + 4))
            cursor = 15
            position = 0
            data = mainStart + 4 + mainSize
        }

        while !stack.isEmpty {
            guard stack[stack.count - 1].remaining > 0 else {
                stack.removeLast()
                continue
            }
            stack[stack.count - 1].remaining -= 1
            nodes += 1
            guard nodes <= maxNodes else { throw .damaged("too many entries") }

            let record: Record
            let fileOffset: Int64
            if legacy {
                let head = try reader.bytes(at: position, count: 4)
                let size = Int(head.u32(0)) + 4
                guard size <= maxRecord else { throw .damaged("record size \(size)") }
                let bytes = try reader.bytes(at: position, count: size)
                record = try parse(bytes, at: 0, legacy: true)
                fileOffset = position + Int64(size)
                position = fileOffset + (record.type == 2 ? record.stored : 0)
            } else {
                record = try parse(table, at: cursor, legacy: false)
                cursor = record.next
                fileOffset = data
                if record.type == 2 { data += record.stored }
            }

            let parent = stack[stack.count - 1].prefix
            let isRoot = stack.count == 1
            switch record.type {
            case 3:
                guard stack.count < maxDepth else { throw .damaged("folders nest too deep") }
                var prefix: [String]?
                if isRoot {
                    prefix = record.name == "%DEFAULT FOLDER%" ? [] : nil
                } else {
                    prefix = parent.map { $0 + ArchiveReader.pathComponents(record.name) }
                }
                if let prefix, !prefix.isEmpty { directories.append(prefix.joined(separator: "/")) }
                stack.append((prefix, record.count))
            case 2:
                guard record.stored >= 0, record.size >= 0, fileOffset + record.stored <= reader.size else {
                    throw .damaged("\(record.name) lies past the end of the .exe")
                }
                guard !isRoot, let parent else { continue }
                let path = parent + ArchiveReader.pathComponents(record.name)
                guard path.count > parent.count else { continue }
                files.append(File(path: path.joined(separator: "/"), size: record.size, stored: record.stored, offset: fileOffset))
            default:
                throw .unsupported("entry type \(record.type)")
            }
        }

        let end = legacy ? position : data
        guard end - start == Int64(clamping: total) else { throw .unsupported("the file table doesn't add up to the pack's size") }
        return (files, directories)
    }

    private static func parse(_ bytes: [UInt8], at start: Int, legacy: Bool) throws(Failure) -> Record {
        guard start + 16 <= bytes.count else { throw .damaged("the file table ends early") }
        let count = Int(bytes.u32(start + 12))
        var end = start + 16
        var units: [UInt16] = []
        while true {
            guard end + 2 <= bytes.count else { throw .damaged("a name runs past the file table") }
            let unit = bytes.u16(end)
            end += 2
            if unit == 0 { break }
            units.append(unit)
        }
        guard end < bytes.count else { throw .damaged("the file table ends early") }
        let type = bytes[end]
        end += 1
        let name = String(decoding: units, as: UTF16.self)
        switch (type, legacy) {
        case (3, false):
            guard end + 25 <= bytes.count else { throw .damaged("the file table ends early") }
            return Record(name: name, type: type, count: count, size: 0, stored: 0, next: end + 25)
        case (2, false):
            guard end + 53 <= bytes.count else { throw .damaged("the file table ends early") }
            return Record(name: name, type: type, count: 0, size: Int64(bytes.u32(end + 2)), stored: Int64(bytes.u32(end + 49)),
                          next: end + 53)
        case (2, true):
            let payload = bytes.count - 49
            guard payload >= end else { throw .damaged("a record is too short") }
            return Record(name: name, type: type, count: 0, size: Int64(bytes.u32(payload + 2)),
                          stored: Int64(bytes.u32(payload + 41)), next: bytes.count)
        default:
            return Record(name: name, type: type, count: count, size: 0, stored: 0, next: bytes.count)
        }
    }

    private static func gameTitle(_ files: [File], reader: Reader) -> String? {
        guard let system = files.first(where: {
            ["www/data/system.json", "data/system.json"].contains($0.path.lowercased()) && !$0.isCompressed && $0.size <= 1 << 20
        }), var bytes = try? reader.bytes(at: system.offset, count: Int(system.size)) else { return nil }
        if bytes.starts(with: [0xef, 0xbb, 0xbf]) { bytes.removeFirst(3) }
        let json = try? JSONSerialization.jsonObject(with: Data(bytes)) as? [String: Any]
        let title = (json?["gameTitle"] as? String)?.trimmingCharacters(in: .whitespacesAndNewlines)
        return title?.isEmpty == false ? title : nil
    }
}

nonisolated extension Array where Element == UInt8 {
    func u16(_ offset: Int) -> UInt16 { UInt16(self[offset]) | UInt16(self[offset + 1]) << 8 }
    func u32(_ offset: Int) -> UInt32 { (0..<4).reduce(0) { $0 | UInt32(self[offset + $1]) << (8 * $1) } }
    func u64(_ offset: Int) -> UInt64 { (0..<8).reduce(0) { $0 | UInt64(self[offset + $1]) << (8 * $1) } }
}
