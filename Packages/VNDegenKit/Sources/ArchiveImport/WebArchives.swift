import Foundation
import System
import VNCore
import WebRuntime

nonisolated extension ArchiveExtractor {
    static func unpackWebArchives(in root: URL, reserve: Int64 = spaceReserve) throws {
        try unpackPackageNW(in: root, reserve: reserve)
        try unpackAppAsar(in: root, reserve: reserve)
    }

    static func unpackPackageNW(in root: URL, reserve: Int64 = spaceReserve) throws {
        let fm = FileManager.default
        let package = FileCase.resolve("package.nw", in: root)
        guard ["nw.dll", "nw.pak"].contains(where: { fm.fileExists(atPath: FileCase.resolve($0, in: root).path) }),
              isFile(package) else { return }
        let source = ArchiveSource(packageNW: package)
        guard let contents = try? list(source) else { return }
        try checkSpace(for: contents, on: root)
        try replace(package) { staging in
            _ = try extract(source, password: nil, into: staging, total: contents, reserve: reserve) { _ in }
            return fm.fileExists(atPath: FileCase.resolve("index.html", in: staging).path)
        }
    }

    static func unpackAppAsar(in root: URL, reserve: Int64 = spaceReserve) throws {
        let fm = FileManager.default
        let resources = FileCase.resolve("resources", in: root)
        let asar = FileCase.resolve("app.asar", in: resources)
        let app = FileCase.resolve("app", in: resources)
        guard isFile(asar), !fm.fileExists(atPath: app.path), let handle = try? FileHandle(forReadingFrom: asar) else { return }
        defer { try? handle.close() }
        guard let archive = try? AsarArchive(reading: handle) else { return }
        let files = archive.files
        let cores = ["js/rmmz_core.js", "js/rpg_core.js"]
        guard files.contains(where: { file in cores.contains { file.path.lowercased().hasSuffix($0) } }) else { return }
        let unpacked = resources.appendingPathComponent(asar.lastPathComponent + ".unpacked", isDirectory: true)
        var total = ArchiveContents()
        total.fileCount = files.count
        for file in files {
            switch file.member {
            case let .packed(_, size): total.byteCount += size
            case let .unpacked(path): total.byteCount += (try? unpacked.appendingPathComponent(path).resourceValues(forKeys: [.fileSizeKey]).fileSize).map(Int64.init) ?? 0
            }
        }
        total.spaceLimit = total.byteCount + Int64(files.count * 2) * entryOverhead
        try checkSpace(for: total, on: root)
        let moved = try replace(asar, with: app) { staging in
            var meter = SpaceMeter(limit: total.spaceLimit, folder: staging, reserve: reserve)
            for file in files {
                try Task.checkCancellation()
                try meter.use(entryOverhead)
                let url = FileCase.resolve(file.path, in: staging)
                try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                switch file.member {
                case let .packed(offset, size):
                    let out = try FileDescriptor.open(FilePath(url.path), .writeOnly, options: [.create, .truncate],
                                                      permissions: [.ownerReadWrite, .groupRead, .otherRead])
                    defer { try? out.close() }
                    try handle.seek(toOffset: offset)
                    var left = size
                    while left > 0 {
                        let chunk = try handle.read(upToCount: Int(min(left, 1 << 20))) ?? Data()
                        guard !chunk.isEmpty else { throw ArchiveError.unreadable("\(asar.lastPathComponent) ends inside \(file.path)") }
                        try meter.use(Int64(chunk.count))
                        _ = try chunk.withUnsafeBytes { try out.writeAll($0) }
                        left -= Int64(chunk.count)
                    }
                case let .unpacked(path):
                    let from = FileCase.resolve(path, in: unpacked)
                    try meter.use((try? from.resourceValues(forKeys: [.fileSizeKey]).fileSize).map(Int64.init) ?? 0)
                    try fm.copyItem(at: from, to: url)
                }
            }
            return true
        }
        if moved, fm.fileExists(atPath: unpacked.path) { try fm.removeItem(at: unpacked) }
    }

    @discardableResult
    private static func replace(_ file: URL, with folder: URL? = nil, _ fill: (URL) throws -> Bool) throws -> Bool {
        let fm = FileManager.default
        let staging = file.deletingLastPathComponent().appendingPathComponent(".\(file.lastPathComponent)-\(UUID().uuidString)", isDirectory: true)
        try fm.createDirectory(at: staging, withIntermediateDirectories: true)
        do {
            guard try fill(staging) else {
                try fm.removeItem(at: staging)
                return false
            }
            try fm.removeItem(at: file)
            try fm.moveItem(at: staging, to: folder ?? file)
            return true
        } catch {
            try? fm.removeItem(at: staging)
            throw error
        }
    }

    private static func isFile(_ url: URL) -> Bool {
        var isFolder: ObjCBool = false
        return FileManager.default.fileExists(atPath: url.path, isDirectory: &isFolder) && !isFolder.boolValue
    }
}
