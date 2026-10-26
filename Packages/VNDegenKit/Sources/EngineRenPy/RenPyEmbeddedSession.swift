#if os(iOS)
import CRenPy
import UIKit
import VNCore

/// Ren'Py on iPhone and iPad runs inside VNDegen, because iOS has no helper processes. The app links Ren'Py 8's static
/// libraries and keeps one Python interpreter for its lifetime, since CPython can't be restarted in one process. Each
/// game runs on the main thread inside `vndegen_renpy_run`, whose event loop runs the main run loop, so the hub keeps
/// working while it plays. Ren'Py's SDL view is moved from SDL's own window into `view`, which keeps the hub's lock
/// screen and orientation lock over the game.
///
/// As on macOS, the game reads the keyboard, touches, and controllers itself and shows the VNDegen menu inside the game.
/// Quitting from that menu or the game's own Quit ends the session through `onExit`. When the hub ends the session
/// instead, the game quits the way its Quit button does once the player confirms, and `waitUntilStopped()` returns
/// once it has.
final class RenPyEmbeddedSession: EngineSession {
    let view: PlatformView
    private let container = RenPyContainerView()
    private let launch: GameLaunch
    private var running = true
    /// Set once the hub ends the session, so the game's end does not end it a second time.
    private var ending = false
    private var exitWaiters: [CheckedContinuation<Void, Never>] = []
    private var observers: [NSObjectProtocol] = []

    /// The session whose game runs in the interpreter. Only one game can, so a launch while another game still quits
    /// fails until it has.
    private static weak var active: RenPyEmbeddedSession?

    var drawsOwnMenu: Bool { true }

    init(_ launch: GameLaunch, layout: RenPyLayout) throws {
        guard let runtimes = RenPyCommand.bundledRuntimes, let support = RenPyCommand.support,
              FileManager.default.fileExists(atPath: runtimes.appendingPathComponent("8/renpy.py").path)
        else { throw RenPyError.runtimeMissing(.renPy8) }
        guard Self.active == nil else { throw RenPyError.busy }
        self.launch = launch
        view = container
        let base = launch.folder.appendingPathComponent(layout.path(""), isDirectory: true)
        try RenPyPlugin.importGameSaves(base: base, dataDirectory: launch.dataDirectory)
        let command = RenPyCommand(base: base, runtime: .renPy8, gameLine: layout.runtime, runtimes: runtimes, support: support,
                                   dataDirectory: launch.dataDirectory, placement: nil, buttons: launch.game.buttonMapping)
        let code = Self.code(launcher: command.arguments[0], base: base.path,
                             environment: command.environment.filter { $0.key.hasPrefix("VNDEGEN_") })
        let home = runtimes.appendingPathComponent(RenPyRuntime.renPy8.rawValue).path
        Self.active = self
        // The hub can pause or end the session before its game starts, so only a new session clears the last one's state.
        vndegen_renpy_set_paused(0)
        _ = vndegen_renpy_take_requests()
        vndegen_renpy_set_window_handler { window in
            guard let window else { return }
            let uiWindow = Unmanaged<UIWindow>.fromOpaque(window).takeUnretainedValue()
            MainActor.assumeIsolated { RenPyEmbeddedSession.active?.container.show(uiWindow) }
        }
        observeLifeCycle()
        // A run loop block, not a main-queue one, as vndegen_renpy_run requires. It also runs after the hub has put
        // the session's view on screen.
        CFRunLoopPerformBlock(CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue) { [weak self] in
            MainActor.assumeIsolated { self?.run(home: home, code: code) }
        }
        CFRunLoopWakeUp(CFRunLoopGetMain())
    }

    private func run(home: String, code: String) {
        renPyLog.info("starting \(self.launch.game.id.rawValue, privacy: .public) on Ren'Py 8 in process")
        RenPyBootRecord.started(dataDirectory: launch.dataDirectory, runtime: .renPy8)
        let status = Self.withOutput(to: Self.logFile(launch.dataDirectory)) { vndegen_renpy_run(home, code) }
        RenPyBootRecord.finished(dataDirectory: launch.dataDirectory, runtime: .renPy8)
        renPyLog.info("\(self.launch.game.id.rawValue, privacy: .public) ended with status \(status)")
        running = false
        Self.active = nil
        stopObserving()
        container.clear()
        let waiters = exitWaiters
        exitWaiters = []
        waiters.forEach { $0.resume() }
        if !ending { launch.onExit() }
    }

    /// The game reads its input itself. The hub's only input for it is its menu button, as a `.hub` press.
    func send(_ event: ControllerEvent) {
        if case .button(.hub, pressed: true) = event { vndegen_renpy_request(Int32(VNDEGEN_RENPY_OPEN_MENU)) }
    }

    func setPaused(_ paused: Bool) {
        vndegen_renpy_set_paused(paused ? 1 : 0)
    }

    /// The game autosaves and writes its persistent data as its own Quit button does. Unlike a helper process, it
    /// can't be killed if it doesn't quit.
    func terminate() {
        ending = true
        guard running else { return }
        vndegen_renpy_set_paused(0)
        vndegen_renpy_request(Int32(VNDEGEN_RENPY_QUIT))
    }

    func waitUntilStopped() async {
        guard running else { return }
        await withCheckedContinuation { exitWaiters.append($0) }
    }

