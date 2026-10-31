import Foundation
import Observation
import OSLog
import Rclone
import VNCore
#if canImport(UIKit)
import UIKit
#endif

private let log = Logger(subsystem: "com.chronostriker1.vndegen", category: "savesync")

/// A VNDegen game's Ludusavi game.
public struct GameMatch: Equatable, Sendable {
    public enum Source: Equatable, Sendable {
        /// The user picked it.
        case picked
        /// The Steam app ID in the game's metadata, looked up in Ludusavi's manifest.
        case steamID(Int)
        /// A game in the store whose title matches.
        case storeTitle
        /// A game in Ludusavi's manifest whose title matches.
        case manifestTitle
        /// Nothing matched, so a first backup goes under the game's own title.
        case gameTitle
    }

    /// The game folder in the store.
    public var folder: String
    /// The Ludusavi title, which mapping.yaml records.
    public var title: String
    public var source: Source
    /// Whether the store has the folder yet.
    public var inStore: Bool
}

/// How a game's saves here compare with its latest backup on the server.
public enum SyncStatus: Equatable, Sendable {
    /// The server has no backup of the game yet.
    case noBackups
    /// The saves here match the latest backup, or neither they nor the server changed since the last backup or restore.
    case inSync
    /// The saves here changed since the last backup or restore, and the server did not.
    case localChanged
    /// A newer backup came from another device, and the saves here did not change since.
    case newerBackup(BackupSummary)
    /// The saves here changed, and so did the server.
    case conflict(BackupSummary)
}

/// A game's mapping.yaml on the server as one status check read it.
public struct ServerRevision: Equatable, Sendable {
    let mapping: Data?
}

/// A status check's result, and the server state it judged. A backup given that state refuses to run on any other, so
/// it never lands on top of a backup the check did not see.
public struct SyncCheck: Equatable, Sendable {
    public var status: SyncStatus
    public var revision: ServerRevision
}

/// Where a game keeps its saves, for the game page.
public struct SaveLocationInfo: Equatable, Sendable {
    /// The folder on this device.
    public var device: String
    /// The folder the latest backup recorded, such as the Steam Deck's.
    public var recorded: String?
}

/// Backs up and restores game saves through the user's Ludusavi store on an SMB share, with rclone's own SMB code as
/// Ludusavi uses it, so saves move between VNDegen and the PC copies that Ludusavi manages.
@MainActor
@Observable
public final class SaveSyncService {
    public enum Connection: Equatable {
        case notSetUp
        case checking
        case connected
        case failed(String)
    }

    /// Reaches the store through rclone. Tests replace it with a local folder and hooks between rc calls.
    public typealias Connector = @Sendable (SaveServer) async throws -> RemoteAccess

    /// The share, when one is set up.
    public var server: SaveServer? { settings.server }
    public private(set) var connection: Connection = .notSetUp
    /// How many game folders the last connection test found.
    public private(set) var foundGames: Int?
    /// Settings from VNDegen 0.4.0 named an SSH server, which save sync no longer reaches. True until a share is set up.
    public var needsShare: Bool { settings.droppedOldServer && settings.server == nil }
    /// Game folder in the store to its Ludusavi title.
    public private(set) var storeGames: [String: String] = [:]
    public var autoBackup: Bool {
        get { settings.autoBackup }
        set { settings.autoBackup = newValue; saveSettings() }
    }
    public var checkBeforeLaunch: Bool {
        get { settings.checkBeforeLaunch }
        set { settings.checkBeforeLaunch = newValue; saveSettings() }
    }
    /// Ludusavi's retention limits from its config on this Mac. Nil elsewhere. Tests clear it, so a Mac with Ludusavi
    /// installed runs them with the limits they set.
    public internal(set) var configRetention: Retention?
    /// The user's Ludusavi retention limits, entered by hand. Nil infers them from the store and never deletes a backup.
    public var manualRetention: Retention? {
        get { settings.retention }
        set { settings.retention = newValue; saveSettings() }
    }

