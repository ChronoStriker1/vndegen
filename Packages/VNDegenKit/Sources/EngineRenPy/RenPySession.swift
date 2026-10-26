#if os(macOS)
import AppKit
import OSLog
import VNCore

/// Ren'Py runs as a helper process with its own SDL window, so a crash or `os._exit` in a game never takes down the hub,
/// and Ren'Py 7 and 8 games each get their own Python. The game window opens over the hub window and covers it exactly.
/// The hub window stays ordered in behind it. Ordering out the app's last window would quit the app, and when the game
/// quits, macOS gives the focus to the app whose window is then in front. Quitting from the game's VNDegen menu, or any other way
/// the process ends, brings the hub back and ends the session through `onExit`. When the hub ends the session instead,
/// the game quits the way its own Quit button does, and `waitUntilStopped()` returns once it has.
final class RenPySession: EngineSession {
    let view: PlatformView
    private let process = Process()
    private let launch: GameLaunch
    private let base: URL
    private let runtime: RenPyRuntime
    /// The runtime the game's own version calls for, which `runtime` overrides when the player picked another.
    private let gameLine: RenPyRuntime
    private let runtimes: URL
    private let support: URL
    private weak var hubWindow: NSWindow?
    private var spaceChange: (any NSObjectProtocol)?
    private var stopped = false
    /// Set once the hub ends the session, so the game's exit does not end it a second time.
    private var ending = false
    private var exitWaiters: [CheckedContinuation<Void, Never>] = []

    /// How long a game gets to autosave and quit after the hub ends its session. Ren'Py itself waits up to 3 seconds
    /// for an autosave on quit.
    static let quitTimeout: Duration = .seconds(10)

    init(_ launch: GameLaunch, layout: RenPyLayout, runtime: RenPyRuntime) throws {
        guard let runtimes = RenPyCommand.bundledRuntimes, let support = RenPyCommand.support,
              FileManager.default.isExecutableFile(atPath: RenPyCommand.python(runtime, in: runtimes).path)
        else { throw RenPyError.runtimeMissing(runtime) }
        self.launch = launch
        self.runtime = runtime
        gameLine = layout.runtime
        self.runtimes = runtimes
        self.support = support
        base = launch.folder.appendingPathComponent(layout.path(""), isDirectory: true)
        try RenPyPlugin.importGameSaves(base: base, dataDirectory: launch.dataDirectory)
        let host = RenPyHostView(message: "\(launch.game.title) is running in its own window.")
        view = host
        host.onWindow = { [weak self] window in self?.start(in: window) }
        host.onShowGame = { [weak self] in self?.showGame() }
    }

    /// Starts the game once the hub window that will host the placeholder is known, so the game window can take its place.
    private func start(in window: NSWindow) {
        hubWindow = window
        let placement: RenPyCommand.Placement = window.styleMask.contains(.fullScreen) ? .fullScreen : .frame(Self.gameFrame(over: window))
        let command = RenPyCommand(base: base, runtime: runtime, gameLine: gameLine, runtimes: runtimes, support: support,
                                   dataDirectory: launch.dataDirectory, placement: placement, buttons: launch.game.buttonMapping)
        process.executableURL = command.executable
        process.arguments = command.arguments
        process.environment = command.environment
        if let log = Self.logFile(launch.dataDirectory) {
            process.standardOutput = log
            process.standardError = log
        }
        process.terminationHandler = { [weak self] process in
            let status = process.terminationStatus
            Task { @MainActor in self?.ended(status: status) }
        }
        do {
            try process.run()
        } catch {
            renPyLog.error("starting \(self.runtime.displayName, privacy: .public) failed: \(error.localizedDescription, privacy: .public)")
            // Called from a view update, which must not change the hub's screen itself.
            Task { [onExit = launch.onExit] in onExit() }
            return
        }
        renPyLog.info("started \(self.launch.game.id.rawValue, privacy: .public) on \(self.runtime.displayName, privacy: .public), pid \(self.process.processIdentifier)")
    }

    private func ended(status: Int32) {
        renPyLog.info("\(self.launch.game.id.rawValue, privacy: .public) exited with status \(status)")
        let waiters = exitWaiters
        exitWaiters = []
        waiters.forEach { $0.resume() }
        guard !ending else { return }
        showHub()
        launch.onExit()
    }

    /// A full-screen game's Space is still closing when its process exits, and macOS then switches to a desktop Space.
    /// When "switch to a Space with open windows" is off, nothing brings a full-screen hub back from its own Space after
    /// that, so the hub comes forward again on the next Space change. The observer keeps the session alive until then,
    /// because the hub lets go of it as soon as it ends.
    private func showHub() {
        guard let hubWindow else { return }
        hubWindow.makeKeyAndOrderFront(nil)
        NSApp.activate()
        guard hubWindow.styleMask.contains(.fullScreen), !hubWindow.isOnActiveSpace, spaceChange == nil else { return }
        spaceChange = NSWorkspace.shared.notificationCenter.addObserver(
            forName: NSWorkspace.activeSpaceDidChangeNotification, object: nil, queue: .main
        ) { _ in
            MainActor.assumeIsolated { self.spaceChanged() }
        }
    }