    /// SDL learns about the app going to the background from its own app delegate, which VNDegen replaces.
    private func observeLifeCycle() {
        let states: [(Notification.Name, Int)] = [
            (UIApplication.willResignActiveNotification, VNDEGEN_RENPY_WILL_RESIGN_ACTIVE),
            (UIApplication.didEnterBackgroundNotification, VNDEGEN_RENPY_DID_ENTER_BACKGROUND),
            (UIApplication.willEnterForegroundNotification, VNDEGEN_RENPY_WILL_ENTER_FOREGROUND),
            (UIApplication.didBecomeActiveNotification, VNDEGEN_RENPY_DID_BECOME_ACTIVE),
        ]
        observers = states.map { name, state in
            NotificationCenter.default.addObserver(forName: name, object: nil, queue: nil) { _ in
                vndegen_renpy_app_state(Int32(state))
            }
        }
    }

    private func stopObserving() {
        observers.forEach(NotificationCenter.default.removeObserver)
        observers = []
    }

    /// Python that sets the launcher's environment and runs it, as the helper process does on macOS. The launcher
    /// reads `os.environ`, which Python copied at startup, so the variables go there rather than to `setenv`. Ren'Py's
    /// iOS startup sends standard output and error to the system log, and they go back to the file descriptors, which
    /// `withOutput` points at the game's log. On iOS Ren'Py's log objects hold the standard output from the start, and
    /// Ren'Py's module backup pickles them, so the streams pickle as a call that reopens them. The launcher runs as
    /// `__main__`, as it does in the helper process, because the backup finds the functions it defines by their
    /// module's name. JSON strings are valid Python string literals.
    static func code(launcher: String, base: String, environment: [String: String]) -> String {
        func json(_ value: some Encodable) -> String {
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
            // Strings and string collections always encode.
            return String(decoding: try! encoder.encode(value), as: UTF8.self)
        }
        return """
        import io, os, sys, traceback

        class LogStream(io.TextIOWrapper):
            def __reduce_ex__(self, protocol):
                return (log_stream, (self.fileno(),))

        def log_stream(fd):
            return LogStream(io.FileIO(fd, "w", closefd=False), encoding="utf-8", errors="replace", line_buffering=True)

        try:
            sys.stdout = log_stream(1)
            sys.stderr = log_stream(2)
            for key in [key for key in os.environ if key.startswith("VNDEGEN_")]:
                del os.environ[key]
            os.environ.update(\(json(environment)))
            sys.argv = \(json([launcher, base]))
            main = sys.modules["__main__"]
            main.__file__ = sys.argv[0]
            exec(compile(open(sys.argv[0]).read(), sys.argv[0], "exec"), main.__dict__)
        except SystemExit:
            pass
        except BaseException:
            os.write(2, traceback.format_exc().encode("utf-8", "replace"))
            raise
        """
    }

    /// Ren'Py's log, written fresh for each launch as on macOS. Its tracebacks land beside it in `logs/`.
    private static func logFile(_ dataDirectory: URL) -> URL {
        let logs = dataDirectory.appendingPathComponent("logs", isDirectory: true)
        try? FileManager.default.createDirectory(at: logs, withIntermediateDirectories: true)
        return logs.appendingPathComponent("renpy.txt")
    }

    /// Points the process's standard output and error at `file` while `body` runs. Ren'Py logs to them.
    private static func withOutput<T>(to file: URL, _ body: () -> T) -> T {
        let fd = open(file.path, O_WRONLY | O_CREAT | O_TRUNC, 0o644)
        guard fd >= 0 else { return body() }
        let saved = (dup(STDOUT_FILENO), dup(STDERR_FILENO))
        dup2(fd, STDOUT_FILENO)
        dup2(fd, STDERR_FILENO)
        close(fd)
        defer {
            dup2(saved.0, STDOUT_FILENO)
            dup2(saved.1, STDERR_FILENO)
            close(saved.0)
            close(saved.1)
        }
        return body()
    }
}

/// Holds Ren'Py's SDL view. SDL makes a window of its own for it, which has no scene and so never shows. The view moves
/// here, inside the hub's window, and its view controller keeps reporting size changes to SDL.
private final class RenPyContainerView: UIView {
    /// SDL's view controller, taken from SDL's window. The next game reuses SDL's window, which then has no root view
    /// controller, so the controller is kept for it.
    private static var sdlController: UIViewController?
    private weak var gameView: UIView?

    init() {
        super.init(frame: .zero)
        backgroundColor = .black
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    /// Runs after each display mode change. SDL puts its view controller back into its window when it makes a new view.
    func show(_ sdlWindow: UIWindow) {
        if let controller = sdlWindow.rootViewController {
            sdlWindow.rootViewController = nil
            Self.sdlController = controller
        }
        sdlWindow.isHidden = true
        guard let view = Self.sdlController?.view else { return }
        view.frame = bounds
        view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        addSubview(view)
        gameView = view
        renPyLog.info("Ren'Py view moved into the hub, \(Int(self.bounds.width))x\(Int(self.bounds.height))")
    }

    func clear() {
        gameView?.removeFromSuperview()
    }

    /// SDL sets the view's frame itself: to the screen's bounds, which follow the snapshots iOS takes in the background,
    /// and moved up while the on-screen keyboard would cover a text prompt. The view keeps the container's size and only
    /// SDL's upward move.
    override func layoutSubviews() {
        super.layoutSubviews()
        guard let gameView else { return }
        gameView.frame = bounds.offsetBy(dx: 0, dy: min(0, gameView.frame.minY))
    }
}
#endif
