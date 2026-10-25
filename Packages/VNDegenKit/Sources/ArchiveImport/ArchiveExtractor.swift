import Foundation
import System

/// What a listing pass finds in an archive. Listing reads headers only, so it is fast even for large archives.
nonisolated public struct ArchiveContents: Sendable, Equatable {
    public var fileCount = 0
    public var byteCount: Int64 = 0
    public var isEncrypted = false
}

nonisolated public struct ImportProgress: Sendable, Equatable {
    public var files = 0
    public var bytes: Int64 = 0
    public var total: ArchiveContents

    public var fraction: Double { total.byteCount > 0 ? min(1, Double(bytes) / Double(total.byteCount)) : 0 }
}

nonisolated enum ArchiveExtractor {
    static func list(_ source: ArchiveSource) throws(ArchiveError) -> ArchiveContents {
        let reader = try ArchiveReader(source.volumes, password: nil)
        var contents = ArchiveContents()
        while let entry = try reader.next() {
            contents.isEncrypted = contents.isEncrypted || entry.isEncrypted
            guard entry.kind == .file else { continue }
            contents.fileCount += 1
            contents.byteCount += entry.size ?? 0
        }
        if contents.isEncrypted, !reader.canDecrypt { throw .unsupportedEncryption }
        return contents
    }

    /// Streams every file into `folder`. Throws `CancellationError` when the calling task is cancelled.
    static func extract(
        _ source: ArchiveSource, password: String?, into folder: URL, total: ArchiveContents,
        progress report: (ImportProgress) -> Void
    ) throws -> ImportProgress {
        let reader = try ArchiveReader(source.volumes, password: password)
        let isZip = source.volumes.count == 1 && source.volumes[0].pathExtension.lowercased() == "zip"
        let deflate64 = isZip ? try Deflate64Entries(zip: source.volumes[0]) : nil
        let fm = FileManager.default
        var progress = ImportProgress(total: total)
        var lastReport = ContinuousClock.now
        var lastParent: URL?

        while let entry = try reader.next() {
            try Task.checkCancellation()
            guard !entry.path.isEmpty else { continue }
            let url = entry.path.reduce(folder) { $0.appendingPathComponent($1) }
            switch entry.kind {
            case .other:
                continue
            case .directory:
                try fm.createDirectory(at: url, withIntermediateDirectories: true)
            case .file:
                let parent = url.deletingLastPathComponent()
                if parent != lastParent {
                    try fm.createDirectory(at: parent, withIntermediateDirectories: true)
                    lastParent = parent
                }
                let file = try FileDescriptor.open(
                    FilePath(url.path), .writeOnly, options: [.create, .truncate],
                    permissions: [.ownerReadWrite, .groupRead, .otherRead])
                defer { try? file.close() }

                func write(_ block: UnsafeRawBufferPointer, at offset: Int64) throws {
                    try Task.checkCancellation()
                    _ = try file.writeAll(toAbsoluteOffset: offset, block)
                    progress.bytes += Int64(block.count)
                    if ContinuousClock.now - lastReport > .milliseconds(100) {
                        report(progress)
                        lastReport = .now
                    }
                }
                if let deflate64, let location = deflate64.entries[entry.rawName] {
                    var offset: Int64 = 0
                    try deflate64.inflate(location) { block in
                        try write(block, at: offset)
                        offset += Int64(block.count)
                    }
                } else {
                    try reader.readData(write)
                }
                progress.files += 1
            }
        }
        return progress
    }

    /// The folder that becomes the game: the archive's only top-level folder, or the whole extraction when the archive
    /// holds several items. macOS metadata folders don't count.
    static func gameRoot(in staging: URL) -> URL {
        let items = visibleItems(in: staging)
        if items.count == 1, isDirectory(items[0]) { return items[0] }
        return staging
    }

    /// The archive inside `root` when that is all the extraction produced: no subfolders and one archive, possibly
    /// split into volumes, next to any loose files such as a readme or a web link.
    static func nestedArchive(in root: URL) -> ArchiveSource? {
        let items = visibleItems(in: root)
        guard !items.contains(where: isDirectory) else { return nil }
        let archives = items.filter { VolumeName($0.lastPathComponent) != nil }
        guard !archives.isEmpty else { return nil }
        return try? ArchiveSource(picked: archives)
    }

    private static func visibleItems(in folder: URL) -> [URL] {
        let items = (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.isDirectoryKey])) ?? []
        return items.filter { !["__MACOSX", ".DS_Store"].contains($0.lastPathComponent) }
    }

    private static func isDirectory(_ url: URL) -> Bool {
        (try? url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true
    }
}
