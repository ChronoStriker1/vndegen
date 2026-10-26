import CryptoKit
import Foundation
import VNCore

/// A Ludusavi backup folder: one subfolder per game, each with a mapping.yaml. VNDegen keeps a partial copy of the
/// server's store this way and syncs it with rsync. New backups go into new folders, mapping.yaml is rewritten, and an
/// existing backup is only ever removed by the retention limits the user set, as Ludusavi removes it.
public struct LudusaviStore: Sendable {
    public let root: URL

    public init(root: URL) { self.root = root }

    static let mappingFile = "mapping.yaml"
    static let previousMappingFile = "mapping.yaml.vndegen-previous"

    public func mapping(_ folder: String) throws -> LudusaviMapping {
        let url = root.appendingPathComponent(folder).appendingPathComponent(Self.mappingFile)
        let text: String
        do {
            text = try String(contentsOf: url, encoding: .utf8)
        } catch {
            throw SaveSyncError.unreadable(url.path)
        }
        return try LudusaviMapping(yaml: text)
    }

    // MARK: Restore

    /// Copies the files of `backup` (nil for the latest) that belong to `locations` into place. Snapshots the current
    /// local saves into `snapshot` first, so `undoRestore` can put them back. Checks every file's hash before writing.
    public func restore(folder: String, backup: String?, locations: [SaveLocation], snapshot: URL) throws -> RestoreReport {
        let mapping = try mapping(folder)
        let plan = try Self.restorePlan(mapping, backup: backup, locations: locations)
        let gameDir = root.appendingPathComponent(folder, isDirectory: true)

        var contents: [(URL, Data)] = []
        for item in plan.items {
            if item.stored.backup.hasSuffix(".zip") { throw SaveSyncError.zipBackup(item.stored.backup) }
            guard let stored = mapping.storedPath(of: item.path, in: item.stored.backup) else { throw SaveSyncError.unsupportedPath(item.path) }
            let source = gameDir.appendingPathComponent(stored)
            let target = locations[item.location].directory.appendingPathComponent(item.relative)
            try Self.requireInside(locations[item.location].directory, target)
            let data: Data
            do { data = try Data(contentsOf: source) } catch { throw SaveSyncError.unreadable(source.path) }
            guard UInt64(data.count) == item.stored.entry.size, Self.sha1(data) == item.stored.entry.hash else {
                throw SaveSyncError.hashMismatch(source.path)
            }
            contents.append((target, data))
        }

        try Self.takeSnapshot(of: locations, to: snapshot)
        let fm = FileManager.default
        for (target, data) in contents {
            try fm.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: target, options: .atomic)
        }
        return RestoreReport(backup: backup ?? mapping.summaries.last?.name ?? "", restored: plan.items.map(\.relative).sorted(),
                             skipped: plan.skipped)
    }

    /// The saves a restore of `backup` (nil for the latest) writes. A save the backup holds under more than one path
    /// restores from the path a backup would record it under.
    struct RestorePlan {
        struct Item {
            var location: Int
            var relative: String
            /// The path the backup recorded.
            var path: String
            var stored: StoredFile
            /// `location:relative`, the key of `localHashes`.
            var key: String { "\(location):\(relative)" }
        }

        var items: [Item]
        /// Paths in the backup that are not saves this engine keeps.
        var skipped: [String]
    }

    static func restorePlan(_ mapping: LudusaviMapping, backup: String?, locations: [SaveLocation]) throws -> RestorePlan {
        guard let files = mapping.effectiveFiles(backup) else { throw SaveSyncError.noBackups }
        let recorded = RecordedPaths(files.keys, locations: locations)
        var plan = RestorePlan(items: [], skipped: [])
        for (path, stored) in files.sortedByKey {
            guard let (index, relative) = claim(path, locations), recorded.path(of: relative, in: index) == path else {
                plan.skipped.append(path)
                continue
            }
            plan.items.append(.init(location: index, relative: relative, path: path, stored: stored))
        }
        guard !plan.items.isEmpty else { throw SaveSyncError.nothingToRestore(plan.skipped.count) }
        return plan
    }

    /// What restoring `backup` would change, without changing anything.
    public func preview(folder: String, backup: String?, locations: [SaveLocation]) throws -> RestorePreview {
        let plan = try Self.restorePlan(mapping(folder), backup: backup, locations: locations)
        let local = try Self.localHashes(locations)
        var preview = RestorePreview()
        for item in plan.items {
            switch local[item.key] {
            case nil: preview.added.append(item.relative)
            case item.stored.entry.hash?: preview.unchanged.append(item.relative)
            default: preview.replaced.append(item.relative)
            }
        }
        let restored = Set(plan.items.map(\.key))
        preview.kept = try Self.localSaves(locations).filter { !restored.contains("\($0.location):\($0.relative)") }.map(\.relative).sorted()
        return preview
    }

    /// Each local save's SHA-1, keyed `location:relative`.
    static func localHashes(_ locations: [SaveLocation]) throws -> [String: String] {
        Dictionary(uniqueKeysWithValues: try localSaves(locations).map { save in
            let data: Data
            do { data = try Data(contentsOf: save.url) } catch { throw SaveSyncError.unreadable(save.url.path) }
            return ("\(save.location):\(save.relative)", sha1(data))
        })
    }

    /// The saves of the latest backup that this device would restore, keyed like `localHashes`. Equal to
    /// `localHashes` when the device already has exactly what the backup holds.
    func latestHashes(folder: String, locations: [SaveLocation]) throws -> (backup: String, hashes: [String: String])? {
        let mapping = try mapping(folder)
        guard let latest = mapping.summaries.last, let plan = try? Self.restorePlan(mapping, backup: nil, locations: locations) else { return nil }
        return (latest.name, Dictionary(uniqueKeysWithValues: plan.items.map { ($0.key, $0.stored.entry.hash) }))
    }

    /// Puts the saves from the last restore's snapshot back, removing saves the snapshot did not have. Reads every
    /// folder before changing anything, so a folder that can't be read stops the undo with no save touched.
    public static func undoRestore(locations: [SaveLocation], snapshot: URL) throws {
        let fm = FileManager.default
        guard fm.fileExists(atPath: snapshot.path) else { throw SaveSyncError.noSnapshot }
        guard isComplete(snapshot: snapshot) else { throw SaveSyncError.incompleteSnapshot }
        let plans = try locations.enumerated().map { index, location in
            let saved = snapshot.appendingPathComponent(String(index), isDirectory: true)
            guard fm.fileExists(atPath: saved.path) else { throw SaveSyncError.incompleteSnapshot }
            let keep = Set(try regularFiles(in: saved))
            for relative in keep { try requireInside(location.directory, location.directory.appendingPathComponent(relative)) }
            return (location: location, saved: saved, keep: keep, current: try localSaves([location]))
        }
        for plan in plans {
            for relative in plan.keep {
                let target = plan.location.directory.appendingPathComponent(relative)
                try fm.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
                try Data(contentsOf: plan.saved.appendingPathComponent(relative)).write(to: target, options: .atomic)
            }
            for save in plan.current where !plan.keep.contains(save.relative) {
                try fm.removeItem(at: save.url)
            }
        }
        try fm.removeItem(at: snapshot)
    }

    /// True when `snapshot` holds every save it was meant to. Only a complete snapshot can be undone to, because undo
    /// deletes the saves a snapshot lacks.
    public static func isComplete(snapshot: URL) -> Bool {
        FileManager.default.fileExists(atPath: snapshot.appendingPathComponent(snapshotCompleteFile).path)
    }

    static let snapshotCompleteFile = "complete"

    static func takeSnapshot(of locations: [SaveLocation], to snapshot: URL) throws {
        let fm = FileManager.default
        let staging = snapshot.deletingLastPathComponent().appendingPathComponent(snapshot.lastPathComponent + ".new")
        try? fm.removeItem(at: staging)
        for (index, location) in locations.enumerated() {
            let dir = staging.appendingPathComponent(String(index), isDirectory: true)
            try fm.createDirectory(at: dir, withIntermediateDirectories: true)
            for save in try localSaves([location]) {
                let target = dir.appendingPathComponent(save.relative)
                try fm.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
                try fm.copyItem(at: save.url, to: target)
            }
        }
        try Data().write(to: staging.appendingPathComponent(snapshotCompleteFile))
        try? fm.removeItem(at: snapshot)
        try fm.moveItem(at: staging, to: snapshot)
    }

    // MARK: Back up

    /// Writes the local saves as a new backup in Ludusavi's format. Differential when a full backup exists and
    /// `retention` allows it, as Ludusavi decides. Each file keeps the path the latest backup recorded for it, so a
    /// restore on the Steam Deck or the Mac lands in the right place. A save no backup has recorded yet goes under
    /// `pcFolders[location]` when there is one. Returns a report with a nil backup when the latest backup already
    /// matches.
    ///
    /// With `retention` set, the backups beyond its limits are dropped from mapping.yaml and their folders deleted, as
    /// Ludusavi's `forget_excess_backups` does. Nil infers the limits from the folder and never deletes.
    ///
    /// `device` ends the new folder's name, so two devices backing up in the same second never pick the same folder.
    public func backUp(folder: String?, title: String, game: GameID, locations: [SaveLocation], retention: Retention?,
                       pcFolders: [Int: String] = [:], now: Date = .now, device: String? = nil, os: String, comment: String?) throws -> BackupReport {
        let local = try Self.localSaves(locations)
        guard !local.isEmpty else { throw SaveSyncError.noLocalSaves }

        let folder = folder ?? LudusaviMapping.escapeFolderName(title)
        let gameDir = root.appendingPathComponent(folder, isDirectory: true)
        let mappingURL = gameDir.appendingPathComponent(Self.mappingFile)
        let original = try Self.textIfPresent(mappingURL)
        var mapping = try original.map(LudusaviMapping.init(yaml:)) ?? LudusaviMapping(name: title)
        let latest = mapping.effectiveFiles() ?? [:]

        let paths = Self.canonicalPaths(for: local, locations: locations, latest: latest, game: game, pcFolders: pcFolders)

        // The new state: everything in the latest backup, with local saves on top. Files VNDegen does not manage,
        // and saves that are missing here, carry forward unchanged rather than being recorded as removed.
        var state: [String: Source] = latest.mapValues { .stored($0) }
        for (save, path) in zip(local, paths.recorded) {
            let data = try Data(contentsOf: save.url)
            state[path] = .local(save.url, FileEntry(hash: Self.sha1(data), size: UInt64(data.count)))
        }
        if !latest.isEmpty, state.mapValues(\.entry) == latest.mapValues(\.entry) {
            return BackupReport(backup: nil, isDifferential: false, filesWritten: 0, paths: paths.origin, pruned: [])
        }

        let limits = retention ?? Retention(inferredFrom: mapping)
        let differential = limits.allowsDifferential(after: mapping)
        let name = try Self.makeBackupFolder(differential: differential, now: now, device: device, mapping: mapping, gameDir: gameDir)

        // The files whose bytes go into the new folder, and the entries the mapping records.
        var copies: [String: Source] = [:]
        var diffFiles: [String: DifferentialBackup.Change] = [:]
        let full = mapping.backups.last
        if differential, let full {
            for (path, source) in state where full.files[path]?.hash != source.entry.hash {
                copies[path] = source
                diffFiles[path] = .changed(source.entry)
            }
            for path in full.files.keys where state[path] == nil { diffFiles[path] = .removed }
        } else {
            copies = state
        }

        let backupDir = gameDir.appendingPathComponent(name, isDirectory: true)
        let fm = FileManager.default
        var pruned: [String] = []
        do {
            for (path, source) in copies {
                guard let drive = LudusaviMapping.splitDrive(path)?.0 else { throw SaveSyncError.unsupportedPath(path) }
                if !mapping.drives.values.contains(drive) { mapping.drives[LudusaviMapping.driveFolderName(drive)] = drive }
                guard let relative = mapping.storedPath(of: path, in: name) else { throw SaveSyncError.unsupportedPath(path) }
                let target = gameDir.appendingPathComponent(relative)
                try fm.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
                switch source {
                case let .local(url, _):
                    try fm.copyItem(at: url, to: target)
                case let .stored(stored):
                    if stored.backup.hasSuffix(".zip") { throw SaveSyncError.zipBackup(stored.backup) }
                    guard let from = mapping.storedPath(of: path, in: stored.backup) else { throw SaveSyncError.unsupportedPath(path) }
                    try fm.copyItem(at: gameDir.appendingPathComponent(from), to: target)
                }
                let written = try Data(contentsOf: target)
                guard Self.sha1(written) == source.entry.hash else { throw SaveSyncError.hashMismatch(target.path) }
            }

            let when = Timestamp.format(now)
            if differential {
                mapping.backups[mapping.backups.count - 1].children.append(DifferentialBackup(
                    name: name, when: when, os: os, comment: comment, locked: false, semantics: nil, files: diffFiles,
                    registry: full?.registryHash == nil ? nil : .init(hash: nil)))
            } else {
                mapping.backups.append(FullBackup(
                    name: name, when: when, os: os, comment: comment, locked: false, semantics: nil,
                    files: copies.mapValues(\.entry), registryHash: nil, children: []))
            }
            if let retention { pruned = mapping.forgetExcessBackups(retention) }
            try writeMapping(mapping, to: mappingURL, expecting: original)
        } catch {
            // Only the folder this call created, which no other backup can share. Existing backups are never touched.
            try? fm.removeItem(at: backupDir)
            throw error
        }
        for folder in pruned { try? fm.removeItem(at: gameDir.appendingPathComponent(folder)) }
        return BackupReport(backup: name, isDifferential: differential, filesWritten: copies.count, paths: paths.origin, pruned: pruned)
    }

    /// Keeps the previous mapping.yaml beside the new one until the new one is written and reads back correctly.
    /// Refuses to write when someone else changed the file since it was read.
    private func writeMapping(_ mapping: LudusaviMapping, to url: URL, expecting original: String?) throws {
        let fm = FileManager.default
        let current = try Self.textIfPresent(url)
        guard current == original else { throw SaveSyncError.changedDuringBackup }
        let previous = url.deletingLastPathComponent().appendingPathComponent(Self.previousMappingFile)
        if current != nil {
            try? fm.removeItem(at: previous)
            try fm.copyItem(at: url, to: previous)
        }
        try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(mapping.yamlString().utf8).write(to: url, options: .atomic)
        guard let written = try? LudusaviMapping(yaml: String(contentsOf: url, encoding: .utf8)), written == mapping else {
            if current != nil { _ = try? fm.replaceItemAt(url, withItemAt: previous) }
            throw SaveSyncError.unreadable(url.path)
        }
        try? fm.removeItem(at: previous)
    }

    /// The file's text, or nil when there is no file. Any other failure throws, so a mapping that can't be read is never
    /// mistaken for a game with no backups and written over.
    static func textIfPresent(_ url: URL) throws -> String? {
        do {
            return try String(contentsOf: url, encoding: .utf8)
        } catch CocoaError.fileReadNoSuchFile {
            return nil
        } catch {
            throw SaveSyncError.unreadable(url.path)
        }
    }

    /// Creates the folder for a new backup and returns its name: Ludusavi's `backup-<time>` or `backup-<time>-diff`, then
    /// `-<device>` when there is one. Ludusavi reads backup names only from mapping.yaml and prunes any unlisted folder
    /// that starts with `backup-`, so the suffix changes nothing for it. Creating fails when the folder already exists
    /// here, so two backups never share one, and a failed backup removes only its own. The device suffix does the same
    /// for folders on the server that another device is still sending.
    static func makeBackupFolder(differential: Bool, now: Date, device: String? = nil, mapping: LudusaviMapping, gameDir: URL) throws -> String {
        let fm = FileManager.default
        try fm.createDirectory(at: gameDir, withIntermediateDirectories: true)
        var date = now
        while true {
            let name = "backup-\(Timestamp.folderStamp(date))\(differential ? "-diff" : "")\(device.map { "-" + $0 } ?? "")"
            if !mapping.summaries.contains(where: { $0.name == name }) {
                do {
                    try fm.createDirectory(at: gameDir.appendingPathComponent(name, isDirectory: true), withIntermediateDirectories: false)
                    return name
                } catch CocoaError.fileWriteFileExists {}
            }
            date += 1
        }
    }

    enum Source {
        case local(URL, FileEntry)
        case stored(StoredFile)

        var entry: FileEntry {
            switch self {
            case let .local(_, entry): entry
            case let .stored(stored): stored.entry
            }
        }
    }

    /// Picks the path each local save is recorded under, in the order of `local`. The path the latest backup used for
    /// that save wins. A new save takes the most common folder of that location's saves in the latest backup, then the
    /// PC folder from `pcFolders`. With nothing to go on, the save goes under `/VNDegen/<game>/`.
    static func canonicalPaths(for local: [LocalSave], locations: [SaveLocation], latest: [String: StoredFile],
                               game: GameID, pcFolders: [Int: String] = [:]) -> (recorded: [String], origin: [BackupReport.PathOrigin: Int]) {
        let known = RecordedPaths(latest.keys, locations: locations)
        var recorded: [String] = []
        var origin: [BackupReport.PathOrigin: Int] = [:]
        for save in local {
            let location = locations[save.location]
            if let path = known.path(of: save.relative, in: save.location) {
                recorded.append(path)
                origin[.latestBackup, default: 0] += 1
            } else if let folder = known.commonFolder[save.location] {
                recorded.append(folder + save.relative)
                origin[.commonFolder, default: 0] += 1
            } else if let folder = pcFolders[save.location], location.relativePath(folder + save.relative) == save.relative {
                recorded.append(folder + save.relative)
                origin[.pcFolder, default: 0] += 1
            } else {
                let own = "/VNDegen/\(game.rawValue)/\(location.directory.lastPathComponent)/\(save.relative)"
                let fallback = location.directory.appendingPathComponent(save.relative).path
                recorded.append(location.relativePath(own) == save.relative ? own : fallback)
                origin[.vndegen, default: 0] += 1
            }
        }
        return (recorded, origin)
    }

    /// Where a backup keeps each save of each location. A backup can hold the same save under several folders, such as
    /// the Steam Deck's and the Mac's, and restore and back up must agree on one: the folder holding most of that
    /// location's saves, then the alphabetically first.
    struct RecordedPaths {
        /// Location index to save path relative to the location to the chosen absolute path.
        private var chosen: [Int: [String: String]] = [:]
        /// Location index to the folder, with a trailing slash, that holds most of its saves.
        private(set) var commonFolder: [Int: String] = [:]

        init(_ paths: some Sequence<String>, locations: [SaveLocation]) {
            var folders: [Int: [String: [String]]] = [:]
            var counts: [Int: [String: Int]] = [:]
            for path in paths {
                guard let (index, relative) = LudusaviStore.claim(path, locations), path.hasSuffix(relative) else { continue }
                let folder = String(path.dropLast(relative.count))
                folders[index, default: [:]][relative, default: []].append(folder)
                counts[index, default: [:]][folder, default: 0] += 1
            }
            for (index, byRelative) in folders {
                let count = counts[index] ?? [:]
                let first = { (a: String, b: String) in count[a] != count[b] ? count[a, default: 0] > count[b, default: 0] : a < b }
                chosen[index] = Dictionary(uniqueKeysWithValues: byRelative.map { relative, candidates in
                    (relative, candidates.min(by: first)! + relative)
                })
                commonFolder[index] = count.keys.min(by: first)
            }
        }

        func path(of relative: String, in location: Int) -> String? { chosen[location]?[relative] }
    }

    // MARK: Local files

    /// Throws unless `target`, with the symlinks along it resolved, stays inside `directory`. Engines reject paths that
    /// climb out with `..`, but a symlink already in the save folder, such as `link -> ../../..`, would still carry
    /// `link/library.json` out of it.
    static func requireInside(_ directory: URL, _ target: URL) throws {
        let inside = resolvedComponents(directory)
        let resolved = resolvedComponents(target)
        guard resolved.count > inside.count, resolved.starts(with: inside) else { throw SaveSyncError.outsideSaveFolder(target.path) }
    }

    /// `url`'s path components with symlinks resolved, also when the end of the path does not exist yet.
    static func resolvedComponents(_ url: URL) -> [String] {
        var existing = url.standardizedFileURL
        var missing: [String] = []
        while !FileManager.default.fileExists(atPath: existing.path), existing.pathComponents.count > 1 {
            missing.insert(existing.lastPathComponent, at: 0)
            existing.deleteLastPathComponent()
        }
        return existing.resolvingSymlinksInPath().pathComponents + missing
    }

    struct LocalSave {
        var location: Int
        var relative: String
        var url: URL
    }

    /// Files in each location's directory that its rule recognizes as saves.
    static func localSaves(_ locations: [SaveLocation]) throws -> [LocalSave] {
        try locations.enumerated().flatMap { index, location in
            try regularFiles(in: location.directory).compactMap { relative in
                let url = location.directory.appendingPathComponent(relative)
                return location.relativePath(url.path) == relative ? LocalSave(location: index, relative: relative, url: url) : nil
            }
        }
    }

    /// Relative paths with forward slashes, skipping hidden files. A directory that does not exist has none. Any other
    /// failure throws, so a folder that could not be listed is never taken for an empty one.
    static func regularFiles(in directory: URL) throws -> [String] {
        do {
            _ = try directory.checkResourceIsReachable()
        } catch CocoaError.fileReadNoSuchFile {
            return []
        } catch {
            throw SaveSyncError.unreadable(directory.path)
        }
        let base = directory.standardizedFileURL.resolvingSymlinksInPath().pathComponents
        var failed: URL?
        guard let walker = FileManager.default.enumerator(at: directory, includingPropertiesForKeys: [.isRegularFileKey],
                                                          options: [.skipsHiddenFiles], errorHandler: { url, _ in failed = url; return false })
        else { throw SaveSyncError.unreadable(directory.path) }
        var files: [String] = []
        for case let url as URL in walker {
            let values: URLResourceValues
            do { values = try url.resourceValues(forKeys: [.isRegularFileKey]) } catch { throw SaveSyncError.unreadable(url.path) }
            guard values.isRegularFile == true else { continue }
            files.append(url.standardizedFileURL.resolvingSymlinksInPath().pathComponents.dropFirst(base.count).joined(separator: "/"))
        }
        if let failed { throw SaveSyncError.unreadable(failed.path) }
        return files.sorted()
    }

    /// The first location whose rule recognizes `path`, by index, and the path relative to it.
    static func claim(_ path: String, _ locations: [SaveLocation]) -> (Int, String)? {
        for (index, location) in locations.enumerated() {
            if let relative = location.relativePath(path) { return (index, relative) }
        }
        return nil
    }

    static func sha1(_ data: Data) -> String {
        Insecure.SHA1.hash(data: data).map { String(format: "%02x", $0) }.joined()
    }
}

