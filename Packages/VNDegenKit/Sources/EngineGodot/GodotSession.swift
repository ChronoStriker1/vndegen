import CGodot
import FontPack
import Foundation
import OSLog
import QuartzCore
import VNCore
#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

nonisolated let godotLog = Logger(subsystem: "com.chronostriker1.vndegen", category: "godot")

/// A Godot game running in VNDegen's own process and drawn in `view`, so the hub's menu and touch controls sit over it.
/// The engine runs one frame per display refresh on the main thread. The game starts once the view is on screen. The
/// game quitting itself ends the session through `onExit`. When the hub ends the session, the game gets the close
/// request its window's close button would send, and `waitUntilStopped()` returns once it has quit.
final class GodotSession: EngineSession {
    private enum State {
        case waiting
        case running
        /// The hub ended the session at `since` and the game is closing.
        case quitting(since: ContinuousClock.Instant)
        case stopped
    }

    let view: PlatformView
    private let host: GodotHostView
    private let engine: GodotEngine
    private let launch: GameLaunch
    private let paths: GodotPaths
    private let requirements: GodotRequirements
    private var state = State.waiting
    private var paused = false
    private var router = GodotInputRouter()
    private var displayLink: CADisplayLink?
    private var quitTimer: Timer?
    private var stopWaiters: [CheckedContinuation<Void, Never>] = []

    /// Longest a game gets to close after the hub ends it, such as one that asks the player to confirm.
    static let quitTimeout: Duration = .seconds(3)
    /// Whether a session's engine is running. Godot runs one game at a time.
    private(set) static var engineInUse = false
    /// The Godot majors whose engine failed to start a game in a way it can't recover from in this process.
    private(set) static var unusable: Set<Int> = []

    init(_ launch: GameLaunch, paths: GodotPaths, requirements: GodotRequirements, engine: GodotEngine) throws {
        self.launch = launch
        self.engine = engine
        self.paths = paths
        self.requirements = requirements
        try paths.prepare()
        host = GodotHostView()
        view = host
        host.onReady = { [weak self] in self?.start() }
        host.onResize = { [weak self] size, scale in
            guard let self, case .running = state else { return }
            engine.resize(Float(size.width), Float(size.height), Float(scale))
        }
        // The hub's menu keeps the keyboard on this view while it is open, so keys go nowhere while paused. A key held
        // through the pause was released by setPaused, and its own release is then ignored.
        host.onKey = { [weak self] usage, pressed, text in
            guard let self, case .running = state, !paused else { return }
            router.hostKey(usage, pressed: pressed, text: text).forEach(deliver)
        }
        host.onEvent = { [weak self] event in self?.send(event) }
    }

