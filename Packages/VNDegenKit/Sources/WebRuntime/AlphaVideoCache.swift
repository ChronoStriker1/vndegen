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
    /// Clips already logged as having no alpha, so their repeated range requests don't log again.
    private var opaqueLogged = Set<String>()
    private let converter = AlphaVideoConverter()

    public func clear() throws {
        if FileManager.default.fileExists(atPath: root.path) { try FileManager.default.removeItem(at: root) }
        cleaned = false
        opaqueLogged.removeAll()
    }

    private func directory() throws -> URL {
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        var directory = root, values = URLResourceValues()
        values.isExcludedFromBackup = true
        try directory.setResourceValues(values)
        if !cleaned {
            for file in try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil)
                where file.pathExtension == "part" { try FileManager.default.removeItem(at: file) }
            cleaned = true
        }
        return root
    }

    private func noteOpaque(_ clip: String) -> Bool {
        opaqueLogged.insert(clip).inserted
    }

    fileprivate func store(_ partial: URL, as cached: URL) throws {
        guard !FileManager.default.fileExists(atPath: cached.path) else { return }
        let size = try partial.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
        guard size <= Self.limit else { throw CocoaError(.fileWriteOutOfSpace) }
        try evict(adding: size)
        try FileManager.default.moveItem(at: partial, to: cached)
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

    /// The converted movie, nil for a WebM without alpha, which is served as it is. Fails rather than serve an alpha
    /// WebM unconverted, because WebKit draws its transparent parts opaque, often white over the whole scene.
    nonisolated func response(folder: GameFolder, path: String, method: String, range: String?,
                              cancellation: AlphaVideoCancellation, log: Logger) async throws -> FileResponse? {
        do {
            return try await convertedResponse(folder: folder, path: path, method: method, range: range,
                                               cancellation: cancellation, log: log)
        } catch {
            if !(error is CancellationError) && !Task.isCancelled {
                log.error("alpha video \(path, privacy: .public) not served: \(error.localizedDescription, privacy: .public)")
            }
            throw error
        }
    }

    private nonisolated func convertedResponse(folder: GameFolder, path: String, method: String, range: String?,
                                               cancellation: AlphaVideoCancellation, log: Logger) async throws -> FileResponse? {
        try Task.checkCancellation()
        guard path.lowercased().hasSuffix(".webm"), let (handle, base, _, name) = folder.open(servedPath: path), base == 0 else { return nil }
        defer { try? handle.close() }
        // Read the descriptor GameFolder confined, rather than reopen a path that could change beneath us.
        let source = handle.fileDescriptor
        var error = [CChar](repeating: 0, count: 512)
        switch vnalpha_inspect(source, &error, Int32(error.count)) {
        case 1: break
        case 0:
            if await noteOpaque(folder.root.path + "\u{0}" + path) {
                log.notice("alpha video \(name, privacy: .public): no alpha track, served as WebM")
            }
            return nil
        default:
            let reason = String(decoding: error.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
            log.error("alpha video \(name, privacy: .public): can't be checked, \(reason, privacy: .public)")
            throw CocoaError(.fileReadUnknown)
        }
        var info = stat()
        guard fstat(handle.fileDescriptor, &info) == 0 else { throw CocoaError(.fileReadUnknown) }
        let identity = "\(info.st_dev):\(info.st_ino):\(info.st_size):\(info.st_mtimespec.tv_sec):\(info.st_mtimespec.tv_nsec)"
        let key = SHA256.hash(data: Data(identity.utf8)).map { String(format: "%02x", $0) }.joined()
        let cached = try await directory().appendingPathComponent(key + ".mov")
        if !FileManager.default.fileExists(atPath: cached.path) {
            try await converter.convert(source, to: cached, name: name, cancellation: cancellation, log: log, cache: self)
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

private actor AlphaVideoConverter {
    func convert(_ source: Int32, to cached: URL, name: String, cancellation: AlphaVideoCancellation, log: Logger,
                 cache: AlphaVideoCache) async throws {
        try Task.checkCancellation()
        guard !FileManager.default.fileExists(atPath: cached.path) else { return }
        let partial = cached.deletingLastPathComponent().appendingPathComponent(UUID().uuidString + ".part")
        defer { try? FileManager.default.removeItem(at: partial) }
        var result = vnalpha_result()
        var error = [CChar](repeating: 0, count: 512)
        let status = vnalpha_convert(source, partial.path, cancellation.native, &result, &error, Int32(error.count))
        let detail = status == 0 ? "complete" : String(decoding: error.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
        log.notice("alpha video \(name, privacy: .public): clip \(result.duration) s, conversion \(result.seconds) s, process peak \(result.peak_bytes / 1048576) MiB, \(detail, privacy: .public)")
        guard status == 0 else { throw CocoaError(.fileReadCorruptFile) }
        try Task.checkCancellation()
        try await cache.store(partial, as: cached)
    }
}