    private func spaceChanged() {
        if let spaceChange { NSWorkspace.shared.notificationCenter.removeObserver(spaceChange) }
        spaceChange = nil
        guard let hubWindow, !hubWindow.isOnActiveSpace else { return }
        renPyLog.info("bringing the full-screen hub back to its Space")
        hubWindow.makeKeyAndOrderFront(nil)
        NSApp.activate()
    }

    private func showGame() {
        guard process.isRunning, let game = NSRunningApplication(processIdentifier: process.processIdentifier) else { return }
        NSApp.yieldActivation(to: game)
        game.activate()
    }

    /// The game window reads the keyboard, mouse, and controller itself, and `zz_vndegen.rpy` applies the game's button
    /// mapping there, keys and clicks included. The hub only gets input while VNDegen is in front of the game, so a press
    /// then brings the game window back.
    func send(_ event: ControllerEvent) {
        switch event {
        case .button(_, pressed: true), .gamepad(_, pressed: true), .key(_, pressed: true), .click(_, pressed: true): showGame()
        case .button, .gamepad, .key, .click, .pointer, .scroll: break
        }
    }

    /// Pauses only while VNDegen is in front, which is when the hub's own menu or lock screen covers the placeholder.
    /// While the game window is in front, the hub has nothing on screen to pause for. Resuming brings the game back.
    func setPaused(_ paused: Bool) {
        guard process.isRunning else { return }
        if paused, !stopped, NSApp.isActive {
            kill(process.processIdentifier, SIGSTOP)
            stopped = true
        } else if !paused, stopped {
            kill(process.processIdentifier, SIGCONT)
            stopped = false
            showGame()
        }
    }

    /// `zz_vndegen.rpy` answers SIGTERM by quitting as Ren'Py's Quit button does once the player confirms, so the game
    /// autosaves and writes its persistent data. A game still running after `quitTimeout` is killed. Ren'Py writes each
    /// save to a new file and renames it into place, so even that cannot leave a half-written save.
    func terminate() {
        ending = true
        guard process.isRunning else { return }
        let pid = process.processIdentifier
        if stopped {
            kill(pid, SIGCONT)
            stopped = false
        }
        kill(pid, SIGTERM)
        Task {
            try? await Task.sleep(for: Self.quitTimeout)
            guard process.isRunning else { return }
            renPyLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not quit in time, killing it")
            kill(pid, SIGKILL)
        }
    }

    func waitUntilStopped() async {
        guard process.isRunning else { return }
        await withCheckedContinuation { exitWaiters.append($0) }
    }

    /// The hub window's frame in SDL's coordinates.
    static func gameFrame(over window: NSWindow) -> CGRect {
        let top = NSScreen.screens.first?.frame.maxY ?? window.frame.maxY
        return CGRect(x: window.frame.minX, y: top - window.frame.maxY, width: window.frame.width, height: window.frame.height)
    }

    /// Ren'Py's log, written fresh for each launch. Its tracebacks land beside it in `logs/`.
    private static func logFile(_ dataDirectory: URL) -> FileHandle? {
        let logs = dataDirectory.appendingPathComponent("logs", isDirectory: true)
        let url = logs.appendingPathComponent("renpy.txt")
        try? FileManager.default.createDirectory(at: logs, withIntermediateDirectories: true)
        guard FileManager.default.createFile(atPath: url.path, contents: nil) else { return nil }
        return try? FileHandle(forWritingTo: url)
    }
}

/// What the hub window shows while its game runs in the helper's window: a line saying so and a button that brings the
/// game window back, for when the player switches to VNDegen mid-game.
private final class RenPyHostView: NSView {
    var onWindow: ((NSWindow) -> Void)?
    var onShowGame: (() -> Void)?

    init(message: String) {
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = NSColor.black.cgColor
        let label = NSTextField(labelWithString: message)
        label.alignment = .center
        label.textColor = .secondaryLabelColor
        label.font = .systemFont(ofSize: 15, weight: .semibold)
        let button = NSButton(title: "Show game", target: self, action: #selector(showGame))
        button.bezelStyle = .push
        let stack = NSStackView(views: [label, button])
        stack.orientation = .vertical
        stack.spacing = 14
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)
        NSLayoutConstraint.activate([
            stack.centerXAnchor.constraint(equalTo: centerXAnchor),
            stack.centerYAnchor.constraint(equalTo: centerYAnchor),
            stack.widthAnchor.constraint(lessThanOrEqualTo: widthAnchor, constant: -48),
        ])
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        guard let window, let onWindow else { return }
        self.onWindow = nil
        onWindow(window)
    }

    @objc private func showGame() { onShowGame?() }
}
#endif