    @ObservationIgnored private let library: LibraryStore
    @ObservationIgnored private let directory: URL
    @ObservationIgnored private let connector: Connector
    @ObservationIgnored private let manifestStore: ManifestStore?
    @ObservationIgnored private var manifest: ManifestIndex?
    @ObservationIgnored private var manifestRefresh: Task<Void, Never>?
    @ObservationIgnored private var steamLibrary: String?
    private var settings: Settings
    /// The last restore, undo, backup, or server read started. Each new one waits for it, so two never touch the same
    /// folders at once.
    @ObservationIgnored private var lastOperation: Task<Void, Never>?
    /// How long the check before launch waits, and how long one operation may keep its connection to the server.
    @ObservationIgnored var timeouts = Timeouts()
    /// The time new backups are named after. Tests set it.
    @ObservationIgnored var clock: @Sendable () -> Date = { .now }
    /// Runs right after a backup has read and copied the saves, before anything goes to the server. Tests set it.
    @ObservationIgnored var afterLocalBackup: (@Sendable () -> Void)?

    struct Timeouts: Sendable {
        var launchCheck: Duration = .seconds(6)
        var operation: Duration = .seconds(180)
        /// rclone's, on every call.
        var rclone = RcloneLimits()
    }

    /// Ludusavi's primary manifest, at the address Ludusavi itself reads it from.
    public static let ludusaviManifest = URL(string: "https://raw.githubusercontent.com/mtkennerly/ludusavi-manifest/master/data/manifest.yaml")!

    /// `manifest` nil leaves Ludusavi's manifest out, as tests do. On a Mac, the retention limits come from Ludusavi's own
    /// config when it has one.
    public convenience init(library: LibraryStore, connector: Connector? = nil, manifest: URL? = ludusaviManifest) {
        self.init(library: library, connector: connector, manifest: manifest, configRetention: LudusaviConfig.load()?.retention)
    }

    /// `configRetention` stands in for Ludusavi's config, so tests never depend on the config of the Mac they run on.
    init(library: LibraryStore, connector: Connector?, manifest: URL?, configRetention: Retention?) {
        self.library = library
        let directory = library.paths.support.appendingPathComponent("SaveSync", isDirectory: true)
        self.directory = directory
        settings = (try? JSONDecoder().decode(Settings.self, from: Data(contentsOf: directory.appendingPathComponent("settings.json")))) ?? Settings()
        self.configRetention = configRetention
        self.connector = connector ?? Self.smbConnector(directory: directory)
        manifestStore = manifest.map { ManifestStore(source: $0, directory: library.paths.support.appendingPathComponent("SaveSync", isDirectory: true)) }
        if settings.migrating {
            // Once, on the first start after 0.4.0: its SSH key goes, and so do the copy of the old store and the sync
            // records, as when the user forgets a server.
            OldSSHKey.delete(in: directory)
            settings.synced = [:]
            try? FileManager.default.removeItem(at: directory.appendingPathComponent("Store", isDirectory: true))
            settings.migrating = false
        }
        if settings.server != nil { connection = .connected }
        saveSettings()
        loadIndexFromMirror()
    }

    /// The share in `SaveServer`, with the password kept apart: in the Keychain on iPhone and iPad, in a file only this
    /// user can read on the Mac.
    public static func smbConnector(directory: URL) -> Connector {
        { server in
            guard case let .smb(smb) = server.remote else { throw SaveSyncError.notSetUp }
            return try await RemoteAccess.smb(server, password: smb.isGuest ? nil : SMBPassword.read(in: directory))
        }
    }

    private struct Settings: Codable {
        var server: SaveServer?
        /// The settings named an SSH server, as VNDegen 0.4.0 saved it.
        var droppedOldServer = false
        /// Set while reading 0.4.0 settings, and never saved.
        var migrating = false
        var autoBackup = false
        var checkBeforeLaunch = true
        var retention: Retention?
        /// Ludusavi game folder per game, when the user picked one.
        var matches: [String: String] = [:]
        /// Per game, the server's state and the saves here at the last backup or restore.
        var synced: [String: SyncRecord] = [:]
        /// Ends this device's backup folder names, so they never collide with another device's.
        var deviceID = Settings.newDeviceID()

        init() {}

        static func newDeviceID() -> String {
            String(UUID().uuidString.replacingOccurrences(of: "-", with: "").prefix(8)).lowercased()
        }

        enum CodingKeys: String, CodingKey {
            case server, droppedOldServer, autoBackup, checkBeforeLaunch, retention, matches, synced, deviceID
        }

        private enum OldServerKeys: String, CodingKey { case ssh }

