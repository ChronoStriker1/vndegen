import Foundation
import Observation
import OSLog
import Rsync
import VNCore
#if canImport(UIKit)
import UIKit
#endif

private let log = Logger(subsystem: "com.chronostriker1.vndegen", category: "savesync")

/// The server that holds the user's Ludusavi store, and where on it.
public struct SaveServer: Codable, Equatable, Sendable {
    public var ssh: SSHServer
    /// The store folder on the server, such as `/srv/ludusavi`.
    public var path: String
    /// The server's host key, pinned when the user trusted it. An OpenSSH public key line.
    public var hostKey: String?

    public init(ssh: SSHServer, path: String, hostKey: String? = nil) {
        self.ssh = ssh
        self.path = path
        self.hostKey = hostKey
    }
}

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

/// Backs up and restores game saves through the user's Ludusavi store on a server, over SSH and rsync, so saves move
/// between VNDegen and the PC copies that Ludusavi manages.
@MainActor
@Observable
public final class SaveSyncService {
    public enum Connection: Equatable {
        case notSetUp
        case checking
        /// The server presented a host key that is not pinned yet. The user checks its fingerprint and trusts it.
        case untrusted(hostKey: String)
        case connected
        case failed(String)
    }

    /// Opens a connection that runs rsync on the server. Tests replace it with a local rsync.
    public typealias Connector = @Sendable (SaveServer) async throws -> any RsyncConnection

    public var server: SaveServer? { settings.server }
    public private(set) var connection: Connection = .notSetUp
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
    /// Ludusavi's retention limits from its config on this Mac. Nil elsewhere.
    public let configRetention: Retention?
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

    struct Timeouts {
        var launchCheck: Duration = .seconds(6)
        var operation: Duration = .seconds(180)
    }

    /// Ludusavi's primary manifest, at the address Ludusavi itself reads it from.
    public static let ludusaviManifest = URL(string: "https://raw.githubusercontent.com/mtkennerly/ludusavi-manifest/master/data/manifest.yaml")!

    /// `manifest` nil leaves Ludusavi's manifest out, as tests do.
    public init(library: LibraryStore, connector: Connector? = nil, manifest: URL? = ludusaviManifest) {
        self.library = library
        let directory = library.paths.support.appendingPathComponent("SaveSync", isDirectory: true)
        self.directory = directory
        settings = (try? JSONDecoder().decode(Settings.self, from: Data(contentsOf: directory.appendingPathComponent("settings.json")))) ?? Settings()
        configRetention = LudusaviConfig.load()?.retention
        self.connector = connector ?? { server in
            try await SSHConnection.connect(to: server.ssh, key: SSHIdentity.key(in: directory), hostKey: server.hostKey)
        }
        manifestStore = manifest.map { ManifestStore(source: $0, directory: library.paths.support.appendingPathComponent("SaveSync", isDirectory: true)) }
        if settings.server?.hostKey != nil { connection = .connected }
        saveSettings()
        loadIndexFromMirror()
    }

    private struct Settings: Codable {
        var server: SaveServer?
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