/// How many backups Ludusavi keeps per game. Decides between full and differential, as Ludusavi's `plan_backup_kind`.
public struct Retention: Codable, Equatable, Sendable {
    public var full: Int
    public var differential: Int

    public init(full: Int, differential: Int) {
        self.full = full
        self.differential = differential
    }

    /// Without Ludusavi's config, assume it allows what the folder already has.
    init(inferredFrom mapping: LudusaviMapping) {
        full = max(1, mapping.backups.count)
        differential = mapping.backups.map(\.children.count).max() ?? 0
    }

    func allowsDifferential(after mapping: LudusaviMapping) -> Bool {
        let fulls = mapping.backups.filter { !$0.locked }.count
        let diffs = mapping.backups.last?.children.filter { !$0.locked }.count ?? 0
        return fulls > 0 && (diffs < differential || (full == 1 && differential > 0))
    }
}

public struct RestoreReport: Sendable {
    public var backup: String
    /// Paths relative to the save directory.
    public var restored: [String]
    /// Paths in the backup that are not saves this engine keeps, such as other games' files or settings.
    public var skipped: [String]
}

/// What restoring a backup would do to the saves on this device, by path relative to the save folder.
public struct RestorePreview: Equatable, Sendable {
    /// Saves the device does not have yet.
    public var added: [String] = []
    /// Saves the backup replaces with different content.
    public var replaced: [String] = []
    /// Saves that already match the backup.
    public var unchanged: [String] = []
    /// Saves the backup does not have. Restore leaves them as they are.
    public var kept: [String] = []
}