        init(from decoder: any Decoder) throws {
            let values = try decoder.container(keyedBy: CodingKeys.self)
            // VNDegen 0.4.0 reached the store over SSH. Those settings leave save sync off until the user enters the share.
            if let old = try? values.nestedContainer(keyedBy: OldServerKeys.self, forKey: .server), old.contains(.ssh) {
                droppedOldServer = true
                migrating = true
            } else {
                server = try values.decodeIfPresent(SaveServer.self, forKey: .server)
                droppedOldServer = try values.decodeIfPresent(Bool.self, forKey: .droppedOldServer) ?? false
            }
            autoBackup = try values.decodeIfPresent(Bool.self, forKey: .autoBackup) ?? false
            checkBeforeLaunch = try values.decodeIfPresent(Bool.self, forKey: .checkBeforeLaunch) ?? true
            retention = try values.decodeIfPresent(Retention.self, forKey: .retention)
            matches = try values.decodeIfPresent([String: String].self, forKey: .matches) ?? [:]
            synced = try values.decodeIfPresent([String: SyncRecord].self, forKey: .synced) ?? [:]
            deviceID = try values.decodeIfPresent(String.self, forKey: .deviceID) ?? Settings.newDeviceID()
        }
    }

    struct SyncRecord: Codable, Equatable {
        var folder: String
        /// The latest backup on the server then. Records from before VNDegen kept the backup's time have none, which
        /// counts as a server change, so VNDegen asks rather than backs up over a backup it can't tell apart.
        var latest: BackupRevision?
        /// Each save's SHA-1, keyed `location:relative`.
        var saves: [String: String]
        /// The user kept the saves here over `latest`, so they count as changed until a backup records them.
        var keptLocal: Bool? = nil
    }

    var deviceID: String { settings.deviceID }

    private var mirror: URL { directory.appendingPathComponent("Store", isDirectory: true) }

    // MARK: Server

    /// The share's password as kept on this device. Nil for a guest login or when none is kept.
    public func storedPassword() -> String? {
        guard case let .smb(smb)? = settings.server?.remote, !smb.isGuest else { return nil }
        return try? SMBPassword.read(in: directory)
    }

    /// Saves the share's details, as Ludusavi's SMB remote and cloud path name them. A guest login keeps no password and
    /// deletes a kept one. Another host, port, or path forgets the copy of the old store.
    public func setServer(host: String, port: Int, guest: Bool, username: String, password: String, path: String) throws {
        let remote = SMBRemote(host: host.trimmingCharacters(in: .whitespaces), port: port,
                               username: guest ? SMBRemote.guest : username.trimmingCharacters(in: .whitespaces))
        let server = SaveServer(remote: .smb(remote), path: path.trimmingCharacters(in: .whitespaces))
        try SMBPassword.save(remote.isGuest ? nil : password, in: directory)
        let old = settings.server
        guard old != server else { return }
        settings.server = server
        if old.map(Self.store) != Self.store(server) {
            settings.synced = [:]
            try? FileManager.default.removeItem(at: mirror)
            storeGames = [:]
        }
        saveSettings()
        foundGames = nil
        connection = .notSetUp
    }

    public func forgetServer() {
        settings.server = nil
        settings.synced = [:]
        saveSettings()
        try? SMBPassword.save(nil, in: directory)
        try? FileManager.default.removeItem(at: mirror)
        storeGames = [:]
        foundGames = nil
        connection = .notSetUp
    }

    /// Where the store is, whatever the login.
    private static func store(_ server: SaveServer) -> String {
        switch server.remote {
        case let .smb(smb): "smb://\(smb.host):\(smb.port)/\(server.path)"
        }
    }

    /// Lists the store folder and counts its games. Reads only.
    public func testConnection() async {
        guard let server = settings.server else { connection = .notSetUp; return }
        connection = .checking
        do {
            foundGames = try await withStore(server) { store in
                try await store.countGames()
            }
            connection = .connected
        } catch {
            connection = .failed(error.localizedDescription)
        }
    }

    /// Reads the store's game list and Ludusavi's manifest when they are due. Run when a save screen appears.
    public func refresh() async {
        // The manifest download is 18 MB, so it runs on its own and never holds up the store.
        if let manifestStore, manifestRefresh == nil {
            manifestRefresh = Task {
                if let fresh = await manifestStore.refresh() { manifest = fresh }
                manifestRefresh = nil
            }
        }
        guard let server = settings.server else { return }
        do {
            try await refreshIndex(server)
            connection = .connected
        } catch {
            connection = .failed(error.localizedDescription)
        }
    }

