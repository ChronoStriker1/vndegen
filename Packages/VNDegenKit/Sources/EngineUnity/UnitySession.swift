#if os(macOS)
import AppKit
import OSLog
import RegistryFile
import VNCore

let unityLog = Logger(subsystem: "VNDegen", category: "Unity")

/// A Unity game running outside VNDegen, in its own window: the Windows version through the player's CrossOver, or the
/// Mac version the game ships. VNDegen's overlay, controller mapping and cheats don't reach it. The hub window shows a
/// line saying so, with a button that brings the game back, and the session ends when the game quits, so play time and
/// the post-session save backup work as for any game.
@MainActor
class UnityExternalSession: EngineSession {
    let view: PlatformView
    let launch: GameLaunch
    let host: UnityHostView
    /// Set once the hub ends the session, so the game's exit doesn't end it a second time.
    var ending = false
    private var work: Task<Void, Never>?

    /// How long a game gets to save and quit after the hub ends its session.
    static let quitTimeout: Duration = .seconds(10)

    init(_ launch: GameLaunch, message: String) {
        self.launch = launch
        host = UnityHostView(message: message)
        view = host
        host.onShowGame = { [weak self] in self?.showGame() }
        host.onBack = { [weak self] in self?.launch.onExit() }
    }

    /// Starts `body`, which runs the game to its end, and ends the session after it unless the hub ended it first.
    func start(_ body: @escaping @MainActor () async throws -> Void) {
        work = Task {
            do {
                try await body()
            } catch {
                unityLog.error("\(self.launch.game.id.rawValue, privacy: .public): \(error.localizedDescription, privacy: .public)")
                if !ending {
                    host.fail("\(launch.game.title) couldn't start. \(error.localizedDescription)")
                    return
                }
            }
            guard !ending else { return }
            showHub()
            launch.onExit()
        }
    }

    func showGame() {}

    private func showHub() {
        guard let window = host.window else { return }
        window.makeKeyAndOrderFront(nil)
        NSApp.activate()
    }

    func send(_ event: ControllerEvent) {
        switch event {
        case .button(_, pressed: true), .gamepad(_, pressed: true), .key(_, pressed: true), .click(_, pressed: true): showGame()
        case .button, .gamepad, .key, .click, .pointer, .scroll: break
        }
    }

    /// The game is its own app, with nothing of VNDegen's on screen to pause for.
    func setPaused(_ paused: Bool) {}

    func terminate() {
        ending = true
        stopGame()
    }

    func stopGame() {}

    func waitUntilStopped() async {
        await work?.value
    }
}

/// The Windows version, through the player's CrossOver, in VNDegen's own bottle.
final class UnityCrossOverSession: UnityExternalSession {
    private let bottle: UnityBottle
    private let exe: URL
    private let workdir: URL
    private let prefs: RegistrySave?
    private var game: Process?

    init(_ launch: GameLaunch, bottle: UnityBottle, exe: URL, prefs: RegistrySave?) {
        self.bottle = bottle
        self.exe = exe
        workdir = exe.deletingLastPathComponent()
        self.prefs = prefs
        super.init(launch, message: "Getting \(launch.game.title) ready in CrossOver…")
        start { [weak self] in try await self?.run() }
    }

    private var environment: [String: String] { ProcessInfo.processInfo.environment }

    private func run() async throws {
        let fm = FileManager.default
        if !fm.fileExists(atPath: bottle.readyMarker.path) {
            host.setMessage("Setting up CrossOver for Unity games. The first time takes a minute or two.")
            try await setUp()
        }
        try await Self.run(bottle.update(environment: environment), "updating the bottle")
        try bottle.harden()
        try bottle.installFonts()
        try await importPrefs()
        guard !ending else { return }
        host.setMessage("\(launch.game.title) is running in CrossOver, in its own window.", showsGame: true)
        let process = try Self.process(bottle.run(exe, workdir: workdir, environment: environment), log: logFile())
        let status = await withCheckedContinuation { (done: CheckedContinuation<Int32, Never>) in
            process.terminationHandler = { done.resume(returning: $0.terminationStatus) }
            do {
                try process.run()
                game = process
                unityLog.info("started \(self.launch.game.id.rawValue, privacy: .public) in CrossOver, pid \(process.processIdentifier)")
            } catch {
                process.terminationHandler = nil
                unityLog.error("starting CrossOver failed: \(error.localizedDescription, privacy: .public)")
                done.resume(returning: -1)
            }
        }
        game = nil
        unityLog.info("\(self.launch.game.id.rawValue, privacy: .public) exited with status \(status)")
        try await exportPrefs()
    }

    /// Makes the bottle once. A bottle without the ready marker is one an earlier setup didn't finish, which VNDegen made
    /// and may delete.
    private func setUp() async throws {
        try bottle.checkOwnBottle()
        let fm = FileManager.default
        if fm.fileExists(atPath: bottle.path.path) { try fm.removeItem(at: bottle.path) }
        try fm.createDirectory(at: bottle.folder, withIntermediateDirectories: true)
        try await Self.run(bottle.create(environment: environment), "making the bottle")
        try bottle.harden()
        try bottle.installFonts()
        try Data().write(to: bottle.readyMarker)
    }

