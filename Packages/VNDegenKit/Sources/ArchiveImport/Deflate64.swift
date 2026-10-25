import CArchive
import Foundation

/// The entries of a zip that use Deflate64 (method 9), which libarchive can't decompress. Windows writes Deflate64 when
/// it zips large files. These entries are read straight from the zip with zlib's infback9.
nonisolated struct Deflate64Entries {
    struct Location {
        var localHeaderOffset: UInt64
        var compressedSize: UInt64
        var size: UInt64
        var crc: UInt32
        var isEncrypted: Bool
    }

    /// Keyed by the name bytes as stored, which is also what libarchive reports.
    private(set) var entries: [[UInt8]: Location] = [:]
    private let url: URL

    init(zip url: URL) throws {
        self.url = url
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        let directory = try Self.centralDirectory(handle)
        var cursor = 0
        while cursor + 46 <= directory.count, directory.u32(cursor) == 0x0201_4b50 {
            let method = directory.u16(cursor + 10)
            let nameLength = Int(directory.u16(cursor + 28))
            let extraLength = Int(directory.u16(cursor + 30))
            let commentLength = Int(directory.u16(cursor + 32))
            let nameStart = cursor + 46
            guard nameStart + nameLength + extraLength <= directory.count else { break }
            if method == 9 {
                var location = Location(
                    localHeaderOffset: UInt64(directory.u32(cursor + 42)),
                    compressedSize: UInt64(directory.u32(cursor + 20)),
                    size: UInt64(directory.u32(cursor + 24)),
                    crc: directory.u32(cursor + 16),
                    isEncrypted: directory.u16(cursor + 8) & 1 != 0
                )
                Self.applyZip64(directory[(nameStart + nameLength)..<(nameStart + nameLength + extraLength)], to: &location)
                entries[Array(directory[nameStart..<(nameStart + nameLength)])] = location
            }
            cursor = nameStart + nameLength + extraLength + commentLength
        }
    }

    /// Decompresses one entry, handing the output to `write` in order.
    func inflate(_ location: Location, write: (UnsafeRawBufferPointer) throws -> Void) throws {
        guard !location.isEncrypted else { throw ArchiveError.unsupportedEncryption }
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        try handle.seek(toOffset: location.localHeaderOffset)
        let header = [UInt8](try handle.read(upToCount: 30) ?? Data())
        guard header.count == 30, header.u32(0) == 0x0403_4b50 else { throw ArchiveError.unreadable("Bad zip local header") }
        try handle.seek(toOffset: location.localHeaderOffset + 30 + UInt64(header.u16(26)) + UInt64(header.u16(28)))

        let window = UnsafeMutablePointer<UInt8>.allocate(capacity: 1 << 16)
        defer { window.deallocate() }
        var z = z_stream()
        guard inflateBack9Init_(&z, window, ZLIB_VERSION, Int32(MemoryLayout<z_stream>.size)) == Z_OK else {
            throw ArchiveError.unreadable("Deflate64 decoder failed to start")
        }
        defer { _ = inflateBack9End(&z) }

        // The stream is released before this closure returns, so `write` never outlives the call.
        try withoutActuallyEscaping(write) { write in
            let stream = InflateStream(handle: handle, remaining: location.compressedSize, write: write)
            let context = Unmanaged.passUnretained(stream).toOpaque()
            let result = inflateBack9(&z, { context, buffer in
                Unmanaged<InflateStream>.fromOpaque(context!).takeUnretainedValue().read(into: buffer!)
            }, context, { context, bytes, count in
                Unmanaged<InflateStream>.fromOpaque(context!).takeUnretainedValue().write(bytes!, count: count)
            }, context)
            if let error = stream.error { throw error }
            guard result == Z_STREAM_END, stream.written == location.size, stream.crc == location.crc else {
                throw ArchiveError.unreadable("Deflate64 data is damaged")
            }
        }
    }

    /// Finds the central directory through the end record, or the zip64 end record when the archive needs one.
    private static func centralDirectory(_ handle: FileHandle) throws -> [UInt8] {
        let fileSize = try handle.seekToEnd()
        let tailStart = fileSize - min(fileSize, 22 + 0xFFFF)
        try handle.seek(toOffset: tailStart)
        let tail = [UInt8](try handle.readToEnd() ?? Data())
        guard let end = stride(from: tail.count - 22, through: 0, by: -1).first(where: { tail.u32($0) == 0x0605_4b50 }) else {
            throw ArchiveError.unreadable("No zip central directory")
        }
        var size = UInt64(tail.u32(end + 12))
        var offset = UInt64(tail.u32(end + 16))
        if size == 0xFFFF_FFFF || offset == 0xFFFF_FFFF, end >= 20, tail.u32(end - 20) == 0x0706_4b50 {
            try handle.seek(toOffset: tail.u64(end - 20 + 8))
            let record = [UInt8](try handle.read(upToCount: 56) ?? Data())
            guard record.count == 56, record.u32(0) == 0x0606_4b50 else { throw ArchiveError.unreadable("Bad zip64 end record") }
            size = record.u64(40)
            offset = record.u64(48)
        }
        try handle.seek(toOffset: offset)
        return [UInt8](try handle.read(upToCount: Int(size)) ?? Data())
    }

    /// Replaces 32-bit fields that overflowed with their values from the zip64 extra field, which lists only those.
    private static func applyZip64(_ extra: ArraySlice<UInt8>, to location: inout Location) {
        let extra = Array(extra)
        var cursor = 0
        while cursor + 4 <= extra.count {
            let id = extra.u16(cursor), length = Int(extra.u16(cursor + 2))
            if id == 1 {
                var field = cursor + 4
                for keyPath in [\Location.size, \.compressedSize, \.localHeaderOffset] where location[keyPath: keyPath] == 0xFFFF_FFFF {
                    guard field + 8 <= cursor + 4 + length else { return }
                    location[keyPath: keyPath] = extra.u64(field)
                    field += 8
                }
            }
            cursor += 4 + length
        }
    }
}

