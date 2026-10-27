import Foundation

/// The app's answers to requests to quit, as AppKit asks for them. A running game ends first so its play time is
/// recorded and it writes its saves, then the app waits for automatic backups, including one still running from a game
/// closed just before. The wait and the timeout run separately, because a backup stuck on an unreachable share never
/// returns, cancelled or not.
@MainActor
public final class AppQuit {
    public enum Answer {
        case now, later, cancel
    }

    private enum Pending {
        case reply(() -> Void)
        case quitAgain(() -> Void)
    }

    private let timeout: Duration
    private let frameIsRunning: () -> Bool
    private let prepare: () async -> Void
    /// What finishes the quit in progress. AppKit takes exactly one reply to `.later`.
    private var pending: Pending?
    /// Set once a cancelled quit is done waiting, so the next request quits at once.
    private var ready = false

    /// `prepare` ends the game and waits for backups. `frameIsRunning` tells whether a game's frame is on the stack
    /// below the request.
    public init(timeout: Duration, frameIsRunning: @escaping () -> Bool,
                prepare: @escaping () async -> Void) {
        self.timeout = timeout
        self.frameIsRunning = frameIsRunning
        self.prepare = prepare
    }

    /// Answers a request to quit. After `.later`, `reply` runs once the app may quit. AppKit waits for that reply inside
    /// the call that asked, and the app exits from there. A request that arrives through a game's own dialog, whose loop
    /// runs the app's events inside a frame, would hold that frame open until the app exited, so the game could never
    /// close and write its saves. Such a request is cancelled instead, and `quitAgain` runs once the game has stopped and
    /// the backups are done. The request it makes is answered `.now`.
    ///
    /// A request `fromSystem`, made as macOS logs out, restarts, or shuts down, is never cancelled, since that would
    /// cancel the log out too. Inside a frame it waits there, and the game, which cannot close, writes its system saves
    /// where it waits.
    public func request(fromSystem: Bool, reply: @escaping () -> Void, quitAgain: @escaping () -> Void) -> Answer {
        if ready { return .now }
        switch pending {
        case .reply: return .later
        case .quitAgain:
            guard fromSystem else { return .cancel }
            pending = .reply(reply)
            return .later
        case nil: break
        }
        let cancels = !fromSystem && frameIsRunning()
        pending = cancels ? .quitAgain(quitAgain) : .reply(reply)
        Task {
            await prepare()
            finish()
        }
        Task {
            try? await Task.sleep(for: timeout)
            finish()
        }
        return cancels ? .cancel : .later
    }

    private func finish() {
        guard let pending else { return }
        self.pending = nil
        switch pending {
        case let .reply(reply):
            reply()
        case let .quitAgain(quitAgain):
            ready = true
            quitAgain()
        }
    }
}