    /// Puts the game's PlayerPrefs from its file into the bottle: its key is deleted first, then written from the file,
    /// so no value the file lacks stays behind and no other key changes. A file holding any other key is not imported.
    private func importPrefs() async throws {
        guard let prefs, let data = try? Data(contentsOf: prefs.file) else { return }
        let file = try RegistryData(reg: data)
        guard file.keys.count == 1, let values = file.values(of: prefs.key) else {
            unityLog.error("not importing \(prefs.file.path, privacy: .public): it holds another key")
            return
        }
        let text = Self.importText(key: prefs.key, values: values)
        let staged = launch.dataDirectory.appendingPathComponent("PlayerPrefs-import.reg")
        try RegistryData.utf16File(text).write(to: staged, options: .atomic)
        defer { try? FileManager.default.removeItem(at: staged) }
        try await Self.run(bottle.importRegistry(staged, environment: environment), "applying the game's settings")
    }

    /// `[-key]`, then `key` with `values`.
    static func importText(key: String, values: [String: RegistryValue]) -> String {
        var data = RegistryData()
        data.setValues(values, of: key)
        let written = data.regText
        let header = RegistryData.regedit5Header + "\n\n"
        return header + "[-\(key)]\n\n" + written.dropFirst(header.count)
    }

    /// Reads the game's key back out of the bottle after it quits and keeps its own values, not its subkeys', in the
    /// file save sync backs up. Nothing changes when the game wrote none.
    private func exportPrefs() async throws {
        guard let prefs else { return }
        let staged = launch.dataDirectory.appendingPathComponent("PlayerPrefs-export.reg")
        defer { try? FileManager.default.removeItem(at: staged) }
        try? FileManager.default.removeItem(at: staged)
        // regedit fails when the key doesn't exist, which is a game that never saved its PlayerPrefs.
        guard (try? await Self.run(bottle.exportRegistry(prefs.key, to: staged, environment: environment), "reading the game's settings")) != nil,
              let values = try RegistryData(reg: Data(contentsOf: staged)).values(of: prefs.key)
        else { return }
        var file = RegistryData()
        file.setValues(values, of: prefs.key)
        try FileManager.default.createDirectory(at: prefs.file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(file.regText.utf8).write(to: prefs.file, options: .atomic)
    }

    override func showGame() {
        guard let game, game.isRunning, let app = Self.gameApp(under: game.processIdentifier) else { return }
        NSApp.yieldActivation(to: app)
        app.activate()
    }

    /// Closes the game as its close button does, then stops every Wine process in the bottle if it's still running after
    /// `quitTimeout`.
    override func stopGame() {
        guard let game, game.isRunning else { return }
        let exeName = exe.lastPathComponent, bottle = bottle, environment = environment
        Task {
            try? await Self.run(bottle.close(exeName, environment: environment), "closing the game")
            try? await Task.sleep(for: Self.quitTimeout)
            guard game.isRunning else { return }
            unityLog.error("\(self.launch.game.id.rawValue, privacy: .public) didn't quit in time, stopping its bottle")
            try? await Self.run(bottle.killAll(environment: environment), "stopping the bottle")
        }
    }

    // MARK: Processes

    enum Failure: Error, LocalizedError {
        case tool(String, Int32)

        var errorDescription: String? {
            switch self {
            case let .tool(step, status): "CrossOver stopped while \(step) (status \(status))."
            }
        }
    }

    static func process(_ command: UnityBottle.Command, log: FileHandle? = nil) -> Process {
        let command = sandboxed(command)
        let process = Process()
        process.executableURL = command.executable
        process.arguments = command.arguments
        process.environment = command.environment
        process.standardInput = FileHandle.nullDevice
        process.standardOutput = log ?? FileHandle.nullDevice
        process.standardError = log ?? FileHandle.nullDevice
        return process
    }

    /// Runs one CrossOver tool to its end, throwing unless it succeeds.
    static func run(_ command: UnityBottle.Command, _ step: String) async throws {
        let process = process(command)
        let status = try await withCheckedThrowingContinuation { (done: CheckedContinuation<Int32, any Error>) in
            process.terminationHandler = { done.resume(returning: $0.terminationStatus) }
            do {
                try process.run()
            } catch {
                process.terminationHandler = nil
                done.resume(throwing: error)
            }
        }
        guard status == 0 else { throw Failure.tool(step, status) }
    }

    /// In DEBUG with `-unityNoNetwork YES`, every CrossOver process runs with IP connections blocked, so a test never
    /// lets a game reach the network. The shipped app runs the game as a PC would.
    static func sandboxed(_ command: UnityBottle.Command) -> UnityBottle.Command {
        #if DEBUG
        guard UserDefaults.standard.bool(forKey: "unityNoNetwork") else { return command }
        let profile = "(version 1)(allow default)(deny network-outbound (remote ip))(deny network-bind (local ip))"
        return .init(executable: URL(fileURLWithPath: "/usr/bin/sandbox-exec"),
                     arguments: ["-p", profile, command.executable.path] + command.arguments, environment: command.environment)
        #else
        return command
        #endif
    }

    /// CrossOver's log for this launch, written fresh each time beside the game's other data.
    private func logFile() -> FileHandle? {
        let logs = launch.dataDirectory.appendingPathComponent("logs", isDirectory: true)
        let url = logs.appendingPathComponent("crossover.txt")
        try? FileManager.default.createDirectory(at: logs, withIntermediateDirectories: true)
        guard FileManager.default.createFile(atPath: url.path, contents: nil) else { return nil }
        return try? FileHandle(forWritingTo: url)
    }

    /// The app Wine made for the game's window: a running app started below `pid`, the `wine` script VNDegen ran.
    static func gameApp(under pid: pid_t) -> NSRunningApplication? {
        NSWorkspace.shared.runningApplications.first { app in
            var current = app.processIdentifier
            for _ in 0..<16 {
                guard current > 1 else { return false }
                if current == pid { return true }
                current = parent(of: current)
            }
            return false
        }
    }

    private static func parent(of pid: pid_t) -> pid_t {
        var info = kinfo_proc()
        var size = MemoryLayout<kinfo_proc>.stride
        var name = [CTL_KERN, KERN_PROC, KERN_PROC_PID, pid]
        guard sysctl(&name, 4, &info, &size, nil, 0) == 0, size > 0 else { return 0 }
        return info.kp_eproc.e_ppid
    }
}

/// The Mac version a game ships, such as SiNiSistar 2's, opened as its own app.
final class UnityMacAppSession: UnityExternalSession {
    private let app: URL
    private var running: NSRunningApplication?