        init(from decoder: any Decoder) throws {
            let values = try decoder.container(keyedBy: CodingKeys.self)
            server = try values.decodeIfPresent(SaveServer.self, forKey: .server)
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

    /// The line to add to the server account's `authorized_keys`.
    public func publicKey() throws -> String { try SSHIdentity.authorizedKeysLine(in: directory) }

    /// Saves the server details. A different server forgets the pinned host key and the copy of the old store.
    public func setServer(host: String, port: Int, user: String, path: String) {
        let ssh = SSHServer(host: host.trimmingCharacters(in: .whitespaces), port: port, user: user.trimmingCharacters(in: .whitespaces))
        let path = path.trimmingCharacters(in: .whitespaces)
        let old = settings.server
        guard old?.ssh != ssh || old?.path != path else { return }
        settings.server = SaveServer(ssh: ssh, path: path, hostKey: old?.ssh == ssh ? old?.hostKey : nil)
        settings.synced = [:]
        saveSettings()
        try? FileManager.default.removeItem(at: mirror)
        storeGames = [:]
        connection = .notSetUp
    }

    public func forgetServer() {
        settings.server = nil
        settings.synced = [:]
        saveSettings()
        try? FileManager.default.removeItem(at: mirror)
        storeGames = [:]
        connection = .notSetUp
    }

    /// Connects, and on success reads the store's game list. A host key that is not pinned yet ends in `.untrusted`.
    public func testConnection() async {
        guard let server = settings.server else { connection = .notSetUp; return }
        connection = .checking
        do {
            try await refreshIndex(server)
            connection = .connected
        } catch let SSHError.unknownHostKey(key) {
            connection = .untrusted(hostKey: key)
        } catch {
            connection = .failed(error.localizedDescription)
        }
    }

    /// Pins the host key the last test saw, then tests again.
    public func trustHostKey() async {
        guard case let .untrusted(key) = connection, settings.server != nil else { return }
        settings.server?.hostKey = key
        saveSettings()
        await testConnection()
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
        guard let server = settings.server, server.hostKey != nil else { return }
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
        let index = try await withStore(server) { store, client in try await store.refreshIndex(client) }
        apply(index)
    }

    private func loadIndexFromMirror() {
        let store = RemoteStore(path: ".", mirror: mirror)
        let folders = ((try? FileManager.default.contentsOfDirectory(atPath: mirror.path)) ?? []).filter {
            FileManager.default.fileExists(atPath: mirror.appendingPathComponent($0).appendingPathComponent(LudusaviStore.mappingFile).path)
        }
        apply(StoreIndex(mirror: store.local, folders: folders, root: nil))
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
        guard let server = trustedServer, let match = match(for: id) else { return [] }
        return try await withStore(server) { store, client in
            try await store.refreshGame(client, folder: match.folder)
            return (try? store.local.mapping(match.folder).summaries) ?? []
        }
    }

    /// What restoring `backup` would change here. Reads only VNDegen's copy of the mapping, so call `backups` first.
    public func preview(_ id: GameID, backup: String) async throws -> RestorePreview {
        guard let match = match(for: id) else { throw SaveSyncError.noBackups }
        let locations = try saveLocations(id), store = RemoteStore(path: ".", mirror: mirror).local
        return try await serialized { try store.preview(folder: match.folder, backup: backup, locations: locations) }
    }

    /// Restores `backup`, or the latest when nil. Brings the backup down from the server first.
    public func restore(_ id: GameID, backup: String?) async throws -> RestoreReport {
        guard let server = trustedServer, let match = match(for: id) else { throw SaveSyncError.notSetUp }
        let locations = try saveLocations(id)
        let snapshot = snapshotURL(id)
        try FileManager.default.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
        let (report, synced) = try await withStore(server) { store, client in
            try await store.refreshGame(client, folder: match.folder)
            try await store.fetch(client, folder: match.folder, backup: backup)
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
    /// first, then mapping.yaml, and only when the server's mapping.yaml still holds exactly the bytes the backup started
    /// from.
    ///
    /// Retention drops old backups from mapping.yaml only. VNDegen never deletes a backup folder another device might
    /// still list; Ludusavi deletes unlisted `backup-*` folders at its next backup of the game.
    ///
    /// No lock is shared with Ludusavi or other devices. The mapping upload is set up first and waits until rsync on the
    /// server asks for the file. Only then does a second channel read the server's mapping.yaml for the last time. As
    /// soon as its bytes are in and checked, before that read's closing statistics and exit, the new bytes go, or the
    /// file is skipped and the server's copy stays untouched. What remains is another device writing mapping.yaml during
    /// about one round trip: after the server sent the bytes of that last read and before rsync renames the new file
    /// into place. Reading the mapping back afterwards reports a change that lands later.
    public func backUp(_ id: GameID, judged: ServerRevision) async throws -> BackupReport {
        guard let server = trustedServer, let match = match(for: id), library.game(id) != nil else { throw SaveSyncError.notSetUp }
        let locations = try saveLocations(id)
        let retention = configRetention ?? manualRetention, os = Self.os, comment = Self.comment
        let pcFolders = pcFolders(id, match: match, locations: locations)
        let now = clock(), device = deviceID, afterLocalBackup = afterLocalBackup
        let (report, synced) = try await withStore(server) { store, client in
            let before = try await store.refreshGame(client, folder: match.folder)
            guard judged.mapping == before.mapping else { throw SaveSyncError.changedSinceCheck }
            let base = try before.mapping.map { try LudusaviMapping(yaml: String(decoding: $0, as: UTF8.self)) }
            if base?.backups.isEmpty == false {
                try await store.fetch(client, folder: match.folder, backup: nil)
            }
            let mappingURL = store.mirror.appendingPathComponent(match.folder).appendingPathComponent(LudusaviStore.mappingFile)
            let report = try store.local.backUp(folder: match.folder, title: match.title, game: id, locations: locations, retention: retention,
                                                pcFolders: pcFolders, now: now, device: device, os: os, comment: comment)
            afterLocalBackup?()
            let saves = report.saves
            guard let name = report.backup else {
                let latest = try store.local.mapping(match.folder).summaries.last?.revision
                return (report, SyncRecord(folder: match.folder, latest: latest, saves: saves))
            }
            let pushed = try Data(contentsOf: mappingURL)
            let comparison = Comparison()
            do {
                try await store.push(client, folder: match.folder, backup: name, like: before.entries)
                try await store.verify(client, folder: match.folder, backup: name, differential: report.isDifferential)
                let sent = try await store.pushMapping(client, folder: match.folder, like: before.entries) {
                    let (same, rest) = try await store.mappingIs(before.mapping, client, folder: match.folder)
                    comparison.decided(same, rest: rest)
                    return same
                }
                // The comparison's read ends after the decision. A failure there still fails the backup, without
                // undoing a mapping already sent.
                try await comparison.rest?.value
                guard sent else { throw SaveSyncError.changedDuringBackup }
            } catch {
                // Until mapping.yaml starts going up, nothing names the new folder, and its name is this device's alone.
                if !comparison.approved { try? await store.delete(client, folder: match.folder, names: [name]) }
                // Back to the server's state: the next read brings its mapping.yaml down again.
                try? FileManager.default.removeItem(at: store.mirror.appendingPathComponent(match.folder).appendingPathComponent(name))
                if let original = before.mapping { try? original.write(to: mappingURL) } else { try? FileManager.default.removeItem(at: mappingURL) }
                throw error
            }
            guard try await store.readGame(client, folder: match.folder).mapping == pushed else {
                log.error("the server's mapping of \(id.rawValue) changed right after backup \(name)")
                throw SaveSyncError.changedAfterBackup
            }
            let latest = try LudusaviMapping(yaml: String(decoding: pushed, as: UTF8.self)).summaries.last?.revision
            return (report, SyncRecord(folder: match.folder, latest: latest, saves: saves))
        }
        record(synced, for: id)
        if !match.inStore { storeGames[match.folder] = match.title }
        log.info("backed up \(id.rawValue): \(report.backup ?? "already current")")
        return report
    }

    /// How the saves here compare with the server's latest backup. Reads the game's mapping.yaml from the server.
    public func status(for id: GameID) async throws -> SyncStatus {
        try await check(id).status
    }

    /// `status`, with the server state it judged, for a backup that must start from that state.
    public func check(_ id: GameID) async throws -> SyncCheck {
        guard let server = trustedServer, let match = match(for: id) else { throw SaveSyncError.notSetUp }
        let locations = try saveLocations(id), previous = settings.synced[id.rawValue].flatMap { $0.folder == match.folder ? $0 : nil }
        let (check, inSync) = try await withStore(server) { store, client -> (SyncCheck, SyncRecord?) in
            let state = try await store.refreshGame(client, folder: match.folder)
            let revision = ServerRevision(mapping: state.mapping)
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
        guard checkBeforeLaunch, trustedServer != nil else { return nil }
        let check = Task { try await status(for: id) }
        switch await Self.value(of: check, within: timeouts.launchCheck) {
        case let offer?: switch offer {
            case .newerBackup, .conflict: return offer
            default: return nil
            }
        case nil:
            // Cancelling closes the check's connection, so the operations queued behind it don't wait for the server.
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
        guard autoBackup, trustedServer != nil else { return }
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
        let latest = match(for: id).flatMap { try? RemoteStore(path: ".", mirror: mirror).local.mapping($0.folder).effectiveFiles() } ?? [:]
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

    private var trustedServer: SaveServer? {
        settings.server.flatMap { $0.hostKey == nil ? nil : $0 }
    }

    /// Runs `work` with one connection to the server, after every operation started before it. When `timeouts.operation`
    /// passes or the caller is cancelled, cancels the connection attempt or closes the connection, which ends every read
    /// and write waiting on it, so a stalled server never holds up the operations after this one. The connector must
    /// end when cancelled, as `SSHConnection.connect` does.
    private func withStore<T: Sendable>(_ server: SaveServer, _ work: @escaping @Sendable (RemoteStore, RsyncClient) async throws -> T) async throws -> T {
        let store = RemoteStore(path: server.path, mirror: mirror), connector = connector, limit = timeouts.operation
        return try await serialized {
            let job = Task {
                let connection = try await connector(server)
                do {
                    let result = try await withTaskCancellationHandler {
                        try await work(store, RsyncClient(connection: connection))
                    } onCancel: {
                        Task { await connection.close() }
                    }
                    await connection.close()
                    return result
                } catch {
                    await connection.close()
                    throw error
                }
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
        let locations = plugin.saveLocations(folder: library.paths.folder(for: id), dataDirectory: library.paths.dataDirectory(for: id))
        guard !locations.isEmpty else { throw SaveSyncError.engineUnsupported }
        return locations
    }

    /// For saves kept inside the game folder: the same folder in the PC's install, `<Steam library><install dir>/...`,
    /// where the Steam library is the one most backups in the store record and the install dir comes from the manifest.
    private func pcFolders(_ id: GameID, match: GameMatch, locations: [SaveLocation]) -> [Int: String] {
        guard let steamLibrary else { return [:] }
        let game = library.paths.folder(for: id).resolvingSymlinksInPath().standardizedFileURL.pathComponents
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

/// The last mapping comparison of a backup, recorded by the upload's confirm closure: whether it let the mapping go,
/// and the rest of its read, which ends after the decision.
private final class Comparison: @unchecked Sendable {
    private let lock = NSLock()
    private var same = false
    private var remainder: Task<Void, any Error>?

    var approved: Bool { lock.withLock { same } }
    var rest: Task<Void, any Error>? { lock.withLock { remainder } }

    func decided(_ same: Bool, rest: Task<Void, any Error>) {
        lock.withLock { (self.same, remainder) = (same, rest) }
    }
}
