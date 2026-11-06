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

    /// The exe names, lowercased, of the games running in VNDegen's bottle. Wine's `taskkill` finds a game by its exe's
    /// name, so two games with the same one never run at once, and closing one can't close the other.
    static var runningExes: Set<String> = []

    init(_ launch: GameLaunch, bottle: UnityBottle, exe: URL, prefs: RegistrySave?) {
        self.bottle = bottle
        self.exe = exe
        Self.runningExes.insert(exe.lastPathComponent.lowercased())
        workdir = exe.deletingLastPathComponent()
        self.prefs = prefs
        super.init(launch, message: "Getting \(launch.game.title) ready in CrossOver…")
        start { [weak self] in try await self?.run() }
    }

    private var environment: [String: String] { ProcessInfo.processInfo.environment }

    private func run() async throws {
        defer { Self.runningExes.remove(exe.lastPathComponent.lowercased()) }
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
                Task { await self.bringGameForward() }
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

    /// Makes the bottle once. A bottle folder without the ready marker is one an earlier setup didn't finish, or only
    /// the saves a restore put there before the first launch. VNDegen made it and may remake it, but keeps the saves:
    /// they move aside while CrossOver makes the bottle, and back after.
    private func setUp() async throws {
        try bottle.checkOwnBottle()
        let fm = FileManager.default
        let saves = bottle.userFolder.appendingPathComponent("AppData/LocalLow", isDirectory: true)
        let aside = bottle.folder.appendingPathComponent("LocalLow-kept", isDirectory: true)
        // Merged, not moved, so a setup that stopped halfway, with saves in both places, loses none of them.
        if fm.fileExists(atPath: saves.path) {
            try bottle.checkInside(saves)
            try Self.merge(saves, into: aside)
        }
        if fm.fileExists(atPath: bottle.path.path) { try fm.removeItem(at: bottle.path) }
        try fm.createDirectory(at: bottle.folder, withIntermediateDirectories: true)
        try await Self.run(bottle.create(environment: environment), "making the bottle")
        try bottle.checkOwnBottle()
        try bottle.harden()
        try bottle.installFonts()
        if fm.fileExists(atPath: aside.path) {
            try bottle.checkInside(saves)
            try Self.merge(aside, into: saves)
        }
        try Data().write(to: bottle.readyMarker)
    }

    /// Moves everything in `source` into `target`, folder by folder, keeping what `target` already has where both hold
    /// the same file, and removes `source` once it's empty. Links are moved as links, never followed.
    static func merge(_ source: URL, into target: URL) throws {
        let fm = FileManager.default
        try fm.createDirectory(at: target, withIntermediateDirectories: true)
        for name in try fm.contentsOfDirectory(atPath: source.path) {
            let from = source.appendingPathComponent(name), to = target.appendingPathComponent(name)
            let fromIsFolder = (try? from.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])).map { $0.isDirectory == true && $0.isSymbolicLink != true } ?? false
            let toIsFolder = (try? to.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])).map { $0.isDirectory == true && $0.isSymbolicLink != true } ?? false
            if (try? fm.attributesOfItem(atPath: to.path)) == nil {
                try fm.moveItem(at: from, to: to)
            } else if fromIsFolder, toIsFolder {
                try merge(from, into: to)
            }
        }
        // A file in both places stays in `source`, which then isn't empty and isn't removed.
        if try fm.contentsOfDirectory(atPath: source.path).isEmpty { try fm.removeItem(at: source) }
    }

    /// Puts the game's PlayerPrefs from its file into the bottle: the file's values, and a deletion for each value the
    /// bottle holds under that key that the file lacks. Its subkeys and every other key stay as they are. A file holding
    /// any other key is not imported.
    private func importPrefs() async throws {
        guard let prefs, let data = try? Data(contentsOf: prefs.file) else { return }
        let file = try RegistryData(reg: data)
        guard file.keys.count == 1, let values = file.values(of: prefs.key) else {
            unityLog.error("not importing \(prefs.file.path, privacy: .public): it holds another key")
            return
        }
        let current = await bottleValues(prefs.key) ?? [:]
        let text = Self.importText(key: prefs.key, values: RegistryData.writable(values), removing: Set(current.keys))
        let staged = launch.dataDirectory.appendingPathComponent("PlayerPrefs-import.reg")
        try RegistryData.utf16File(text).write(to: staged, options: .atomic)
        defer { try? FileManager.default.removeItem(at: staged) }
        try await Self.run(bottle.importRegistry(staged, environment: environment), "applying the game's settings")
    }

    /// `key` with `values`, and `"name"=-` for each of `removing` that `values` lacks, compared without case.
    static func importText(key: String, values: [String: RegistryValue], removing: Set<String>) -> String {
        var data = RegistryData()
        data.setValues(values, of: key)
        var text = data.regText
        let kept = Set(values.keys.map { $0.lowercased() })
        // A name with a line break can't be written, so it can't be deleted either, and stays as the bottle has it.
        let gone = removing.filter { !kept.contains($0.lowercased()) && !RegistryData.breaksLine($0) }
            .sorted { Array($0.utf8).lexicographicallyPrecedes(Array($1.utf8)) }
        if !gone.isEmpty {
            text.removeLast()
            text += gone.map { "\n\"\(RegistryData.escape($0))\"=-" }.joined() + "\n"
        }
        return text
    }

    /// The values the bottle holds under `key`, not its subkeys'. Nil when the key isn't there or regedit fails.
    private func bottleValues(_ key: String) async -> [String: RegistryValue]? {
        let staged = launch.dataDirectory.appendingPathComponent("PlayerPrefs-export.reg")
        defer { try? FileManager.default.removeItem(at: staged) }
        try? FileManager.default.removeItem(at: staged)
        guard (try? await Self.run(bottle.exportRegistry(key, to: staged, environment: environment), "reading the game's settings")) != nil,
              let data = try? Data(contentsOf: staged) else { return nil }
        return (try? RegistryData(reg: data))?.values(of: key)
    }

    /// Reads the game's key back out of the bottle after it quits and keeps its own values, not its subkeys', in the
    /// file save sync backs up. Nothing changes when the game wrote none.
    private func exportPrefs() async throws {
        // regedit fails when the key doesn't exist, which is a game that never saved its PlayerPrefs.
        guard let prefs, let values = await bottleValues(prefs.key) else { return }
        var file = RegistryData()
        file.setValues(RegistryData.writable(values), of: prefs.key)
        try FileManager.default.createDirectory(at: prefs.file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(file.regText.utf8).write(to: prefs.file, options: .atomic)
    }

    /// macOS doesn't let a new app take the focus from VNDegen, so VNDegen hands it over once the game's window app is
    /// up, which takes Wine a few seconds.
    private func bringGameForward() async {
        for _ in 0..<120 {
            guard let game, game.isRunning, !ending else { return }
            if gameApp() != nil {
                showGame()
                return
            }
            try? await Task.sleep(for: .milliseconds(500))
        }
    }

    override func showGame() {
        guard let game, game.isRunning, let app = gameApp() else { return }
        NSApp.yieldActivation(to: app)
        app.activate()
    }

    /// The app Wine made for the game's window. Wine starts it through LaunchServices, so it isn't a child of the `wine`
    /// process VNDegen ran. It is the regular app whose `WINEPREFIX` is VNDegen's bottle, preferring the one named after
    /// the game's exe over Wine's own helpers such as explorer.exe.
    private func gameApp() -> NSRunningApplication? {
        let prefix = "WINEPREFIX=" + UnityBottle.resolved(bottle.path)
        let ours = NSWorkspace.shared.runningApplications.filter { app in
            app.activationPolicy == .regular && !app.isTerminated && Self.environment(of: app.processIdentifier).contains(prefix)
        }
        let name = exe.lastPathComponent.lowercased()
        return ours.first { ($0.localizedName ?? "").lowercased() == name } ?? ours.first
    }

    /// Closes the game as its close button does, then ends its process if it's still running after `quitTimeout`.
    /// Both reach this game's exe alone, so another Unity game the player started meanwhile keeps running.
    override func stopGame() {
        guard let game, game.isRunning else { return }
        let exeName = exe.lastPathComponent, bottle = bottle, environment = environment
        Task {
            try? await Self.run(bottle.close(exeName, environment: environment), "closing the game")
            try? await Task.sleep(for: Self.quitTimeout)
            guard game.isRunning else { return }
            unityLog.error("\(self.launch.game.id.rawValue, privacy: .public) didn't quit in time, ending it")
            try? await Self.run(bottle.close(exeName, force: true, environment: environment), "ending the game")
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

    /// The environment of a process of the same user, from `KERN_PROCARGS2`: the argument count, the executable path,
    /// the arguments, then the environment, each ending in a null.
    static func environment(of pid: pid_t) -> [String] {
        var name = [CTL_KERN, KERN_PROCARGS2, pid]
        var size = 0
        guard sysctl(&name, 3, nil, &size, nil, 0) == 0, size > 4 else { return [] }
        var buffer = [UInt8](repeating: 0, count: size)
        guard sysctl(&name, 3, &buffer, &size, nil, 0) == 0, size > 4 else { return [] }
        let argc = buffer.withUnsafeBytes { Int($0.loadUnaligned(as: Int32.self)) }
        let strings = buffer[4..<size].split(separator: 0, omittingEmptySubsequences: true).map { String(decoding: $0, as: UTF8.self) }
        // The executable path, then the arguments.
        return Array(strings.dropFirst(1 + argc))
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
        try Self.makeRunnable(app)
        let configuration = NSWorkspace.OpenConfiguration()
        configuration.createsNewApplicationInstance = true
        configuration.activates = true
        let running = try await NSWorkspace.shared.openApplication(at: app, configuration: configuration)
        self.running = running
        unityLog.info("opened \(self.app.lastPathComponent, privacy: .public), pid \(running.processIdentifier)")
        if ending {
            // The hub ended the session while the app was opening, before `stopGame` could reach it.
            stopGame()
        } else {
            host.setMessage("\(launch.game.title) is running in its own Mac version.", showsGame: true)
        }
        await Self.waitForExit(of: running)
        unityLog.info("\(self.app.lastPathComponent, privacy: .public) quit")
    }

    /// Returns once `app` has quit. The observer is in place before `isTerminated` is read, so a quit between the two
    /// isn't missed.
    private static func waitForExit(of app: NSRunningApplication) async {
        let pid = app.processIdentifier
        let center = NSWorkspace.shared.notificationCenter
        let waiter = ExitWaiter()
        let observer = center.addObserver(forName: NSWorkspace.didTerminateApplicationNotification, object: nil, queue: .main) { note in
            let ended = note.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication
            guard ended?.processIdentifier == pid else { return }
            MainActor.assumeIsolated { waiter.finish() }
        }
        await withCheckedContinuation { (done: CheckedContinuation<Void, Never>) in
            waiter.continuation = done
            if app.isTerminated || waiter.finished { waiter.finish() }
        }
        center.removeObserver(observer)
    }

    /// Resumes the wait once, whether the quit was seen before the wait began or after.
    private final class ExitWaiter {
        var continuation: CheckedContinuation<Void, Never>?
        var finished = false

        func finish() {
            finished = true
            continuation?.resume()
            continuation = nil
        }
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

    /// A Mac version unpacked from an archive made on Windows, such as SiNiSistar 2's zip, has lost its execute bits,
    /// and macOS won't open it. The app's main executable gets them back, as an unzip on a Mac would have kept them.
    /// The code signature doesn't cover file modes, so it stays valid.
    static func makeRunnable(_ app: URL) throws {
        let plist = app.appendingPathComponent("Contents/Info.plist")
        guard let info = NSDictionary(contentsOf: plist), let name = info["CFBundleExecutable"] as? String,
              !name.contains("/") else { return }
        let executable = app.appendingPathComponent("Contents/MacOS", isDirectory: true).appendingPathComponent(name)
        let fm = FileManager.default
        guard fm.fileExists(atPath: executable.path), !fm.isExecutableFile(atPath: executable.path) else { return }
        let mode = (try fm.attributesOfItem(atPath: executable.path)[.posixPermissions] as? NSNumber)?.int16Value ?? 0o644
        try fm.setAttributes([.posixPermissions: NSNumber(value: mode | 0o111)], ofItemAtPath: executable.path)
        unityLog.info("made \(executable.lastPathComponent, privacy: .public) executable")
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