    init(_ launch: GameLaunch, app: URL) {
        self.app = app
        super.init(launch, message: "Opening \(app.deletingPathExtension().lastPathComponent)…")
        start { [weak self] in try await self?.run() }
    }

    private func run() async throws {
        let configuration = NSWorkspace.OpenConfiguration()
        configuration.createsNewApplicationInstance = true
        configuration.activates = true
        let running = try await NSWorkspace.shared.openApplication(at: app, configuration: configuration)
        self.running = running
        host.setMessage("\(launch.game.title) is running in its own Mac version.", showsGame: true)
        unityLog.info("opened \(self.app.lastPathComponent, privacy: .public), pid \(running.processIdentifier)")
        guard !running.isTerminated else { return }
        let center = NSWorkspace.shared.notificationCenter
        let pid = running.processIdentifier
        let stream = center.notifications(named: NSWorkspace.didTerminateApplicationNotification)
        for await note in stream {
            let ended = note.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication
            if ended?.processIdentifier == pid { break }
        }
        unityLog.info("\(self.app.lastPathComponent, privacy: .public) quit")
    }

    override func showGame() {
        guard let running, !running.isTerminated else { return }
        NSApp.yieldActivation(to: running)
        running.activate()
    }

    /// Quits the app as its Quit command does, and forces it after `quitTimeout`.
    override func stopGame() {
        guard let running, !running.isTerminated else { return }
        running.terminate()
        Task {
            try? await Task.sleep(for: Self.quitTimeout)
            if !running.isTerminated { running.forceTerminate() }
        }
    }
}

/// What the hub window shows while its game runs in its own window: a line saying so and a button that brings the game
/// back, or, when it couldn't start, why, with a button back to the library.
final class UnityHostView: NSView {
    var onShowGame: (() -> Void)?
    var onBack: (() -> Void)?
    private let label: NSTextField
    private let showButton: NSButton
    private let backButton: NSButton

    init(message: String) {
        label = NSTextField(wrappingLabelWithString: message)
        showButton = NSButton(title: "Show game", target: nil, action: nil)
        backButton = NSButton(title: "Back to Library", target: nil, action: nil)
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = NSColor.black.cgColor
        label.alignment = .center
        label.textColor = .secondaryLabelColor
        label.font = .systemFont(ofSize: 15, weight: .semibold)
        for button in [showButton, backButton] {
            button.bezelStyle = .push
            button.target = self
        }
        showButton.action = #selector(showGame)
        backButton.action = #selector(back)
        showButton.isHidden = true
        backButton.isHidden = true
        let stack = NSStackView(views: [label, showButton, backButton])
        stack.orientation = .vertical
        stack.spacing = 14
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)
        NSLayoutConstraint.activate([
            stack.centerXAnchor.constraint(equalTo: centerXAnchor),
            stack.centerYAnchor.constraint(equalTo: centerYAnchor),
            stack.widthAnchor.constraint(lessThanOrEqualTo: widthAnchor, constant: -48),
            label.widthAnchor.constraint(lessThanOrEqualToConstant: 520),
        ])
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    func setMessage(_ message: String, showsGame: Bool = false) {
        label.stringValue = message
        showButton.isHidden = !showsGame
        backButton.isHidden = true
    }

    func fail(_ message: String) {
        label.stringValue = message
        label.textColor = .labelColor
        showButton.isHidden = true
        backButton.isHidden = false
    }

    @objc private func showGame() { onShowGame?() }
    @objc private func back() { onBack?() }
}
#endif