public struct BackupReport: Sendable {
    /// The new backup folder. Nil when the latest backup already matched the local saves.
    public var backup: String?
    public var isDifferential: Bool
    public var filesWritten: Int
    public var paths: [PathOrigin: Int]
    /// Backups the retention limits dropped from mapping.yaml. Their folders stay on the server until Ludusavi on a PC
    /// prunes them at its next backup of the game.
    public var pruned: [String]

    public enum PathOrigin: Sendable {
        /// The path the latest backup recorded for that save.
        case latestBackup
        /// A new save, put beside the latest backup's other saves.
        case commonFolder
        /// No backup recorded the game's saves yet, so the save is recorded where the PC keeps it: the Steam library
        /// the other backups use, the game's install folder, and the save folder inside it.
        case pcFolder
        /// No prior backup to follow, so the save is recorded under `/VNDegen/<game>/`.
        case vndegen
    }
}

public enum SaveSyncError: LocalizedError, Equatable {
    case notSetUp
    case keychain(OSStatus)
    case unreadable(String)
    case noBackups
    case nothingToRestore(Int)
    case hashMismatch(String)
    case zipBackup(String)
    case noLocalSaves
    case noSnapshot
    case incompleteSnapshot
    case unsupportedPath(String)
    case changedDuringBackup
    case changedAfterBackup
    case changedSinceCheck
    case incompleteUpload(String)
    case timedOut
    case engineUnsupported
    case outsideSaveFolder(String)

