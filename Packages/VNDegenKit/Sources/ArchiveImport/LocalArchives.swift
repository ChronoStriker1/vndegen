import Foundation

/// An archive the user copied into VNDegen's own folder, which the library offers for import. Some sideloaded iPhone
/// installs can't receive a file from the picker, a drop, or Open in VNDegen, but they can read their own container.
nonisolated public struct LocalArchive: Sendable, Hashable, Identifiable {
    /// The first volume. The import sheet finds the other volumes beside it.
    public let url: URL
    /// Volumes found, counting the first.
    public let parts: Int
    public let byteCount: Int64

    public var id: URL { url }
    public var fileName: String { url.lastPathComponent }
}

nonisolated public enum LocalArchives {
    /// The archives at the top level of VNDegen's folders, one per volume set, by name. Game folders and dot files are
    /// skipped, and so is `Documents/Inbox`, which holds copies another app handed over.
    @concurrent
    public static func find(gamesRoot: URL) async -> [LocalArchive] {
        scan(folders(gamesRoot: gamesRoot))
    }

    @concurrent
    public static func listing(gamesRoot: URL) async -> [String] {
        folders(gamesRoot: gamesRoot).flatMap { folder in
            ((try? FileManager.default.contentsOfDirectory(atPath: folder.path)) ?? []).map { folder.path + "/" + $0 }
        }.sorted()
    }

    /// Games, and on iPhone and iPad also the Documents folder that Files shows as VNDegen.
    static func folders(gamesRoot: URL) -> [URL] {
        #if os(iOS)
        let documents = URL.documentsDirectory
        return documents.isSamePath(as: gamesRoot) ? [gamesRoot] : [documents, gamesRoot]
        #else
        return [gamesRoot]
        #endif
    }

    /// Whether `url` sits directly in one of the folders the library lists archives from.
    static func isListed(_ url: URL, gamesRoot: URL) -> Bool {
        let parent = url.deletingLastPathComponent()
        return folders(gamesRoot: gamesRoot).contains { $0.isSamePath(as: parent) }
    }

    static func scan(_ folders: [URL]) -> [LocalArchive] {
        folders.flatMap(scan).sorted { $0.fileName.localizedStandardCompare($1.fileName) == .orderedAscending }
    }

    private static func scan(_ folder: URL) -> [LocalArchive] {
        let keys: Set<URLResourceKey> = [.isRegularFileKey, .fileSizeKey]
        let urls = (try? FileManager.default.contentsOfDirectory(
            at: folder, includingPropertiesForKeys: Array(keys), options: .skipsHiddenFiles)) ?? []
        var sets: [String: [(name: VolumeName, url: URL, size: Int64)]] = [:]
        var setups: [String: (url: URL, size: Int64)] = [:]
        var parts: [String: [Int64]] = [:]
        for url in urls {
            guard let values = try? url.resourceValues(forKeys: keys), values.isRegularFile == true else { continue }
            let size = Int64(values.fileSize ?? 0)
            if let installer = InstallerName(url.lastPathComponent) {
                if installer.isSetup { setups[installer.stem] = (url, size) } else { parts[installer.stem, default: []].append(size) }
                continue
            }
            guard let name = VolumeName(url.lastPathComponent) else { continue }
            sets[name.set, default: []].append((name, url, size))
        }
        let installers = setups.map { stem, setup in
            LocalArchive(url: setup.url, parts: 1 + (parts[stem]?.count ?? 0),
                         byteCount: setup.size + (parts[stem] ?? []).reduce(0, +))
        }
        return installers + sets.values.compactMap { volumes in
            // A set whose first volume is missing can't be read, so it waits until the user copies that part in.
            guard let first = volumes.min(by: { $0.name.index < $1.name.index }),
                  first.name.index == first.name.kind.firstIndex else { return nil }
            return LocalArchive(url: first.url, parts: volumes.count, byteCount: volumes.reduce(0) { $0 + $1.size })
        }
    }
}

nonisolated extension URL {
    func isSamePath(as other: URL) -> Bool {
        resolvingSymlinksInPath().standardizedFileURL.path == other.resolvingSymlinksInPath().standardizedFileURL.path
    }
}
