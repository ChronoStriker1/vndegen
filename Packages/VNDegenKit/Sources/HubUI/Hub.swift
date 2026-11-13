import Foundation
import Observation
import VNCore
import Metadata
import GameInput
import Lock
import SaveSync
import ArchiveImport

/// App-level state: what is on screen and the services every screen shares.
@MainActor
@Observable
public final class Hub {
    public enum Screen {
        case library
        case playing(GameID, any EngineSession)
    }

    public let library: LibraryStore
    public let metadata: MetadataService
    public let input: InputHub
    public let lock: LockService
    public let saveSync: SaveSyncService
    public let layouts: LayoutLibrary
    public private(set) var screen: Screen = .library
    /// Archives copied into VNDegen's folder, listed under Ready to import.
    public private(set) var localArchives: [LocalArchive] = []
    public var launchError: String?
    /// Games whose last session is still quitting or backing up. One can't launch again until it is done, so two
    /// sessions never write the same saves. A backup gets at most `closingTimeout` once the game stopped.
    public private(set) var closing: Set<GameID> = []
    @ObservationIgnored private var sessionStart: Date?
    /// The running game's last played time before this launch, put back if the game fails to start.
    @ObservationIgnored private var lastPlayedBefore: Date?
    @ObservationIgnored private var matching: Task<Void, Never>?
    @ObservationIgnored private var ownFolders: [String]?
    /// The automatic backups that closed sessions started, chained so the last one finishes after all the others.
    @ObservationIgnored private var sessionEndWork: Task<Void, Never>?

    public init(library: LibraryStore, metadata: MetadataService, input: InputHub, lock: LockService, saveSync: SaveSyncService) {
        self.library = library
        self.metadata = metadata
        self.input = input
        self.lock = lock
        self.saveSync = saveSync
        layouts = LayoutLibrary(paths: library.paths)
        ControlMethod.migrateTouchControlsSetting()
        ButtonMapping.convertLegacyMappings(in: library)
        ButtonMapping.moveOldDpadOrder(in: library)
    }

    /// Rescans the games folder and the archives beside it, then looks up metadata for games that have none, without
    /// holding up the caller.
    public func refreshLibrary() async {
        ownFolders = await LocalArchives.listing(gamesRoot: library.paths.gamesRoot)
        async let archives = LocalArchives.find(gamesRoot: library.paths.gamesRoot)
        await library.refresh()
        localArchives = await archives
        matchMetadata()
    }

    public func returnToApp(fromBackground: Bool) async {
        if !fromBackground, await LocalArchives.listing(gamesRoot: library.paths.gamesRoot) == ownFolders { return }
        await refreshLibrary()
    }

    /// Adds a games folder the player picked, then looks up metadata for its games.
    public func addLibraryFolder(_ url: URL) async throws {
        try await library.addFolder(url)
        matchMetadata()
    }

    /// Looks up metadata for games that have none, without holding up the caller.
    private func matchMetadata() {
        guard matching == nil else { return }
        let unmatched = library.games.filter { $0.metadata == nil }.map(\.id)
        guard !unmatched.isEmpty else { return }
        matching = Task {
            for id in unmatched { await metadata.autoMatch(id, in: library) }
            matching = nil
        }
    }

    public func launch(_ id: GameID) {
        guard case .library = screen, let game = library.game(id) else { return }
        guard !closing.contains(id) else {
            launchError = "\(game.title) is still closing. Try again in a moment."
            return
        }
        guard let plugin = library.plugin(for: game.engine) else {
            launchError = "No engine is set for \(game.title). Pick one on the game's page."
            return
        }
        if let problem = library.unavailableReason(id) {
            launchError = "\(game.title) is in \(folderName(of: game.libraryFolder)), which VNDegen can't reach. \(problem)"
            return
        }
        guard plugin.descriptor.platforms.contains(.current) else {
            launchError = plugin.descriptor.platforms.isEmpty
                ? "VNDegen can't run \(plugin.descriptor.displayName) games. The game's page says why."
                : "VNDegen can't run \(plugin.descriptor.displayName) games on this device. The game's page says why."
            return
        }
        layouts.prepareSuggestion(for: game, in: library)
        do {
            guard let launch = try library.makeLaunch(id, onExit: { [weak self] in self?.endSession() }) else { return }
            let session = try plugin.makeSession(launch)
            sessionStart = .now
            lastPlayedBefore = game.lastPlayedAt
            // Set at launch rather than at the end, so a game VNDegen quits or crashes in still counts as played.
            library.update(id) { $0.lastPlayedAt = .now }
            screen = .playing(id, session)
        } catch {
            launchError = "\(game.title) failed to start: \(error.localizedDescription)"
        }
    }

