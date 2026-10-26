import Foundation
import Observation
import VNCore
import Metadata
import GameInput
import Lock
import SaveSync

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
    public var launchError: String?
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
    }

    /// Rescans the games folder, then looks up metadata for games that have none, without holding up the caller.
    public func refreshLibrary() async {
        await library.refresh()
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
        let previous = sessionEndWork
        sessionEndWork = Task {
            await previous?.value
            await saveSync.sessionEnded(id)
        }
    }

    /// Ends a running game the way quitting it from the hub would, then waits for every pending automatic backup. Call
    /// before the app quits, so the play time and the backups are not lost. Returns at once when nothing is pending.
    public func prepareToQuit() async {
        endSession()
        await sessionEndWork?.value
    }
}