    /// Returns once a manifest refresh that `refresh` started is done.
    func manifestRefreshed() async { await manifestRefresh?.value }

    private func refreshIndex(_ server: SaveServer) async throws {
        let index = try await withStore(server) { store in try await store.refreshIndex() }
        apply(index)
    }

    private func loadIndexFromMirror() {
        let folders = ((try? FileManager.default.contentsOfDirectory(atPath: mirror.path)) ?? []).filter {
            FileManager.default.fileExists(atPath: mirror.appendingPathComponent($0).appendingPathComponent(LudusaviStore.mappingFile).path)
        }
        apply(StoreIndex(mirror: LudusaviStore(root: mirror), folders: folders))
        Task {
            if let manifestStore { manifest = await manifestStore.current() }
        }
    }

    private func apply(_ index: StoreIndex) {
        storeGames = index.games
        steamLibrary = index.steamLibrary
    }

    // MARK: Games

    /// The Ludusavi game for a game: the user's pick; the store game with the Steam app ID's manifest title; a store
    /// game whose title matches; the manifest title of the Steam app ID; a manifest title that matches; the game's own.
    public func match(for id: GameID) -> GameMatch? {
        guard let game = library.game(id) else { return nil }
        let inStore = { (folder: String) in self.storeGames[folder] != nil }
        if let picked = settings.matches[id.rawValue] {
            return GameMatch(folder: picked, title: storeGames[picked] ?? picked, source: .picked, inStore: inStore(picked))
        }
        let titles = [game.title, id.rawValue, game.metadata?.title].compactMap { $0 }
        let steamIDs = (game.metadata?.refs ?? []).filter { $0.source == .steam }.compactMap { Int($0.id) }
        let manifestTitles = steamIDs.compactMap { id in manifest?.title(steamID: id).map { (id, $0) } }
        for (steamID, title) in manifestTitles {
            if let folder = storeFolder(titled: title) { return GameMatch(folder: folder, title: title, source: .steamID(steamID), inStore: true) }
        }
        for title in titles {
            if let folder = storeFolder(matching: title) {
                return GameMatch(folder: folder, title: storeGames[folder] ?? folder, source: .storeTitle, inStore: true)
            }
        }
        if let (steamID, title) = manifestTitles.first {
            return GameMatch(folder: LudusaviMapping.escapeFolderName(title), title: title, source: .steamID(steamID), inStore: false)
        }
        for title in titles {
            if let known = manifest?.title(matching: title) {
                let folder = LudusaviMapping.escapeFolderName(known)
                return GameMatch(folder: folder, title: known, source: .manifestTitle, inStore: inStore(folder))
            }
        }
        let folder = LudusaviMapping.escapeFolderName(game.title)
        return GameMatch(folder: folder, title: game.title, source: .gameTitle, inStore: inStore(folder))
    }

    private func storeFolder(titled title: String) -> String? {
        storeGames.first { $0.value == title }?.key ?? (storeGames[LudusaviMapping.escapeFolderName(title)] != nil ? LudusaviMapping.escapeFolderName(title) : nil)
    }

    /// Ludusavi's normalized titles first, then letters and digits only. Mapping titles before folder names.
    private func storeFolder(matching title: String) -> String? {
        let sorted = storeGames.sorted { $0.key < $1.key }
        for key in [LudusaviTitle.normalize, LudusaviTitle.letters] {
            let wanted = key(title)
            if let hit = sorted.first(where: { key($0.value) == wanted }) ?? sorted.first(where: { key($0.key) == wanted }) { return hit.key }
        }
        return nil
    }

    /// Picks the Ludusavi game folder by hand, or nil to match automatically.
    public func setMatch(_ folder: String?, for id: GameID) {
        settings.matches[id.rawValue] = folder
        settings.synced[id.rawValue] = nil
        saveSettings()
    }

    /// The game's backups on the server, oldest first. Reads the game's mapping.yaml from the server.
    public func backups(for id: GameID) async throws -> [BackupSummary] {
        guard let server = settings.server, let match = match(for: id) else { return [] }
        return try await withStore(server) { store in
            try await store.refreshGame(match.folder)
            return (try? store.local.mapping(match.folder).summaries) ?? []
        }
    }