/// State shared with infback9's C callbacks.
private nonisolated final class InflateStream {
    let handle: FileHandle
    var remaining: UInt64
    let writeBlock: (UnsafeRawBufferPointer) throws -> Void
    let input = UnsafeMutablePointer<UInt8>.allocate(capacity: 1 << 16)
    var written: UInt64 = 0
    var crc: UInt32 = 0
    var error: Error?

    init(handle: FileHandle, remaining: UInt64, write: @escaping (UnsafeRawBufferPointer) throws -> Void) {
        self.handle = handle
        self.remaining = remaining
        writeBlock = write
    }

    deinit { input.deallocate() }

    func read(into buffer: UnsafeMutablePointer<UnsafeMutablePointer<UInt8>?>) -> UInt32 {
        guard remaining > 0, error == nil else { return 0 }
        do {
            guard let data = try handle.read(upToCount: Int(min(remaining, 1 << 16))), !data.isEmpty else { return 0 }
            data.copyBytes(to: input, count: data.count)
            remaining -= UInt64(data.count)
            buffer.pointee = input
            return UInt32(data.count)
        } catch {
            self.error = error
            return 0
        }
    }

    func write(_ bytes: UnsafeMutablePointer<UInt8>, count: UInt32) -> Int32 {
        do {
            try writeBlock(UnsafeRawBufferPointer(start: bytes, count: Int(count)))
            crc = UInt32(crc32(uLong(crc), bytes, count))
            written += UInt64(count)
            return 0
        } catch {
            self.error = error
            return 1
        }
    }
}

private nonisolated extension Array where Element == UInt8 {
    func u16(_ at: Int) -> UInt16 { UInt16(self[at]) | UInt16(self[at + 1]) << 8 }
    func u32(_ at: Int) -> UInt32 { UInt32(u16(at)) | UInt32(u16(at + 2)) << 16 }
    func u64(_ at: Int) -> UInt64 { UInt64(u32(at)) | UInt64(u32(at + 4)) << 32 }
}