    private func start() {
        guard case .waiting = state else { return }
        #if DEBUG
        // Test hooks, as launch arguments, since a simulator's app can't be given environment variables: frame timing
        // for the frame rate checks, frames written to a file, and an input trace. Relative paths go in the game's logs.
        for (argument, variable) in [("godotFrameLog", "VNGD_FRAME_LOG"), ("godotSnapshot", "VNGD_SNAPSHOT"), ("godotTraceInput", "VNGD_TRACE_INPUT")] {
            guard let value = UserDefaults.standard.string(forKey: argument) else { continue }
            let path = value.hasPrefix("/") || variable == "VNGD_TRACE_INPUT" ? value : paths.log.deletingLastPathComponent().appendingPathComponent(value).path
            setenv(variable, path, 1)
        }
        #endif
        let strings = [paths.pack.path, paths.executable.path, paths.appData.path, paths.log.path].map { strdup($0) }
        defer { strings.forEach { free($0) } }
        let rendering = paths.renderingMethod.flatMap { strdup($0) }
        defer { free(rendering) }
        let size = host.bounds.size
        // The Windows font substitutes, for games that ask for a font by name.
        let fontTable = FontPack.table.flatMap { strdup($0.path) }
        defer { free(fontTable) }
        let folderPaths = FontPack.folders.map { strdup($0.path) }
        let fontFolders = UnsafeMutablePointer<UnsafePointer<CChar>?>.allocate(capacity: max(folderPaths.count, 1))
        fontFolders.initialize(from: folderPaths.map { $0.map { UnsafePointer($0) } }, count: folderPaths.count)
        defer {
            fontFolders.deallocate()
            folderPaths.forEach { free($0) }
        }
        Self.engineInUse = true
        let result = (requirements.encryptionKey ?? Data()).withUnsafeBytes { key in
            var settings = VNGDLaunch(pack: strings[0], executable: strings[1], dataDir: strings[2], logPath: strings[3],
                                      renderingMethod: rendering, layer: Unmanaged.passUnretained(host.metalLayer).toOpaque(),
                                      width: Float(size.width), height: Float(size.height), scale: Float(host.scale),
                                      keyboard: nil, context: nil, fontTable: fontTable, fontFolders: fontFolders,
                                      fontFolderCount: Int32(folderPaths.count), steam: requirements.steam,
                                      epicOnlineServices: requirements.epicOnlineServices,
                                      encryptionKey: requirements.encryptionKey == nil ? nil : key.bindMemory(to: UInt8.self).baseAddress)
            return engine.start(&settings)
        }
        guard result == 0 else {
            if result == 2 { Self.unusable.insert(paths.major) }
            godotLog.error("\(self.launch.game.id.rawValue, privacy: .public) did not start: \(self.engine.lastProblem ?? "", privacy: .public)")
            engine.stop()
            Self.engineInUse = false
            stopped()
            launch.onExit()
            return
        }
        godotLog.info("started \(self.launch.game.id.rawValue, privacy: .public)")
        state = .running
        if paused { engine.setPaused(true) }
        host.takeFocus()
        let target = FrameTarget(self)
        #if canImport(UIKit)
        let link = CADisplayLink(target: target, selector: #selector(FrameTarget.step))
        #else
        let link = host.displayLink(target: target, selector: #selector(FrameTarget.step))
        #endif
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    fileprivate func step() {
        switch state {
        case .running:
            if engine.iterate() != 0 { finish() }
        case let .quitting(since):
            if engine.iterate() != 0 || ContinuousClock.now > since + Self.quitTimeout { finish() }
        case .waiting, .stopped:
            break
        }
    }

    private func finish() {
        let quitByGame = if case .running = state { true } else { false }
        displayLink?.invalidate()
        displayLink = nil
        quitTimer?.invalidate()
        quitTimer = nil
        engine.stop()
        Self.engineInUse = false
        godotLog.info("\(self.launch.game.id.rawValue, privacy: .public) stopped\(quitByGame ? " by the game" : "", privacy: .public)")
        stopped()
        if quitByGame { launch.onExit() }
    }

    private func stopped() {
        state = .stopped
        let waiters = stopWaiters
        stopWaiters = []
        waiters.forEach { $0.resume() }
    }

    private func deliver(_ input: GodotInput) {
        engine.send(input)
    }

    // MARK: EngineSession

    func send(_ event: ControllerEvent) {
        guard case .running = state else { return }
        router.translate(event).forEach(deliver)
    }

    func type(_ typed: TypedText) {
        guard case .running = state else { return }
        router.type(typed).forEach(deliver)
    }

    func setPaused(_ paused: Bool) {
        self.paused = paused
        guard case .running = state else { return }
        if paused { router.releaseAll().forEach(deliver) }
        engine.setPaused(paused)
        if !paused { host.takeFocus() }
    }

    func terminate() {
        switch state {
        case .waiting:
            stopped()
        case .running:
            if paused { engine.setPaused(false) }
            engine.requestQuit()
            state = .quitting(since: .now)
            // The hub takes the view off screen, which would stop a display link tied to it.
            displayLink?.invalidate()
            displayLink = nil
            let timer = Timer(timeInterval: 1.0 / 60, repeats: true) { _ in
                MainActor.assumeIsolated { self.step() }
            }
            RunLoop.main.add(timer, forMode: .common)
            quitTimer = timer
        case .quitting, .stopped:
            break
        }
    }

    func waitUntilStopped() async {
        if case .stopped = state { return }
        await withCheckedContinuation { stopWaiters.append($0) }
    }
}

/// A display link keeps its target, so the session sits behind a weak link to avoid a cycle.
private final class FrameTarget: NSObject {
    private weak var session: GodotSession?

    init(_ session: GodotSession) {
        self.session = session
    }

    @objc func step() { session?.step() }
}

/// The files a launch hands the engine. The game's data directory stands in for the Windows AppData folder, where
/// Godot keeps `user://`, and holds the engine's log.
nonisolated struct GodotPaths {
    /// The Godot major that runs the game.
    var major: Int
    var pack: URL
    var executable: URL
    var appData: URL
    var log: URL
    /// The renderer to use instead of the game's. Godot 4 runs every game on its mobile renderer on Metal, since
    /// VNDegen's Godot 4 has no OpenGL. The iOS Simulator is the exception: its Metal is too limited for that renderer,
    /// so there every game runs on the compatibility renderer, through MetalANGLE.
    var renderingMethod: String?

    init(folder: URL, dataDirectory: URL, layout: GodotLayout) {
        major = layout.major
        pack = folder.appendingPathComponent(layout.pack)
        executable = folder.appendingPathComponent(layout.executable)
        appData = GodotPlugin.appDataDirectory(dataDirectory)
        log = dataDirectory.appendingPathComponent("logs/godot.txt")
        #if targetEnvironment(simulator)
        renderingMethod = layout.major >= 4 ? "gl_compatibility" : nil
        #else
        renderingMethod = layout.major >= 4 ? "mobile" : nil
        #endif
    }

    func prepare() throws {
        let fileManager = FileManager.default
        for folder in [log.deletingLastPathComponent(), appData] {
            try fileManager.createDirectory(at: folder, withIntermediateDirectories: true)
        }
        try Data().write(to: log)
    }
}