    /// What restoring `backup` would change here. Reads only VNDegen's copy of the mapping, so call `backups` first.
    public func preview(_ id: GameID, backup: String) async throws -> RestorePreview {
        guard let match = match(for: id) else { throw SaveSyncError.noBackups }
        let locations = try saveLocations(id), store = LudusaviStore(root: mirror)
        return try await serialized { try store.preview(folder: match.folder, backup: backup, locations: locations) }
    }

    /// Restores `backup`, or the latest when nil. Brings the backup down from the server first.
    public func restore(_ id: GameID, backup: String?) async throws -> RestoreReport {
        guard let server = settings.server, let match = match(for: id) else { throw SaveSyncError.notSetUp }
        let locations = try saveLocations(id)
        let snapshot = snapshotURL(id)
        try FileManager.default.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
        let (report, synced) = try await withStore(server) { store in
            try await store.refreshGame(match.folder)
            try await store.fetch(match.folder, backup: backup)
            // A download that finished after a cancel still reports success, so the saves here change only if the
            // restore itself was not cancelled.
            try Task.checkCancellation()
            let report = try store.local.restore(folder: match.folder, backup: backup, locations: locations, snapshot: snapshot)
            let latest = try store.local.mapping(match.folder).summaries.last?.revision
            return (report, SyncRecord(folder: match.folder, latest: latest, saves: try LudusaviStore.localHashes(locations)))
        }
        record(synced, for: id)
        log.info("restored \(report.restored.count) saves of \(id.rawValue) from \(report.backup)")
        return report
    }

    public func canUndoRestore(_ id: GameID) -> Bool {
        LudusaviStore.isComplete(snapshot: snapshotURL(id))
    }

    public func undoRestore(_ id: GameID) async throws {
        let locations = try saveLocations(id)
        let snapshot = snapshotURL(id)
        try await serialized { try LudusaviStore.undoRestore(locations: locations, snapshot: snapshot) }
    }

    /// Adds a backup of the saves here to the server, on top of `judged`, the server state a status check saw. Refuses
    /// with `changedSinceCheck` when the server's mapping.yaml is anything else by now. The new backup folder goes up
    /// first and is checked, then mapping.yaml replaces the server's in one step, and only while the server's still holds
    /// exactly the bytes the backup started from. Reading it back afterwards reports a change that lands later.
    ///
    /// Retention drops old backups from mapping.yaml only. VNDegen never deletes a backup folder another device might
    /// still list; Ludusavi deletes unlisted `backup-*` folders at its next backup of the game.
    ///
    /// No lock is shared with Ludusavi or other devices. VNDegen deletes on the server only this device's own new folder
    /// and temporary mapping, and only before its mapping goes up.
    public func backUp(_ id: GameID, judged: ServerRevision) async throws -> BackupReport {
        guard let server = settings.server, let match = match(for: id), library.game(id) != nil else { throw SaveSyncError.notSetUp }
        let locations = try saveLocations(id)
        let retention = configRetention ?? manualRetention, os = Self.os, comment = Self.comment
        let pcFolders = pcFolders(id, match: match, locations: locations)
        let now = clock(), device = deviceID, afterLocalBackup = afterLocalBackup
        let (report, synced) = try await withStore(server) { store in
            let folder = match.folder
            let base = try await store.refreshGame(folder)
            guard judged.mapping == base else { throw SaveSyncError.changedSinceCheck }
            if try base.map({ try LudusaviMapping(yaml: String(decoding: $0, as: UTF8.self)) })?.backups.isEmpty == false {
                try await store.fetch(folder, backup: nil)
            }
            let mappingURL = store.mirror.appendingPathComponent(folder).appendingPathComponent(LudusaviStore.mappingFile)
            let report = try store.local.backUp(folder: folder, title: match.title, game: id, locations: locations, retention: retention,
                                                pcFolders: pcFolders, now: now, device: device, os: os, comment: comment)
            afterLocalBackup?()
            let saves = report.saves
            guard let name = report.backup else {
                let latest = try store.local.mapping(folder).summaries.last?.revision
                return (report, SyncRecord(folder: folder, latest: latest, saves: saves))
            }
            let pushed = try Data(contentsOf: mappingURL)
            // Back to the server's state in the mirror: the next read brings its mapping.yaml down again.
            let forget = {
                try? FileManager.default.removeItem(at: store.mirror.appendingPathComponent(folder).appendingPathComponent(name))
                if let base { try? base.write(to: mappingURL) } else { try? FileManager.default.removeItem(at: mappingURL) }
            }
            do {
                // A game's first backup creates its folder, so the store folder must exist already.
                if base == nil { _ = try await store.countGames() }
                try await store.push(folder, backup: name)
                try await store.verify(folder, backup: name, differential: report.isDifferential)
                try await store.stageMapping(folder, device: device)
            } catch {
                // Nothing names the new folder yet, and its name is this device's alone.
                try? await store.discard(folder, backup: name, device: device)
                forget()
                throw error
            }
            let replaced: Bool
            do {
                replaced = try await store.replaceMapping(folder, base: base, device: device)
            } catch {
                // VNDegen can't tell whether the rename happened, so it deletes nothing. Ludusavi prunes the folder if
                // nothing lists it, and the next commit replaces the temporary file.
                forget()
                if error is CancellationError { throw error }
                throw SaveSyncError.commitUnconfirmed(error.localizedDescription)
            }
            guard replaced else {
                try? await store.discard(folder, backup: name, device: device)
                forget()
                throw SaveSyncError.changedDuringBackup
            }
            guard try await store.readMapping(folder) == pushed else {
                log.error("the server's mapping of \(id.rawValue) changed right after backup \(name)")
                throw SaveSyncError.changedAfterBackup
            }
            let latest = try LudusaviMapping(yaml: String(decoding: pushed, as: UTF8.self)).summaries.last?.revision
            return (report, SyncRecord(folder: folder, latest: latest, saves: saves))
        }
        record(synced, for: id)
        if !match.inStore { storeGames[match.folder] = match.title }
        log.info("backed up \(id.rawValue): \(report.backup ?? "already current")")
        return report
    }

