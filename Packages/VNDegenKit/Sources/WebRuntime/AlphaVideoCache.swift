import CryptoKit
import CVNAlpha
import Foundation
import OSLog

// The C control contains only an atomic cancellation flag. A request retains it until the decoder has stopped.
nonisolated final class AlphaVideoCancellation: @unchecked Sendable {
    let native = vnalpha_control_create()!
    func cancel() { vnalpha_control_cancel(native) }
    deinit { vnalpha_control_destroy(native) }
}

/// Generated movies live in Caches, outside game folders, and never enter device backups.
public actor AlphaVideoCache {
    public static let shared = AlphaVideoCache()
    public static let limit = 512 << 20
    private let root = URL.cachesDirectory.appendingPathComponent("AlphaVideo-v1", isDirectory: true)
    private var cleaned = false

    public func clear() throws {
        if FileManager.default.fileExists(atPath: root.path) { try FileManager.default.removeItem(at: root) }
        cleaned = false
    }

    private func directory() throws {
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        var directory = root, values = URLResourceValues()
        values.isExcludedFromBackup = true
        try directory.setResourceValues(values)
        if !cleaned {
            for file in try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil)
                where file.pathExtension == "part" { try FileManager.default.removeItem(at: file) }
            cleaned = true
        }
    }

    private func evict(adding bytes: Int) throws {
        let files = try FileManager.default.contentsOfDirectory(at: root,
            includingPropertiesForKeys: [.fileSizeKey, .contentModificationDateKey]).filter { $0.pathExtension == "mov" }
        let entries = try files.map { file in
            let values = try file.resourceValues(forKeys: [.fileSizeKey, .contentModificationDateKey])
            return (file, values.fileSize ?? 0, values.contentModificationDate ?? .distantPast)
        }.sorted { $0.2 < $1.2 }
        var total = entries.reduce(bytes) { $0 + $1.1 }
        for (file, size, _) in entries where total > Self.limit {
            try FileManager.default.removeItem(at: file)
            total -= size
        }
    }

    // This actor runs the synchronous converter on its executor, serially and off the main thread.
    func response(folder: GameFolder, path: String, method: String, range: String?,
                  cancellation: AlphaVideoCancellation, log: Logger) throws -> FileResponse? {
        try Task.checkCancellation()
        guard path.lowercased().hasSuffix(".webm"), let (handle, base, _, name) = folder.open(servedPath: path), base == 0 else { return nil }
        defer { try? handle.close() }
        // Read the descriptor GameFolder confined, rather than reopen a path that could change beneath us.
        let source = "/dev/fd/\(handle.fileDescriptor)"
        guard vnalpha_is_alpha(source) != 0 else { return nil }
        var info = stat()
        guard fstat(handle.fileDescriptor, &info) == 0 else { throw CocoaError(.fileReadUnknown) }
        let identity = "\(info.st_dev):\(info.st_ino):\(info.st_size):\(info.st_mtimespec.tv_sec):\(info.st_mtimespec.tv_nsec)"
        let key = SHA256.hash(data: Data(identity.utf8)).map { String(format: "%02x", $0) }.joined()
        try directory()
        let cached = root.appendingPathComponent(key + ".mov")
        if !FileManager.default.fileExists(atPath: cached.path) {
            let partial = root.appendingPathComponent(UUID().uuidString + ".part")
            defer { try? FileManager.default.removeItem(at: partial) }
            var result = vnalpha_result(), error = [CChar](repeating: 0, count: 512)
            let status = vnalpha_convert(source, partial.path, cancellation.native, &result, &error, Int32(error.count))
            let detail = status == 0 ? "complete" : String(decoding: error.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            log.notice("alpha video \(name, privacy: .public): clip \(result.duration) s, conversion \(result.seconds) s, process peak \(result.peak_bytes / 1048576) MiB, \(detail, privacy: .public)")
            guard status == 0 else { throw CocoaError(.fileReadCorruptFile) }
            try Task.checkCancellation()
            let size = try partial.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
            guard size <= Self.limit else { throw CocoaError(.fileWriteOutOfSpace) }
            try evict(adding: size)
            try FileManager.default.moveItem(at: partial, to: cached)
        }
        try FileManager.default.setAttributes([.modificationDate: Date()], ofItemAtPath: cached.path)
        let output = try FileHandle(forReadingFrom: cached)
        var response = GameFolder.response(output, base: 0, size: Int64(try output.seekToEnd()),
            contentType: "video/quicktime", method: method, range: range)
        response.headers["X-VNDegen-Alpha"] = "1"
        response.headers["Cache-Control"] = "no-store"
        return response
    }
}
