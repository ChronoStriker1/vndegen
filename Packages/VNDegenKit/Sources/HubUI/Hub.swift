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
    public private(set) var screen: Screen = .library
    /// Archives copied into VNDegen's folder, listed under Ready to import.
    public private(set) var localArchives: [LocalArchive] = []
    public var launchError: String?
    /// Games whose last session is still quitting or backing up. One can't launch again until it is done, so two
    /// sessions never write the same saves. A backup gets at most `closingTimeout` once the game stopped.
    public private(set) var closing: Set<GameID> = []
    @ObservationIgnored private var sessionStart: Date?
    @ObservationIgnored private var matching: Task<Void, Never>?
    /// The automatic backups that closed sessions started, chained so the last one finishes after all the others.
    @ObservationIgnored private var sessionEndWork: Task<Void, Never>?

    public init(library: LibraryStore, metadata: MetadataService, input: InputHub, lock: LockService, saveSync: SaveSyncService) {
        self.library = library
        self.metadata = metadata
        self.input = input
        self.lock = lock
        self.saveSync = saveSync
        ControlMethod.migrateTouchControlsSetting()
    }

    /// Rescans the games folder and the archives beside it, then looks up metadata for games that have none, without
    /// holding up the caller.
    public func refreshLibrary() async {
        async let archives = LocalArchives.find(gamesRoot: library.paths.gamesRoot)
        await library.refresh()
        localArchives = await archives
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
        guard plugin.descriptor.platforms.contains(.current) else {
            launchError = "\(plugin.descriptor.displayName) does not run on this device yet."
            return
        }
        do {
            guard let launch = try library.makeLaunch(id, onExit: { [weak self] in self?.endSession() }) else { return }
            let session = try plugin.makeSession(launch)
            sessionStart = .now
            library.update(id) { $0.lastPlayedAt = .now }
            screen = .playing(id, session)
        } catch {
            launchError = "\(game.title) failed to start: \(error.localizedDescription)"
        }
    }

    public func endSession() {
        guard case let .playing(id, session) = screen else { return }
        // The player's input handler captures the session. The library attaches its own when it reappears.
        input.onEvent = nil
        session.terminate()
        if let start = sessionStart {
            library.update(id) { $0.playTime += Date.now.timeIntervalSince(start) }
        }
        sessionStart = nil
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

    /// Ends a running game the way quitting it from the hub would, then waits for every pending automatic backup. Call
    /// before the app quits, so the play time and the backups are not lost. Returns at once when nothing is pending.
    public func prepareToQuit() async {
        endSession()
        await sessionEndWork?.value
    }
}