    /// The name of the library folder `folder`, nil for the built-in one.
    public func folderName(of folder: LibraryFolderID?) -> String {
        library.folders.first { $0.id == folder }?.name ?? library.paths.gamesRoot.lastPathComponent
    }

    public func endSession() {
        guard case let .playing(id, session) = screen else { return }
        let failedByItself = session.failedToStart
        // The player's input handler captures the session. The library attaches its own when it reappears.
        input.onEvent = nil
        session.terminate()
        if session.failedToStart {
            // A launch counts as played only once the game has started (#39).
            library.update(id) { [lastPlayedBefore] in $0.lastPlayedAt = lastPlayedBefore }
            if failedByItself, let game = library.game(id) {
                let title = game.isLocked && !lock.isUnlocked ? "The game" : game.title
                launchError = ["\(title) couldn't start.", session.startFailureReason.flatMap(Self.sentence)]
                    .compactMap(\.self).joined(separator: " ")
            }
        } else if let start = sessionStart {
            library.update(id) { $0.playTime += Date.now.timeIntervalSince(start) }
        }
        sessionStart = nil
        lastPlayedBefore = nil
        screen = .library
        closing.insert(id)
        let stopped = Task { await session.waitUntilStopped() }
        saveSync.hold(until: stopped)
        let previous = sessionEndWork
        let ended = Task {
            await previous?.value
            await stopped.value
            await saveSync.sessionEnded(id)
        }
        sessionEndWork = ended
        // The game can launch again once its backup is done, or `closingTimeout` after it stopped, so a backup stuck on
        // an unreachable share doesn't block the game until VNDegen restarts. A backup still waiting by then may read
        // saves the new session is writing, which is the lesser problem.
        Task {
            await stopped.value
            input.reclaimDevices()
            await Self.wait(for: ended, atMost: Self.closingTimeout)
            closing.remove(id)
        }
    }

    static func sentence(_ text: String) -> String? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let first = trimmed.first, let last = trimmed.last else { return nil }
        return first.uppercased() + trimmed.dropFirst() + (".!?".contains(last) ? "" : ".")
    }

    /// Returns when `work` finishes or `timeout` passes, whichever is first. A task group can't do this, because it
    /// waits for every child and awaiting a task's value ignores cancellation.
    static func wait(for work: Task<Void, Never>, atMost timeout: Duration) async {
        let (done, continuation) = AsyncStream<Void>.makeStream()
        let timer = Task { try? await Task.sleep(for: timeout); continuation.finish() }
        Task { await work.value; continuation.finish() }
        for await _ in done {}
        timer.cancel()
    }

    /// Longest a stopped game waits for its automatic backup before it can launch again.
    static let closingTimeout: Duration = .seconds(20)

    /// Ends a running game the way quitting it from the hub would, then waits for every pending automatic backup, at
    /// most `closingTimeout`. Call before the app quits, so the play time and the backups are not lost. Returns at once
    /// when nothing is pending. A backup cut off by the quit leaves no partial backup listed, because its folder goes
    /// up before the mapping that names it.
    public func prepareToQuit() async {
        endSession()
        if let sessionEndWork { await Self.wait(for: sessionEndWork, atMost: Self.closingTimeout) }
        await library.flush()
    }
}
