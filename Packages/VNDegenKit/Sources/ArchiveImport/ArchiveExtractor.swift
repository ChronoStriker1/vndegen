import Foundation
import RuntimePackages
import System
import VNCore

/// What a listing pass finds in an archive. Listing reads headers only, so it is fast even for large archives.
nonisolated public struct ArchiveContents: Sendable, Equatable {
    public var fileCount = 0
    public var byteCount: Int64 = 0
    public var isEncrypted = false
    /// The most space extraction may use: the listed data plus one file system block for each file and folder. Nil when
    /// a header leaves a file's size out, and then only the free-space reserve bounds extraction.
    var spaceLimit: Int64?
    public internal(set) var installer: InstallerContents?

    func includingAddOns(_ include: Bool) -> ArchiveContents {
        guard var installer else { return self }
        installer.includesAddOns = include
        return ArchiveExtractor.contents(of: installer)
    }
}

nonisolated public struct InstallerContents: Sendable, Equatable {
    public let main: GameInstaller
    public let addOns: [GameInstaller]
    let mainVolumes: ArchiveSource.InstallerVolumes
    let addOnVolumes: [ArchiveSource.InstallerVolumes]
    public internal(set) var includesAddOns = true

    public var title: String { main.title }
    var folderName: String { InstallerName.folderName(for: main.title) }

    var selected: [(game: GameInstaller, volumes: ArchiveSource.InstallerVolumes)] {
        [(main, mainVolumes)] + (includesAddOns ? Array(zip(addOns, addOnVolumes)) : [])
    }
}

nonisolated public struct ImportProgress: Sendable, Equatable {
    public var files = 0
    public var bytes: Int64 = 0
    public var total: ArchiveContents

    public var fraction: Double { total.byteCount > 0 ? min(1, Double(bytes) / Double(total.byteCount)) : 0 }
}

