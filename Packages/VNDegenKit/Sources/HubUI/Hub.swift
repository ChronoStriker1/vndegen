import Foundation
import Observation
import VNCore
import Metadata
import GameInput
import Lock

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
    public private(set) var screen: Screen = .library
    public var launchError: String?
    @ObservationIgnored private var sessionStart: Date?
    @ObservationIgnored private var matching: Task<Void, Never>?

    public init(library: LibraryStore, metadata: MetadataService, input: InputHub, lock: LockService) {
        self.library = library
        self.metadata = metadata
        self.input = input
        self.lock = lock
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
        session.terminate()
        if let start = sessionStart {
            library.update(id) { $0.playTime += Date.now.timeIntervalSince(start) }
        }
        sessionStart = nil
        screen = .library
    }
}
