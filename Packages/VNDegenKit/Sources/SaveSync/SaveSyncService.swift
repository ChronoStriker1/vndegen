import Foundation
import Observation
import OSLog
import VNCore
#if canImport(UIKit)
import UIKit
#endif

private let log = Logger(subsystem: "com.chronostriker1.vndegen", category: "savesync")

/// Restores and backs up game saves through the user's Ludusavi backup folder, so saves move between VNDegen and the
/// PC copies Ludusavi already manages.
@MainActor
@Observable
public final class SaveSyncService {
    public enum FolderState: Equatable {
        case none
        case ready(URL)
        /// The bookmark no longer resolves, or the folder is on a server that is not connected.
        case unavailable(String)
    }

    public private(set) var folder: FolderState = .none
    /// Ludusavi's own backup folder from its config. macOS only.
    public private(set) var suggestedFolder: URL?
    public private(set) var gameFolders: [String] = []
    public var autoBackup: Bool {
        get { settings.autoBackup }
        set { settings.autoBackup = newValue; saveSettings() }
    }
    /// Ludusavi's retention limits from its config. Nil where VNDegen can't read the config, such as on iPhone.
    public let configRetention: Retention?
    /// The user's Ludusavi retention limits, entered by hand where the config can't be read. Nil infers them from the
    /// backups already in the folder.
    public var manualRetention: Retention? {
        get { settings.retention }
        set { settings.retention = newValue; saveSettings() }
    }

    @ObservationIgnored private let library: LibraryStore
    @ObservationIgnored private let directory: URL
    private var settings: Settings
    /// The folder whose security scope is open, so it can be closed when the user picks another.
    @ObservationIgnored private var accessed: URL?
    /// The last restore, undo, or backup started. Each new one waits for it, so two never write the same folders at once.
    @ObservationIgnored private var lastOperation: Task<Void, Never>?

    public init(library: LibraryStore) {
        self.library = library
        directory = library.paths.support.appendingPathComponent("SaveSync", isDirectory: true)
        settings = (try? JSONDecoder().decode(Settings.self, from: Data(contentsOf: directory.appendingPathComponent("settings.json")))) ?? Settings()
        let config = LudusaviConfig.load()
        suggestedFolder = config?.backupPath
        configRetention = config?.retention
        resolveFolder()
    }

    private struct Settings: Codable {
        var bookmark: Data?
        var autoBackup = false
        var retention: Retention?
        /// Ludusavi game folder per game, when the user picked one by hand.
        var matches: [String: String] = [:]
    }

    private var store: LudusaviStore? {
        if case let .ready(url) = folder { LudusaviStore(root: url) } else { nil }
    }

    // MARK: Folder

