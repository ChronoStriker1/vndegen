#if os(macOS)
import AppKit
import OSLog
import VNCore

nonisolated let renPyLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "renpy")

/// Ren'Py runs as a helper process with its own SDL window, so a crash or `os._exit` in a game never takes down the hub,
/// and Ren'Py 7 and 8 games each get their own Python. The game window opens where the hub window is, and the hub window
/// hides while the game is in front. Quitting from the game's VNDegen menu, or any other way the process ends, brings
/// the hub back and ends the session through `onExit`.
final class RenPySession: EngineSession {
    let view: PlatformView
    private let process = Process()
    private let launch: GameLaunch
    private let base: URL
    private let runtime: RenPyRuntime
    private let runtimes: URL
    private let support: URL
    private weak var hubWindow: NSWindow?
    private var activation: (any NSObjectProtocol)?
    private var stopped = false

    init(_ launch: GameLaunch, layout: RenPyLayout, runtime: RenPyRuntime) throws {
        guard let runtimes = RenPyCommand.bundledRuntimes, let support = RenPyCommand.support,
              FileManager.default.isExecutableFile(atPath: RenPyCommand.python(runtime, in: runtimes).path)
        else { throw RenPyError.runtimeMissing(runtime) }
        self.launch = launch
        self.runtime = runtime
        self.runtimes = runtimes
        self.support = support
        base = launch.folder.appendingPathComponent(layout.path(""), isDirectory: true)
        let host = RenPyHostView(message: "\(launch.game.title) is running in its own window.")
        view = host
        host.onWindow = { [weak self] window in self?.start(in: window) }
        host.onShowGame = { [weak self] in self?.showGame() }
    }

    /// Starts the game once the hub window that will host the placeholder is known, so the game window can take its place.
    private func start(in window: NSWindow) {
        hubWindow = window
        RenPyPlugin.importGameSaves(base: base, dataDirectory: launch.dataDirectory)
        let placement: RenPyCommand.Placement = window.styleMask.contains(.fullScreen) ? .fullScreen : .frame(Self.gameFrame(over: window))
        let command = RenPyCommand(base: base, runtime: runtime, runtimes: runtimes, support: support,
                                   dataDirectory: launch.dataDirectory, placement: placement)
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
            renPyLog.error("starting \(self.runtime.displayName) failed: \(error.localizedDescription)")
            // Called from a view update, which must not change the hub's screen itself.
            Task { [onExit = launch.onExit] in onExit() }
            return
        }
        renPyLog.info("started \(self.launch.game.id.rawValue) on \(self.runtime.displayName), pid \(self.process.processIdentifier)")
        let pid = process.processIdentifier
        activation = NSWorkspace.shared.notificationCenter.addObserver(
            forName: NSWorkspace.didActivateApplicationNotification, object: nil, queue: .main
        ) { [weak self] note in
            let app = note.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication
            guard app?.processIdentifier == pid else { return }
            MainActor.assumeIsolated { self?.gameCameForward() }
        }
    }

    /// A full-screen hub stays in its own Space, and the full-screen game opens in another one in front of it.
    private func gameCameForward() {
        guard let hubWindow, !hubWindow.styleMask.contains(.fullScreen) else { return }
        hubWindow.orderOut(nil)
    }

    private func ended(status: Int32) {
        renPyLog.info("\(self.launch.game.id.rawValue) exited with status \(status)")
        stopObserving()
        showHub()
        launch.onExit()
    }

    private func showHub() {
        hubWindow?.makeKeyAndOrderFront(nil)
        NSApp.activate()
    }

    private func showGame() {
        guard process.isRunning, let game = NSRunningApplication(processIdentifier: process.processIdentifier) else { return }
        NSApp.yieldActivation(to: game)
        game.activate()
    }

    private func stopObserving() {
        if let activation { NSWorkspace.shared.notificationCenter.removeObserver(activation) }
        activation = nil
    }

    /// The game window receives keyboard, mouse, and controller input itself.
    func send(_ event: ControllerEvent) {}

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

    /// Ren'Py answers SIGTERM with its quit prompt, so the hub kills the process. Ren'Py writes each save to a new file
    /// and renames it into place, so this cannot leave a half-written save.
    func terminate() {
        process.terminationHandler = nil
        stopObserving()
        if process.isRunning { kill(process.processIdentifier, SIGKILL) }
        if hubWindow?.isVisible == false { showHub() }
    }

    /// The content area of a standard titled window whose frame is the hub window's frame, in SDL's coordinates.
    static func gameFrame(over window: NSWindow) -> CGRect {
        let content = NSWindow.contentRect(forFrameRect: window.frame, styleMask: [.titled, .closable, .miniaturizable, .resizable])
        let top = NSScreen.screens.first?.frame.maxY ?? content.maxY
        return CGRect(x: content.minX, y: top - content.maxY, width: content.width, height: content.height)
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
