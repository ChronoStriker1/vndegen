import CryptoKit
import Foundation
import RegistryFile
import VNCore

/// A Ludusavi backup folder: one subfolder per game, each with a mapping.yaml. VNDegen keeps a partial copy of the
/// server's store this way and syncs it with rclone. New backups go into new folders, mapping.yaml is rewritten, and an
/// existing backup is only ever removed by the retention limits the user set, as Ludusavi removes it.
public struct LudusaviStore: Sendable {
    public let root: URL

    public init(root: URL) { self.root = root }

    static let mappingFile = "mapping.yaml"
    static let previousMappingFile = "mapping.yaml.vndegen-previous"
    /// A backup's registry data, in the order Ludusavi looks for them.
    static let registryFiles = ["registry.reg", "registry.yaml"]

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

    /// Copies the files of `backup` (nil for the latest) that belong to `locations` into place, and `registry`'s values
    /// when the backup holds them. Snapshots the current local saves into `snapshot` first, so `undoRestore` can put them
    /// back. Checks every file's hash before writing.
    public func restore(folder: String, backup: String?, locations: [SaveLocation], registry: RegistrySave? = nil,
                        snapshot: URL) throws -> RestoreReport {
        let mapping = try mapping(folder)
        let gameDir = root.appendingPathComponent(folder, isDirectory: true)
        let values = registry.flatMap { Self.backupRegistryValues(mapping, backup: backup, save: $0, in: gameDir) }
        let plan = try Self.restorePlan(mapping, backup: backup, locations: locations, registry: values)

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

        try Self.takeSnapshot(of: locations, registry: registry, to: snapshot)
        let fm = FileManager.default
        for (target, data) in contents {
            try fm.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
            try data.write(to: target, options: .atomic)
        }
        if let registry, let values = plan.registry { try Self.writeLocalRegistry(values, to: registry) }
        return RestoreReport(backup: backup ?? mapping.summaries.last?.name ?? "", restored: plan.items.map(\.relative).sorted(),
                             skipped: plan.skipped, restoredRegistry: plan.registry != nil)
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
        /// The game's registry values the backup holds, when it keeps any.
        var registry: [String: RegistryValue]?
    }

    /// `registry` is what the backup holds for the game's registry key, which `backupRegistryValues` reads.
    static func restorePlan(_ mapping: LudusaviMapping, backup: String?, locations: [SaveLocation],
                            registry: [String: RegistryValue]? = nil) throws -> RestorePlan {
        guard let files = mapping.effectiveFiles(backup) else { throw SaveSyncError.noBackups }
        let recorded = RecordedPaths(files.keys, locations: locations)
        var plan = RestorePlan(items: [], skipped: [], registry: registry)
        var planned: [String: Int] = [:]
        // Names this plan writes, and their folders, by location and lowercased path, so a later name that differs only
        // in case lands on the same file or folder, as on Windows, even where nothing exists yet.
        var spellings: [Int: [String: String]] = [:]
        func spelled(_ relative: String, in location: Int) -> String {
            var parts = relative.split(separator: "/").map(String.init)
            for index in parts.indices {
                // Folded as FileCase compares names: ignoring case, with canonically equivalent forms equal.
                let key = parts[...index].joined(separator: "/").precomposedStringWithCanonicalMapping
                    .folding(options: .caseInsensitive, locale: nil)
                if let earlier = spellings[location]?[key] {
                    parts[index] = earlier
                } else {
                    spellings[location, default: [:]][key] = parts[index]
                }
            }
            return parts.joined(separator: "/")
        }
        for (path, stored) in files.sortedByKey {
            guard let (index, relative) = claim(path, locations), recorded.path(of: relative, in: index) == path else {
                plan.skipped.append(path)
                continue
            }
            // A save that exists here in another case, as a PC backup's `Save01.rvdata2` beside a local `save01.rvdata2`,
            // is that save, so the restore replaces it rather than writing a second file beside it.
            let local = spelled(FileCase.spelling(of: relative, in: locations[index].directory), in: index)
            let item = RestorePlan.Item(location: index, relative: local, path: path, stored: stored)
            // A backup from a disk that keeps case can hold `Save01.rvdata2` and `save01.rvdata2`, which are one save
            // here. The one spelled as on disk wins, otherwise the first.
            if let earlier = planned[item.key] {
                if relative == local {
                    plan.skipped.append(plan.items[earlier].path)
                    plan.items[earlier] = item
                } else {
                    plan.skipped.append(path)
                }
                continue
            }
            planned[item.key] = plan.items.count
            plan.items.append(item)
        }
        guard !plan.items.isEmpty || plan.registry != nil else { throw SaveSyncError.nothingToRestore(plan.skipped.count) }
        return plan
    }