    /// Remembers a folder picked in the document picker or the open panel.
    public func chooseFolder(_ url: URL) {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        do {
            #if os(iOS)
            settings.bookmark = try url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)
            #else
            settings.bookmark = try url.bookmarkData()
            #endif
        } catch {
            folder = .unavailable("Can't keep access to \(url.lastPathComponent): \(error.localizedDescription)")
            return
        }
        saveSettings()
        resolveFolder()
    }

    public func forgetFolder() {
        settings.bookmark = nil
        saveSettings()
        resolveFolder()
    }

    /// Lists the folder's games. Run when a save screen appears, so a server that came back is picked up.
    public func refresh() async {
        if case .unavailable = folder { resolveFolder() }
        guard let store else { gameFolders = []; return }
        do {
            gameFolders = try await Task.detached(priority: .userInitiated) { try store.gameFolders() }.value
        } catch {
            gameFolders = []
            folder = .unavailable(error.localizedDescription)
        }
    }

    private func resolveFolder() {
        accessed?.stopAccessingSecurityScopedResource()
        accessed = nil
        guard let bookmark = settings.bookmark else { folder = .none; gameFolders = []; return }
        var stale = false
        guard let url = try? URL(resolvingBookmarkData: bookmark, bookmarkDataIsStale: &stale) else {
            folder = .unavailable("The backup folder can't be found. If it is on a server, connect to it in the Files app. Otherwise choose the folder again.")
            return
        }
        if url.startAccessingSecurityScopedResource() { accessed = url }
        if stale, let fresh = try? url.bookmarkData() {
            settings.bookmark = fresh
            saveSettings()
        }
        var isDirectory: ObjCBool = false
        folder = FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory) && isDirectory.boolValue
            ? .ready(url)
            : .unavailable(SaveSyncError.folderUnreachable(url.path).localizedDescription)
    }

    // MARK: Games

    /// The Ludusavi game folder for a game: the user's pick, else the folder whose name matches the game's title.
    public func match(for id: GameID) -> String? {
        if let picked = settings.matches[id.rawValue], gameFolders.isEmpty || gameFolders.contains(picked) { return picked }
        guard let game = library.game(id) else { return nil }
        return LudusaviStore.match([game.title, id.rawValue, game.metadata?.title].compactMap { $0 }, in: gameFolders)
    }

    public func setMatch(_ folder: String?, for id: GameID) {
        settings.matches[id.rawValue] = folder
        saveSettings()
    }

    public func backups(for id: GameID) async throws -> [BackupSummary] {
        guard let store, let folder = match(for: id) else { return [] }
        return try await Task.detached(priority: .userInitiated) { try store.mapping(folder).summaries }.value
    }

    /// Restores `backup`, or the latest when nil.
    public func restore(_ id: GameID, backup: String?) async throws -> RestoreReport {
        guard let store, let folder = match(for: id) else { throw SaveSyncError.noBackups }
        let locations = try saveLocations(id)
        let snapshot = snapshotURL(id)
        try FileManager.default.createDirectory(at: snapshot.deletingLastPathComponent(), withIntermediateDirectories: true)
        let report = try await serialized {
            try store.restore(folder: folder, backup: backup, locations: locations, snapshot: snapshot)
        }
        log.info("restored \(report.restored.count) saves of \(id.rawValue) from \(report.backup)")
        return report
    }

    public func canUndoRestore(_ id: GameID) -> Bool {
        LudusaviStore.isComplete(snapshot: snapshotURL(id))
    }

    public func undoRestore(_ id: GameID) async throws {
        let locations = try saveLocations(id)
        let snapshot = snapshotURL(id)
        try await serialized {
            try LudusaviStore.undoRestore(locations: locations, snapshot: snapshot)
        }
    }

    public func backUp(_ id: GameID) async throws -> BackupReport {
        guard let store else { throw SaveSyncError.folderUnreachable("") }
        guard let game = library.game(id) else { throw SaveSyncError.noLocalSaves }
        let locations = try saveLocations(id)
        let folder = match(for: id), retention = configRetention ?? manualRetention, os = Self.os, comment = Self.comment
        let report = try await serialized {
            try store.backUp(folder: folder, title: game.title, game: id, locations: locations, retention: retention,
                             os: os, comment: comment)
        }
        if folder == nil, report.backup != nil {
            settings.matches[id.rawValue] = LudusaviMapping.escapeFolderName(game.title)
            saveSettings()
            await refresh()
        }
        log.info("backed up \(id.rawValue): \(report.backup ?? "already current")")
        return report
    }

    /// Call when a game session ends. Backs up when automatic backup is on and the game has a Ludusavi match.
    public func sessionEnded(_ id: GameID) async {
        guard autoBackup, store != nil else { return }
        if gameFolders.isEmpty { await refresh() }
        guard match(for: id) != nil else { return }
        do {
            _ = try await backUp(id)
        } catch {
            log.error("automatic backup of \(id.rawValue) failed: \(error.localizedDescription)")
        }
    }

    /// Runs `work` off the main thread after every operation started before it has finished.
    private func serialized<T: Sendable>(_ work: @escaping @Sendable () throws -> T) async throws -> T {
        let previous = lastOperation
        let operation = Task.detached(priority: .userInitiated) {
            await previous?.value
            return try work()
        }
        lastOperation = Task { _ = try? await operation.value }
        return try await operation.value
    }

    private func saveLocations(_ id: GameID) throws -> [SaveLocation] {
        guard let plugin = library.plugin(for: library.game(id)?.engine) else { throw SaveSyncError.engineUnsupported }
        let locations = plugin.saveLocations(folder: library.paths.folder(for: id), dataDirectory: library.paths.dataDirectory(for: id))
        guard !locations.isEmpty else { throw SaveSyncError.engineUnsupported }
        return locations
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

/// The parts of Ludusavi's config.yaml that VNDegen uses.
struct LudusaviConfig {
    var backupPath: URL?
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
        backupPath = backup?["path"]?.string.map { URL(fileURLWithPath: ($0 as NSString).expandingTildeInPath, isDirectory: true) }
        if let full = backup?["retention"]?["full"]?.string.flatMap({ Int($0) }),
           let differential = backup?["retention"]?["differential"]?.string.flatMap({ Int($0) }) {
            retention = Retention(full: full, differential: differential)
        }
    }
}