nonisolated enum ArchiveExtractor {
    /// What each file and folder costs beyond its data, so an archive of many empty files is not free.
    static let entryOverhead: Int64 = 4096
    /// Free space an import always leaves, so it never fills the device to the last byte.
    static let spaceReserve: Int64 = 100_000_000
    /// No real archive lists more than 1 PiB. Capping the total keeps every later sum of sizes far from overflowing.
    static let maxListedSize: Int64 = 1 << 50

    static func list(_ source: ArchiveSource) throws(ArchiveError) -> ArchiveContents {
        if source.isInstaller { return try listInstallers(source) }
        let reader = try ArchiveReader(source.volumes, password: nil)
        var contents = ArchiveContents()
        var entries: Int64 = 0
        var everySizeListed = true
        while let entry = try reader.next() {
            contents.isEncrypted = contents.isEncrypted || entry.isEncrypted
            guard entry.kind != .other else { continue }
            entries += 1
            guard entry.kind == .file else { continue }
            contents.fileCount += 1
            if let size = entry.size {
                contents.byteCount = try addListed(size, to: contents.byteCount)
            } else {
                everySizeListed = false
            }
        }
        if contents.isEncrypted, !reader.canDecrypt { throw .unsupportedEncryption }
        if everySizeListed { contents.spaceLimit = try addListed(entries * entryOverhead, to: contents.byteCount) }
        return contents
    }

    private static func listInstallers(_ source: ArchiveSource) throws(ArchiveError) -> ArchiveContents {
        var opened: [(game: GameInstaller, volumes: ArchiveSource.InstallerVolumes)] = []
        for volumes in source.installers {
            let (setup, slices) = source.installerVolumes(volumes)
            do {
                opened.append((try GameInstaller.open(setup: setup, slices: slices), volumes))
            } catch {
                if volumes.picked { throw .installer(error) }
            }
        }
        let picked = opened.filter(\.volumes.picked)
        let pickedMains = picked.filter { !$0.game.isAddOn }
        guard pickedMains.count <= 1 else { throw .severalGames }
        let roots = Set(picked.compactMap { $0.game.isAddOn ? $0.game.gog?.rootGameId : nil })
        guard let main = pickedMains.first
                ?? opened.first(where: { !$0.game.isAddOn && $0.game.gameID.map(roots.contains) == true }) else {
            throw .addOnAlone(picked.first?.game.title ?? source.suggestedFolderName)
        }
        let addOns = opened.filter { $0.game.isAddOn && $0.game.gog?.rootGameId != nil && $0.game.gog?.rootGameId == main.game.gameID }
        guard picked.allSatisfy({ item in item.volumes == main.volumes || addOns.contains { $0.volumes == item.volumes } }) else {
            throw .severalGames
        }
        let installer = InstallerContents(main: main.game, addOns: addOns.map(\.game), mainVolumes: main.volumes,
                                          addOnVolumes: addOns.map(\.volumes))
        return contents(of: installer)
    }

    static func contents(of installer: InstallerContents) -> ArchiveContents {
        var contents = ArchiveContents()
        var entries: Int64 = 0
        for (game, _) in installer.selected {
            contents.fileCount += game.fileCount
            contents.byteCount += game.byteCount
            entries += Int64(game.fileCount + game.directoryCount)
        }
        contents.spaceLimit = contents.byteCount + entries * entryOverhead
        contents.installer = installer
        return contents
    }

    /// Adds a size from an archive header, which is untrusted, to a running total.
    private static func addListed(_ size: Int64, to total: Int64) throws(ArchiveError) -> Int64 {
        let (sum, overflow) = total.addingReportingOverflow(size)
        guard size >= 0, !overflow, sum <= maxListedSize else { throw .impossibleSize }
        return sum
    }

    /// Throws when `contents` would not fit in the free space with the reserve left over.
    static func checkSpace(for contents: ArchiveContents, on folder: URL) throws {
        let available = try freeSpace(on: folder)
        let needed = (contents.spaceLimit ?? contents.byteCount) + spaceReserve
        if needed > available { throw ArchiveError.notEnoughSpace(needed: needed, available: available) }
    }

    private static func freeSpace(on folder: URL) throws -> Int64 {
        var folder = folder
        // A URL caches resource values, and this one changes while extraction writes.
        folder.removeAllCachedResourceValues()
        return try folder.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey])
            .volumeAvailableCapacityForImportantUsage ?? .max
    }

    /// Streams every file into `folder`. Stops with `ArchiveError.largerThanListed` once the files use more than
    /// `total` listed, and with `ArchiveError.ranOutOfSpace` when free space falls below `reserve`. Throws
    /// `CancellationError` when the calling task is cancelled.
    static func extract(
        _ source: ArchiveSource, password: String?, into folder: URL, total: ArchiveContents,
        reserve: Int64 = spaceReserve, progress report: (ImportProgress) -> Void
    ) throws -> ImportProgress {
        if let installer = total.installer {
            return try extract(installer, from: source, into: folder, total: total, reserve: reserve, progress: report)
        }
        let reader = try ArchiveReader(source.volumes, password: password)
        let isZip = source.volumes.count == 1 && source.volumes[0].pathExtension.lowercased() == "zip"
        let deflate64 = isZip ? try Deflate64Entries(zip: source.volumes[0]) : nil
        let fm = FileManager.default
        var progress = ImportProgress(total: total)
        var lastReport = ContinuousClock.now
        var lastParent: URL?
        var folders: (entry: ArraySlice<String>, url: URL)?
        /// Each file written so far, by its path in lowercase. Extraction starts in an empty folder, so these are all its
        /// files, and reading them here keeps a folder's listing from being read again after every file.
        var files: [String: URL] = [:]
        var used: Int64 = 0
        var usedSinceSpaceCheck: Int64 = 0
        var lastSpaceCheck: ContinuousClock.Instant?

        /// Counts space before it is used. Free space is checked every 64 MB or quarter second, so neither large files
        /// nor many small ones can use up the reserve between checks.
        func use(_ bytes: Int64) throws {
            used += bytes
            if let limit = total.spaceLimit, used > limit { throw ArchiveError.largerThanListed }
            usedSinceSpaceCheck += bytes
            let now = ContinuousClock.now
            if usedSinceSpaceCheck >= 64 << 20 || lastSpaceCheck.map({ now - $0 >= .milliseconds(250) }) ?? true {
                if try freeSpace(on: folder) < reserve { throw ArchiveError.ranOutOfSpace }
                usedSinceSpaceCheck = 0
                lastSpaceCheck = now
            }
        }

        while let entry = try reader.next() {
            try Task.checkCancellation()
            guard !entry.path.isEmpty else { continue }
            // Windows takes `Img/` and `img/` in one archive for the same folder, so a folder that exists in another case
            // takes the entries. The previous entry's folder is reused, which keeps the lookup off most entries.
            let names = entry.kind == .directory ? entry.path[...] : entry.path.dropLast()
            if folders?.entry != names { folders = (names, FileCase.resolve(names.joined(separator: "/"), in: folder)) }
            var url = entry.kind == .directory ? folders!.url : folders!.url.appendingPathComponent(entry.path.last!)
            if entry.kind == .file {
                // A later `img/title.PNG` replaces `Img/Title.png`, as on Windows.
                let key = url.path.lowercased()
                if let earlier = files[key] { url = earlier } else { files[key] = url }
            }
            switch entry.kind {
            case .other:
                continue
            case .directory:
                try use(entryOverhead)
                try fm.createDirectory(at: url, withIntermediateDirectories: true)
            case .file:
                try use(entryOverhead)
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
                    try use(Int64(block.count))
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

    private static func extract(
        _ installer: InstallerContents, from source: ArchiveSource, into folder: URL, total: ArchiveContents,
        reserve: Int64, progress report: (ImportProgress) -> Void
    ) throws -> ImportProgress {
        var progress = ImportProgress(total: total)
        var lastReport = ContinuousClock.now
        var used: Int64 = 0
        var usedSinceSpaceCheck: Int64 = 0
        var lastSpaceCheck: ContinuousClock.Instant?
        func use(_ bytes: Int64) throws {
            used += bytes
            if let limit = total.spaceLimit, used > limit { throw ArchiveError.largerThanListed }
            usedSinceSpaceCheck += bytes
            let now = ContinuousClock.now
            if usedSinceSpaceCheck >= 64 << 20 || lastSpaceCheck.map({ now - $0 >= .milliseconds(250) }) ?? true {
                if try freeSpace(on: folder) < reserve { throw ArchiveError.ranOutOfSpace }
                usedSinceSpaceCheck = 0
                lastSpaceCheck = now
            }
        }
        for (game, volumes) in installer.selected {
            try use(Int64(game.directoryCount) * entryOverhead)
            let (setup, slices) = source.installerVolumes(volumes)
            do {
                try game.extract(setup: setup, slices: slices, into: folder) { bytes, finished in
                    try use(Int64(bytes) + (finished ? entryOverhead : 0))
                    progress.bytes += Int64(bytes)
                    if finished { progress.files += 1 }
                    if ContinuousClock.now - lastReport > .milliseconds(100) {
                        report(progress)
                        lastReport = .now
                    }
                }
            } catch let error as GameInstallerError {
                throw ArchiveError.installer(error)
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
        if !archives.isEmpty { return try? ArchiveSource(picked: archives) }
        let setups = items.filter { InstallerName($0.lastPathComponent)?.isSetup == true && GameInstaller.isInstaller($0) }
        guard !setups.isEmpty else { return nil }
        let parts = items.filter { InstallerName($0.lastPathComponent)?.isSetup == false }
        return try? ArchiveSource(picked: setups + parts)
    }

    private static func visibleItems(in folder: URL) -> [URL] {
        let items = (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.isDirectoryKey])) ?? []
        return items.filter { !["__MACOSX", ".DS_Store"].contains($0.lastPathComponent) }
    }

    private static func isDirectory(_ url: URL) -> Bool {
        (try? url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true
    }
}