    /// What restoring `backup` would change, without changing anything.
    public func preview(folder: String, backup: String?, locations: [SaveLocation], registry: RegistrySave? = nil) throws -> RestorePreview {
        let mapping = try mapping(folder)
        let gameDir = root.appendingPathComponent(folder, isDirectory: true)
        let values = registry.flatMap { Self.backupRegistryValues(mapping, backup: backup, save: $0, in: gameDir) }
        let plan = try Self.restorePlan(mapping, backup: backup, locations: locations, registry: values)
        let local = try Self.localHashes(locations, registry: registry)
        var preview = RestorePreview()
        for item in plan.items {
            switch local[item.key] {
            case nil: preview.added.append(item.relative)
            case item.stored.entry.hash?: preview.unchanged.append(item.relative)
            default: preview.replaced.append(item.relative)
            }
        }
        if let registry, let values = plan.registry {
            switch local[Self.registryHashKey] {
            case nil: preview.added.append(RestorePreview.registryName)
            case Self.registryHash(values, key: registry.key)?: preview.unchanged.append(RestorePreview.registryName)
            default: preview.replaced.append(RestorePreview.registryName)
            }
        }
        let restored = Set(plan.items.map(\.key))
        preview.kept = try Self.localSaves(locations).filter { !restored.contains("\($0.location):\($0.relative)") }.map(\.relative).sorted()
        if plan.registry == nil, local[Self.registryHashKey] != nil { preview.kept.append(RestorePreview.registryName) }
        return preview
    }

    /// Each local save's SHA-1, keyed `location:relative`, and the registry values' under `registryHashKey` when the
    /// game has a file of them here.
    static func localHashes(_ locations: [SaveLocation], registry: RegistrySave? = nil) throws -> [String: String] {
        var hashes = Dictionary(uniqueKeysWithValues: try localSaves(locations).map { save in
            let data: Data
            do { data = try Data(contentsOf: save.url) } catch { throw SaveSyncError.unreadable(save.url.path) }
            return ("\(save.location):\(save.relative)", sha1(data))
        })
        if let registry, let values = try localRegistryValues(registry) { hashes[registryHashKey] = registryHash(values, key: registry.key) }
        return hashes
    }

    /// The saves of the latest backup that this device would restore, keyed like `localHashes`. Equal to
    /// `localHashes` when the device already has exactly what the backup holds.
    func latestHashes(folder: String, locations: [SaveLocation], registry: RegistrySave? = nil) throws -> (backup: String, hashes: [String: String])? {
        let mapping = try mapping(folder)
        let gameDir = root.appendingPathComponent(folder, isDirectory: true)
        let values = registry.flatMap { Self.backupRegistryValues(mapping, backup: nil, save: $0, in: gameDir) }
        guard let latest = mapping.summaries.last,
              let plan = try? Self.restorePlan(mapping, backup: nil, locations: locations, registry: values) else { return nil }
        var hashes = Dictionary(uniqueKeysWithValues: plan.items.map { ($0.key, $0.stored.entry.hash) })
        if let registry, let values { hashes[Self.registryHashKey] = Self.registryHash(values, key: registry.key) }
        return (latest.name, hashes)
    }