    /// What to tell the player before backing up a game's saves, such as when its PC version can't load them. Nil when
    /// there is nothing to say.
    public func backupWarning(_ id: GameID) -> String? {
        guard let game = library.game(id), let plugin = library.plugin(for: game.engine) else { return nil }
        return plugin.backupWarning(for: game, dataDirectory: library.paths.dataDirectory(for: id))
    }

    /// How the saves here compare with the server's latest backup. Reads the game's mapping.yaml from the server.
    public func status(for id: GameID) async throws -> SyncStatus {
        try await check(id).status
    }

    /// `status`, with the server state it judged, for a backup that must start from that state.
    public func check(_ id: GameID) async throws -> SyncCheck {
        guard let server = settings.server, let match = match(for: id) else { throw SaveSyncError.notSetUp }
        let locations = try saveLocations(id), previous = settings.synced[id.rawValue].flatMap { $0.folder == match.folder ? $0 : nil }
        let (check, inSync) = try await withStore(server) { store -> (SyncCheck, SyncRecord?) in
            let revision = ServerRevision(mapping: try await store.refreshGame(match.folder))
            let local = try LudusaviStore.localHashes(locations)
            guard let latest = try? store.local.latestHashes(folder: match.folder, locations: locations),
                  let summary = try store.local.mapping(match.folder).summaries.last else {
                return (SyncCheck(status: .noBackups, revision: revision), nil)
            }
            if latest.hashes == local {
                return (SyncCheck(status: .inSync, revision: revision), SyncRecord(folder: match.folder, latest: summary.revision, saves: local))
            }
            guard let previous else { return (SyncCheck(status: local.isEmpty ? .newerBackup(summary) : .conflict(summary), revision: revision), nil) }
            // Neither side changed since the last backup or restore here. The latest backup can still differ from the saves
            // here, such as a save it carries that this device never had, or an older backup restored on purpose.
            let localChanged = previous.keptLocal == true ? !local.isEmpty : local != previous.saves
            let status: SyncStatus = switch (previous.latest != summary.revision, localChanged) {
            case (true, false): .newerBackup(summary)
            case (true, true): .conflict(summary)
            case (false, true): .localChanged
            case (false, false): .inSync
            }
            return (SyncCheck(status: status, revision: revision), nil)
        }
        if let inSync { record(inSync, for: id) }
        return check
    }

