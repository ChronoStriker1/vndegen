import Foundation
import VNCore

nonisolated extension ArchiveExtractor {
    static func unpackWebArchives(in root: URL, reserve: Int64 = spaceReserve) throws {
        try unpackPackageNW(in: root, reserve: reserve)
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

    private static func replace(_ file: URL, _ fill: (URL) throws -> Bool) throws {
        let fm = FileManager.default
        let staging = file.deletingLastPathComponent().appendingPathComponent(".\(file.lastPathComponent)-\(UUID().uuidString)", isDirectory: true)
        try fm.createDirectory(at: staging, withIntermediateDirectories: true)
        do {
            guard try fill(staging) else {
                try fm.removeItem(at: staging)
                return
            }
            try fm.removeItem(at: file)
            try fm.moveItem(at: staging, to: file)
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
