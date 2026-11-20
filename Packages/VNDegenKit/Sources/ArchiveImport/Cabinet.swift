import Foundation

nonisolated public struct Cabinet: Sendable, Equatable {
    public struct File: Sendable, Equatable {
        public let path: String
        public let size: Int64
        let folder: Int
        let offset: Int64
    }

    struct Folder: Sendable, Equatable {
        let dataOffset: Int64
        let blockCount: Int
        let compression: UInt16
    }

    public let files: [File]
    let folders: [Folder]
    let start: Int64
    let dataReserve: Int

    public var byteCount: Int64 { files.reduce(0) { $0 + $1.size } }

    public var directories: [String] {
        var seen = Set<String>()
        var result: [String] = []
        for file in files {
            var parts = file.path.split(separator: "/").dropLast()
            var chain: [String] = []
            while !parts.isEmpty {
                chain.append(parts.joined(separator: "/"))
                parts = parts.dropLast()
            }
            for folder in chain.reversed() where seen.insert(folder).inserted { result.append(folder) }
        }
        return result
    }

    init(_ url: URL, at start: Int64) throws(EnigmaPack.Failure) {
        let handle: FileHandle
        do {
            handle = try FileHandle(forReadingFrom: url)
        } catch {
            throw .damaged(error.localizedDescription)
        }
        defer { try? handle.close() }
        let size = Int64((try? handle.seekToEnd()) ?? 0)
        func bytes(_ offset: Int64, _ count: Int) throws(EnigmaPack.Failure) -> [UInt8] {
            guard offset >= 0, count >= 0, offset <= size, Int64(count) <= size - offset,
                  (try? handle.seek(toOffset: UInt64(offset))) != nil,
                  let data = try? handle.read(upToCount: count), data.count == count else {
                throw .damaged("the cabinet is cut short")
            }
            return [UInt8](data)
        }

        let header = try bytes(start, 36)
        guard header.starts(with: Array("MSCF".utf8)) else { throw .notAPack }
        let flags = header.u16(30)
        guard flags & 3 == 0 else { throw .unsupported("a cabinet split across several files") }
        var cursor = start + 36
        var folderReserve = 0, dataReserve = 0
        if flags & 4 != 0 {
            let reserve = try bytes(cursor, 4)
            folderReserve = Int(reserve[2])
            dataReserve = Int(reserve[3])
            cursor += 4 + Int64(reserve.u16(0))
        }
        let folderCount = Int(header.u16(26)), fileCount = Int(header.u16(28))
        var folders: [Folder] = []
        for _ in 0..<folderCount {
            let record = try bytes(cursor, 8)
            folders.append(Folder(dataOffset: start + Int64(record.u32(0)), blockCount: Int(record.u16(4)), compression: record.u16(6)))
            cursor += 8 + Int64(folderReserve)
        }
        cursor = start + Int64(header.u32(16))
        var files: [File] = []
        for _ in 0..<fileCount {
            let record = try bytes(cursor, 16)
            let folder = Int(record.u16(8))
            guard folder < folders.count else { throw .unsupported("a file continued from another cabinet") }
            var name: [UInt8] = []
            var at = cursor + 16
            while true {
                let chunk = try bytes(at, min(256, Int(size - at)))
                if let end = chunk.firstIndex(of: 0) {
                    name += chunk[..<end]
                    break
                }
                name += chunk
                at += Int64(chunk.count)
                guard name.count < 1 << 16 else { throw .damaged("a file name with no end") }
            }
            cursor += 16 + Int64(name.count) + 1
            let decoded = record.u16(14) & 0x80 != 0 ? String(decoding: name, as: UTF8.self) : ArchiveReader.decodeName(name)
            let path = ArchiveReader.pathComponents(decoded).joined(separator: "/")
            guard !path.isEmpty else { throw .damaged("a file with no name") }
            files.append(File(path: path, size: Int64(record.u32(0)), folder: folder, offset: Int64(record.u32(4))))
        }
        for folder in Set(files.map(\.folder)) {
            let method = folders[folder].compression & 0xF
            guard method == 0 || method == 3 else {
                throw .unsupported(["MSZIP", "Quantum"].indices.contains(Int(method) - 1) ? ["MSZIP", "Quantum"][Int(method) - 1] + " compression" : "compression type \(method)")
            }
        }
        let byPlace = files.sorted { ($0.folder, $0.offset) < ($1.folder, $1.offset) }
        for (earlier, later) in zip(byPlace, byPlace.dropFirst()) where earlier.folder == later.folder {
            guard earlier.offset + earlier.size <= later.offset else { throw .unsupported("files that share their data") }
        }
        self.files = files
        self.folders = folders
        self.start = start
        self.dataReserve = dataReserve
    }

    func extract(from url: URL, write: (Int, UnsafeRawBufferPointer) throws -> Void) throws {
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        let order = files.indices.sorted { (files[$0].folder, files[$0].offset) < (files[$1].folder, files[$1].offset) }
        let output = UnsafeMutablePointer<UInt8>.allocate(capacity: 1 << 16)
        defer { output.deallocate() }
        var next = 0
        while next < order.count {
            let folderIndex = files[order[next]].folder
            let folder = folders[folderIndex]
            var pending = order[next...].prefix { files[$0].folder == folderIndex }
            next += pending.count
            let method = folder.compression & 0xF
            var decoder: LZXDecoder?
            if method == 3 {
                do {
                    decoder = try LZXDecoder(windowBits: Int(folder.compression >> 8 & 0x1F))
                } catch {
                    throw EnigmaPack.Failure.unsupported(error.detail)
                }
            }
            var cursor = folder.dataOffset
            var position: Int64 = 0
            for index in pending where files[index].size == 0 { try write(index, UnsafeRawBufferPointer(start: output, count: 0)) }
            pending = pending.filter { files[$0].size > 0 }[...]
            for _ in 0..<folder.blockCount {
                guard !pending.isEmpty else { break }
                try Task.checkCancellation()
                try handle.seek(toOffset: UInt64(cursor))
                let header = [UInt8](try handle.read(upToCount: 8) ?? Data())
                guard header.count == 8 else { throw EnigmaPack.Failure.damaged("the cabinet is cut short") }
                let stored = Int(header.u16(4)), size = Int(header.u16(6))
                try handle.seek(toOffset: UInt64(cursor + 8 + Int64(dataReserve)))
                let data = try handle.read(upToCount: stored) ?? Data()
                guard data.count == stored else { throw EnigmaPack.Failure.damaged("the cabinet is cut short") }
                cursor += 8 + Int64(dataReserve) + Int64(stored)
                if let decoder {
                    do {
                        try data.withUnsafeBytes { try decoder.decode($0, count: size, into: output) }
                    } catch let error as LZXDecoder.Damaged {
                        throw EnigmaPack.Failure.damaged(error.detail)
                    }
                } else {
                    guard stored == size else { throw EnigmaPack.Failure.damaged("a stored block of the wrong size") }
                    data.copyBytes(to: output, count: size)
                }
                let blockEnd = position + Int64(size)
                while let index = pending.first, files[index].offset < blockEnd {
                    let file = files[index]
                    let from = max(file.offset, position), to = min(file.offset + file.size, blockEnd)
                    try write(index, UnsafeRawBufferPointer(start: output + Int(from - position), count: Int(to - from)))
                    if to < file.offset + file.size { break }
                    pending.removeFirst()
                }
                position = blockEnd
            }
            if let missing = pending.first { throw EnigmaPack.Failure.damaged("the cabinet ends inside \(files[missing].path)") }
        }
    }
}