    /// Before a game starts: a newer backup from another device, or a conflict, when there is one. Nil when there is
    /// nothing to ask, the check is off, or the server did not answer within a few seconds.
    public func launchOffer(_ id: GameID) async -> SyncStatus? {
        guard checkBeforeLaunch, settings.server != nil else { return nil }
        let check = Task { try await status(for: id) }
        switch await Self.value(of: check, within: timeouts.launchCheck) {
        case let offer?: switch offer {
            case .newerBackup, .conflict: return offer
            default: return nil
            }
        case nil:
            // Cancelling ends the check's rclone call at its next step, or a request already sent when its answer is in
            // or nothing arrives for the idle deadline. The operations queued behind it start once it has ended.
            check.cancel()
            log.notice("no save check for \(id.rawValue) before launch")
            return nil
        }
    }

    /// Keeps the saves here over the server's newer backup. The next backup records them on top of it.
    public func keepLocalSaves(_ id: GameID, over backup: BackupSummary) {
        guard let match = match(for: id) else { return }
        // The saves here count as changed until a backup records them, even when they did not change since this
        // device's last backup. A backup that finds nothing to add records the game as it is and ends that.
        let saves = settings.synced[id.rawValue].flatMap { $0.folder == match.folder ? $0.saves : nil } ?? [:]
        record(SyncRecord(folder: match.folder, latest: backup.revision, saves: saves, keptLocal: true), for: id)
    }

    /// Call when a game session ends. With automatic backup on, backs up saves that changed here, unless the server
    /// changed too. That conflict waits for the user at the next launch or on the game page. The backup refuses to run
    /// when the server changed after the check, which then waits for the user the same way.
    public func sessionEnded(_ id: GameID) async {
        guard autoBackup, settings.server != nil else { return }
        do {
            let judged = try await check(id)
            switch judged.status {
            case .localChanged, .noBackups: _ = try await backUp(id, judged: judged.revision)
            case .conflict: log.notice("not backing up \(id.rawValue): the server has a newer backup too")
            case .inSync, .newerBackup: break
            }
        } catch {
            log.error("automatic backup of \(id.rawValue) failed: \(error.localizedDescription)")
        }
    }

    /// Where the game keeps saves here, and where its latest backup recorded them.
    public func locations(for id: GameID) -> [SaveLocationInfo] {
        guard let locations = try? saveLocations(id) else { return [] }
        let latest = match(for: id).flatMap { try? LudusaviStore(root: mirror).mapping($0.folder).effectiveFiles() } ?? [:]
        let recorded = LudusaviStore.RecordedPaths(latest.keys, locations: locations)
        return locations.indices.map { SaveLocationInfo(device: locations[$0].directory.path, recorded: recorded.commonFolder[$0]) }
    }

    /// The manifest's save rules for the game's Ludusavi title.
    public func manifestRules(for id: GameID) -> [String] {
        match(for: id).flatMap { manifest?.game($0.title)?.saveRules } ?? []
    }

    /// Makes every operation started from now on wait for `work`, such as a game that is still quitting and writing its
    /// saves.
    public func hold(until work: Task<Void, Never>) {
        let previous = lastOperation
        lastOperation = Task {
            await previous?.value
            await work.value
        }
    }

    // MARK: Plumbing

    /// Runs `work` with the store, after every operation started before it. When `timeouts.operation` passes or the
    /// caller is cancelled, cancels the rclone call in progress. That call ends at rclone's next step, or a request
    /// already sent when its answer is in or nothing arrives for the idle deadline, and the operation ends with it. The
    /// operations after this one wait until then: a server that keeps trickling bytes can hold them longer.
    private func withStore<T: Sendable>(_ server: SaveServer, _ work: @escaping @Sendable (RemoteStore) async throws -> T) async throws -> T {
        let connector = connector, mirror = mirror, limit = timeouts.operation, limits = timeouts.rclone
        return try await serialized {
            let job = Task {
                try await work(RemoteStore(access: connector(server), mirror: mirror, limits: limits, path: server.path))
            }
            let deadline = Task {
                do { try await Task.sleep(for: limit) } catch { return false }
                job.cancel()
                return true
            }
            do {
                let result = try await withTaskCancellationHandler { try await job.value } onCancel: { job.cancel() }
                deadline.cancel()
                return result
            } catch {
                deadline.cancel()
                if await deadline.value { throw SaveSyncError.timedOut }
                throw error
            }
        }
    }

