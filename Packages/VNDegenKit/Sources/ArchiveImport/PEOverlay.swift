import Foundation

nonisolated struct PEOverlay {
    let size: UInt64
    let start: UInt64
    private let handle: FileHandle

    static func read<T>(_ url: URL, _ body: (PEOverlay) -> T?) -> T? {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? handle.close() }
        return PEOverlay(handle).flatMap(body)
    }

    private init?(_ handle: FileHandle) {
        guard let size = try? handle.seekToEnd(), let dos = Self.bytes(handle, at: 0, count: 64), dos[0] == 0x4d, dos[1] == 0x5a,
              let header = Self.bytes(handle, at: UInt64(dos.u32(0x3c)), count: 24), header.starts(with: Array("PE\0\0".utf8))
        else { return nil }
        let sections = Int(header.u16(6))
        guard sections <= 96,
              let table = Self.bytes(handle, at: UInt64(dos.u32(0x3c)) + 24 + UInt64(header.u16(20)), count: sections * 40) else { return nil }
        self.handle = handle
        self.size = size
        start = (0..<sections).map { UInt64(table.u32($0 * 40 + 20)) + UInt64(table.u32($0 * 40 + 16)) }.max() ?? 0
    }

    func bytes(at offset: UInt64, count: Int) -> [UInt8]? { Self.bytes(handle, at: offset, count: count) }

    private static func bytes(_ handle: FileHandle, at offset: UInt64, count: Int) -> [UInt8]? {
        guard (try? handle.seek(toOffset: offset)) != nil, let data = try? handle.read(upToCount: count), data.count == count else { return nil }
        return [UInt8](data)
    }
}