    public var errorDescription: String? {
        switch self {
        case .notSetUp: "Set up your Ludusavi server in Settings first."
        case let .keychain(status): "The Keychain refused VNDegen's SSH key (status \(status))."
        case let .unreadable(path): "Can't read \(path)."
        case .noBackups: "Ludusavi has no backups for this game yet."
        case let .nothingToRestore(count): "None of the \(count) files in this backup are saves this game uses."
        case let .hashMismatch(path): "\(path) does not match its recorded checksum. Nothing was changed."
        case let .zipBackup(name): "\(name) is a zip backup. VNDegen reads only Ludusavi's simple format."
        case .noLocalSaves: "This game has no saves to back up yet."
        case .noSnapshot: "There is no restore to undo."
        case .incompleteSnapshot: "The saves kept before the last restore are incomplete, so undoing it could lose saves. Nothing was changed."
        case let .unsupportedPath(path): "Can't store \(path) in a Ludusavi backup."
        case .changedDuringBackup: "Another device backed up this game while VNDegen was writing. Nothing was recorded. Try again."
        case .changedAfterBackup:
            "Another device backed up this game just after VNDegen did, so VNDegen's backup may not be listed. Check the game's backups."
        case .changedSinceCheck: "Another device backed up this game since VNDegen last checked it. Nothing was backed up. Check the game's backups first."
        case let .incompleteUpload(name): "The server's copy of \(name) is incomplete, so VNDegen did not record it. Nothing was changed. Try again."
        case .timedOut: "The server stopped answering, so VNDegen gave up. Try again."
        case .engineUnsupported: "Save sync does not support this game's engine yet."
        case let .outsideSaveFolder(path): "\(path) leads outside the game's save folder. Nothing was changed."
        }
    }
}