    /// Puts the saves from the last restore's snapshot back, removing saves the snapshot did not have. Reads every
    /// folder before changing anything, so a folder that can't be read stops the undo with no save touched.
    ///
    /// The registry file comes back too, or goes when the snapshot recorded that there was none. A snapshot taken without
    /// `registry` leaves it alone.
    public static func undoRestore(locations: [SaveLocation], registry: RegistrySave? = nil, snapshot: URL) throws {
        let fm = FileManager.default
        guard fm.fileExists(atPath: snapshot.path) else { throw SaveSyncError.noSnapshot }
        guard isComplete(snapshot: snapshot) else { throw SaveSyncError.incompleteSnapshot }
        // A snapshot from before VNDegen recorded its folders has none, and undoes as it always did.
        let foldersFile = snapshot.appendingPathComponent(snapshotFoldersFile)
        if fm.fileExists(atPath: foldersFile.path) {
            let recorded = try String(contentsOf: foldersFile, encoding: .utf8)
            guard recorded == locations.map(\.directory.standardizedFileURL.path).joined(separator: "\n") else {
                throw SaveSyncError.snapshotForOtherFolders
            }
        }
        // Read before anything changes. A saved file that can't be read stops the undo, and only a missing one means
        // there was none.
        let savedRegistry = snapshot.appendingPathComponent(snapshotRegistryFolder, isDirectory: true)
        var registryBack: Data??
        if let registry, fm.fileExists(atPath: savedRegistry.path) {
            // Listed rather than probed, since a folder that can't be searched hides a file as well as a missing one.
            let names: [String]
            do { names = try fm.contentsOfDirectory(atPath: savedRegistry.path) } catch { throw SaveSyncError.unreadable(savedRegistry.path) }
            let saved = savedRegistry.appendingPathComponent(registry.file.lastPathComponent)
            if names.contains(registry.file.lastPathComponent) {
                do { registryBack = .some(try Data(contentsOf: saved)) } catch { throw SaveSyncError.unreadable(saved.path) }
            } else {
                registryBack = .some(nil)
            }
        }
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
        if let registry, let registryBack {
            if let data = registryBack {
                try fm.createDirectory(at: registry.file.deletingLastPathComponent(), withIntermediateDirectories: true)
                try data.write(to: registry.file, options: .atomic)
            } else if fm.fileExists(atPath: registry.file.path) {
                try fm.removeItem(at: registry.file)
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
    /// The save folders a snapshot was taken of, one path a line, so an undo never puts it into other ones, such as
    /// after a game switched how it runs.
    static let snapshotFoldersFile = "folders"
    /// Holds a copy of the registry file, or nothing when there was none. Location folders are numbered, so this name
    /// never meets one.
    static let snapshotRegistryFolder = "registry"

    static func takeSnapshot(of locations: [SaveLocation], registry: RegistrySave? = nil, to snapshot: URL) throws {
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
        if let registry {
            let dir = staging.appendingPathComponent(snapshotRegistryFolder, isDirectory: true)
            try fm.createDirectory(at: dir, withIntermediateDirectories: true)
            if fm.fileExists(atPath: registry.file.path) {
                try fm.copyItem(at: registry.file, to: dir.appendingPathComponent(registry.file.lastPathComponent))
            }
        }
        try Data(locations.map(\.directory.standardizedFileURL.path).joined(separator: "\n").utf8)
            .write(to: staging.appendingPathComponent(snapshotFoldersFile))
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
    ///
    /// Registry data from a Windows PC's backup carries forward like any other file VNDegen doesn't manage, so the next
    /// backup holds what the latest one held. Ludusavi on Linux or a Mac instead records it as dropped. A game with a
    /// `registry` file here changes only its own key: the new backup holds the latest registry with that key's values
    /// replaced by the file's, written as Ludusavi writes `registry.reg`. When the backup's registry can't be read, as in
    /// a zip, it carries forward and the file's values wait for a later backup.
    public func backUp(folder: String?, title: String, game: GameID, locations: [SaveLocation], registry: RegistrySave? = nil,
                       retention: Retention?, pcFolders: [Int: String] = [:], now: Date = .now, device: String? = nil, os: String,
                       comment: String?) throws -> BackupReport {
        let local = try Self.localSaves(locations)
        // A file with no values is a game that cleared its PlayerPrefs, which replaces what the backup holds.
        let localRegistry = try registry.flatMap { try Self.localRegistryValues($0) }
        guard !local.isEmpty || localRegistry != nil else { throw SaveSyncError.noLocalSaves }

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
        var saves: [String: String] = [:]
        for (save, path) in zip(local, paths.recorded) {
            let data = try Data(contentsOf: save.url)
            let hash = Self.sha1(data)
            state[path] = .local(save.url, FileEntry(hash: hash, size: UInt64(data.count)))
            saves["\(save.location):\(save.relative)"] = hash
        }
        var newRegistry: String?
        if let registry, let localRegistry {
            saves[Self.registryHashKey] = Self.registryHash(localRegistry, key: registry.key)
            // No registry yet starts an empty one. One the latest backup records but that can't be read, such as a damaged
            // file or a zip, is left as the carry-forward below leaves it, and the values wait for a later backup.
            let base: RegistryData? = if let held = Self.latestRegistry(mapping, in: gameDir) {
                Self.registryData(held, in: gameDir)
            } else if Self.recordsRegistry(mapping) {
                nil
            } else {
                RegistryData()
            }
            if var base, base.values(of: registry.key) != localRegistry {
                base.setValues(localRegistry, of: registry.key)
                newRegistry = base.regText
            }
        }
        if !mapping.backups.isEmpty, newRegistry == nil, state.mapValues(\.entry) == latest.mapValues(\.entry) {
            return BackupReport(backup: nil, isDifferential: false, filesWritten: 0, paths: paths.origin, pruned: [], saves: saves)
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
            // A differential inherits only from its full backup, so a registry an earlier differential holds comes along.
            let previousDiff = full?.children.last
            let carried = Self.latestRegistry(mapping, in: gameDir) ?? Self.unreadableRegistry(mapping, in: gameDir)
            let registryHash: String?
            let override: DifferentialBackup.RegistryOverride?
            if let newRegistry {
                let text = Data(newRegistry.utf8)
                try fm.createDirectory(at: backupDir, withIntermediateDirectories: true)
                try text.write(to: backupDir.appendingPathComponent("registry.reg"))
                registryHash = Self.sha1(text)
                override = .init(hash: registryHash)
            } else {
                let fromDiff = carried != nil && carried?.backup == previousDiff?.name
                override = fromDiff ? .init(hash: carried?.hash) : previousDiff?.registry?.hash == nil ? previousDiff?.registry : nil
                if let carried, !differential || fromDiff {
                    try Self.copyRegistry(carried, in: gameDir, to: backupDir)
                }
                registryHash = carried?.hash
            }
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
                    registry: override))
            } else {
                mapping.backups.append(FullBackup(
                    name: name, when: when, os: os, comment: comment, locked: false, semantics: nil,
                    files: copies.mapValues(\.entry), registryHash: registryHash, children: []))
            }
            if let retention { pruned = mapping.forgetExcessBackups(retention) }
            try writeMapping(mapping, to: mappingURL, expecting: original)
        } catch {
            // Only the folder this call created, which no other backup can share. Existing backups are never touched.
            try? fm.removeItem(at: backupDir)
            throw error
        }
        for folder in pruned { try? fm.removeItem(at: gameDir.appendingPathComponent(folder)) }
        return BackupReport(backup: name, isDifferential: differential, filesWritten: copies.count, paths: paths.origin, pruned: pruned,
                            saves: saves)
    }

    /// Registry data a backup holds: its hash, the backup folder its files are in, and which of `registryFiles` are there.
    struct StoredRegistry: Equatable {
        var hash: String
        var backup: String
        var files: [String]
    }

    /// The registry data a restore of the latest backup applies on Windows, found as Ludusavi's `registry_content` finds
    /// it: the latest differential's file, else nothing when that differential drops the registry, else the full backup's.
    /// Nil when Ludusavi would restore none, such as a recorded hash whose file is missing or can't be read.
    static func latestRegistry(_ mapping: LudusaviMapping, in gameDir: URL) -> StoredRegistry? {
        guard let full = mapping.backups.last else { return nil }
        if let diff = full.children.last, let override = diff.registry {
            if let found = stored(override.hash, in: diff.name, gameDir: gameDir) { return found }
            if override.hash == nil { return nil }
        }
        return stored(full.registryHash, in: full.name, gameDir: gameDir)
    }

    /// The registry files `backup` holds under the recorded `hash`, nil when it records none or none can be read.
    static func stored(_ hash: String?, in backup: String, gameDir: URL) -> StoredRegistry? {
        guard let hash else { return nil }
        // Ludusavi looks inside a zip backup. VNDegen can't, so it takes the registry to be there.
        if backup.hasSuffix(".zip") { return StoredRegistry(hash: hash, backup: backup, files: []) }
        let folder = backup == "." ? gameDir : gameDir.appendingPathComponent(backup, isDirectory: true)
        let files = registryFiles.filter { FileManager.default.fileExists(atPath: folder.appendingPathComponent($0).path) }
        guard files.contains(where: { readable($0, in: folder) }) else { return nil }
        return StoredRegistry(hash: hash, backup: backup, files: files)
    }

    /// Whether Ludusavi could read the registry file `name` in `folder`: it reads the file as UTF-8, and a `.reg` file
    /// must start with a header regedit writes. A file that fails this isn't the one a restore uses, so a damaged
    /// differential's file never replaces the full backup's good one. YAML files are only checked to be UTF-8 text.
    static func readable(_ name: String, in folder: URL) -> Bool {
        guard let data = try? Data(contentsOf: folder.appendingPathComponent(name)), !data.isEmpty,
              let text = String(data: data, encoding: .utf8) else { return false }
        // Ludusavi deserializes the YAML, so one that doesn't parse is skipped and a differential's falls back to the
        // full backup's.
        guard name.hasSuffix(".reg") else { return (try? registryData(yaml: text)) != nil }
        let first = text.drop { $0 == "\u{FEFF}" }.prefix { !$0.isNewline }.trimmingCharacters(in: .whitespaces)
        return first == "Windows Registry Editor Version 5.00" || first == "REGEDIT4"
    }

    /// Copies `registry`'s files into `backupDir` as they are, both formats when there are two, since Ludusavi reads the
    /// first that parses. Its recorded hash can't check a copy: Ludusavi hashes its own serialization, which has changed
    /// between versions.
    private static func copyRegistry(_ registry: StoredRegistry, in gameDir: URL, to backupDir: URL) throws {
        if registry.backup.hasSuffix(".zip") { throw SaveSyncError.zipBackup(registry.backup) }
        let folder = registry.backup == "." ? gameDir : gameDir.appendingPathComponent(registry.backup, isDirectory: true)
        for name in registry.files {
            try FileManager.default.copyItem(at: folder.appendingPathComponent(name), to: backupDir.appendingPathComponent(name))
        }
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
    /// True when the game's registry values came from the backup too.
    public var restoredRegistry = false
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

    /// How the lists name a game's registry values, such as a Unity game's PlayerPrefs.
    public static let registryName = "Game settings (registry)"
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
    /// The SHA-1 of each save as this backup read it, keyed like `localHashes`. A save the game writes while the backup
    /// runs is not in it, so the next check still counts that save as changed.
    public var saves: [String: String] = [:]

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
    case commitUnconfirmed(String)
    case timedOut
    case loginRefused
    case noStoreFolder(String)
    case engineUnsupported
    case outsideSaveFolder(String)
    /// The game keeps its saves somewhere else than when the restore's snapshot was taken.
    case snapshotForOtherFolders
    /// The game's library folder can't be reached, for the reason given.
    case gameFolderUnavailable(String)

    public var errorDescription: String? {
        switch self {
        case .notSetUp: "Set up your Ludusavi share in Settings first."
        case let .keychain(status): "The Keychain refused the share's password (status \(status))."
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
        case let .commitUnconfirmed(reason):
            "The server did not confirm the new backup list (\(reason)), so VNDegen deleted nothing. Check the game's backups before you back up again."
        case .timedOut: "The server stopped answering, so VNDegen gave up. Try again."
        case .loginRefused: "The share refused the username or password. Check them in Settings."
        case let .noStoreFolder(path): "The share has no folder \(path). Enter the share, then the folder Ludusavi backs up to, such as games/Ludusavi."
        case .engineUnsupported: "Save sync does not support this game's engine yet."
        case let .gameFolderUnavailable(reason): "The game's folder can't be reached. \(reason)"
        case let .outsideSaveFolder(path): "\(path) leads outside the game's save folder. Nothing was changed."
        case .snapshotForOtherFolders:
            "The saves kept before the last restore are from another save folder, such as before the game changed how it runs. Nothing was changed."
        }
    }
}