    /// Runs `work` after every operation started before it. Cancelling the caller cancels `work`, or skips it when it
    /// has not started yet.
    private func serialized<T: Sendable>(_ work: @escaping @Sendable () async throws -> T) async throws -> T {
        let previous = lastOperation
        let operation = Task.detached(priority: .userInitiated) {
            await previous?.value
            try Task.checkCancellation()
            return try await work()
        }
        lastOperation = Task { _ = try? await operation.value }
        return try await withTaskCancellationHandler {
            try await operation.value
        } onCancel: {
            operation.cancel()
        }
    }

    /// The task's value, or nil when `duration` passes first. The task keeps running.
    private static func value<T: Sendable>(of task: Task<T, any Error>, within duration: Duration) async -> T? {
        await withCheckedContinuation { continuation in
            let first = FirstResult(continuation)
            Task { first.resume(try? await task.value) }
            Task {
                try? await Task.sleep(for: duration)
                first.resume(nil)
            }
        }
    }

    private func record(_ record: SyncRecord, for id: GameID) {
        settings.synced[id.rawValue] = record
        saveSettings()
    }

    private func saveLocations(_ id: GameID) throws -> [SaveLocation] {
        guard let plugin = library.plugin(for: library.game(id)?.engine) else { throw SaveSyncError.engineUnsupported }
        let locations = plugin.saveLocations(folder: library.folder(for: id), dataDirectory: library.paths.dataDirectory(for: id))
        guard !locations.isEmpty else { throw SaveSyncError.engineUnsupported }
        return locations
    }

    /// For saves kept inside the game folder: the same folder in the PC's install, `<Steam library><install dir>/...`,
    /// where the Steam library is the one most backups in the store record and the install dir comes from the manifest.
    private func pcFolders(_ id: GameID, match: GameMatch, locations: [SaveLocation]) -> [Int: String] {
        guard let steamLibrary else { return [:] }
        let game = library.folder(for: id).resolvingSymlinksInPath().standardizedFileURL.pathComponents
        let install = manifest?.game(match.title)?.installDirs.first ?? id.rawValue
        var folders: [Int: String] = [:]
        for (index, location) in locations.enumerated() {
            let parts = location.directory.resolvingSymlinksInPath().standardizedFileURL.pathComponents
            guard parts.starts(with: game) else { continue }
            folders[index] = steamLibrary + ([install] + parts.dropFirst(game.count)).joined(separator: "/") + "/"
        }
        return folders
    }

    private func snapshotURL(_ id: GameID) -> URL {
        directory.appendingPathComponent("Snapshots/\(id.rawValue)", isDirectory: true)
    }

    private func saveSettings() {
        do {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            try JSONEncoder().encode(settings).write(to: directory.appendingPathComponent("settings.json"), options: .atomic)
        } catch {
            log.error("saving settings failed: \(error.localizedDescription)")
        }
    }

    /// Ludusavi's OS names. It has none for iOS, so iPhone and iPad backups say "other" and name the device in the comment.
    private static var os: String {
        #if os(macOS)
        "mac"
        #else
        "other"
        #endif
    }

    private static var comment: String {
        #if canImport(UIKit)
        "VNDegen on \(UIDevice.current.model)"
        #else
        "VNDegen on Mac"
        #endif
    }
}

/// Resumes a continuation with whichever result comes first.
private final class FirstResult<T: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<T?, Never>?

    init(_ continuation: CheckedContinuation<T?, Never>) { self.continuation = continuation }

    func resume(_ value: T?) {
        lock.withLock {
            continuation?.resume(returning: value)
            continuation = nil
        }
    }
}

/// The parts of Ludusavi's config.yaml that VNDegen uses.
struct LudusaviConfig {
    var retention: Retention?

    static func load() -> LudusaviConfig? {
        #if os(macOS)
        let url = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/ludusavi/config.yaml")
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return nil }
        return try? LudusaviConfig(yaml: text)
        #else
        return nil
        #endif
    }

    init(yaml text: String) throws {
        let backup = try YAML.parse(text)["backup"]
        if let full = backup?["retention"]?["full"]?.string.flatMap({ Int($0) }),
           let differential = backup?["retention"]?["differential"]?.string.flatMap({ Int($0) }) {
            retention = Retention(full: full, differential: differential)
        }
    }
}
